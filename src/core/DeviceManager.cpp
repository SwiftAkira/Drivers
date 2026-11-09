#include "DeviceManager.h"

#include "DeviceBase.h"
#include "devices/lamzu/LamzuMouse.h"
#include "devices/mad/MadKeyboard.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJSEngine>
#include <QLoggingCategory>

#include <libudev.h>
#include <unistd.h>

#include <algorithm>

Q_LOGGING_CATEGORY(lcDevices, "flux.devices")

namespace flux {

DeviceManager::DeviceManager(QObject *parent)
    : QObject(parent)
{
    qCInfo(lcDevices) << "DeviceManager created" << this;
    s_instance = this;
    HidDevice::initLibrary();

    m_matches = {
        LamzuMouse::match(),
        MadKeyboard::match(),
    };

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(350);
    connect(&m_debounce, &QTimer::timeout, this, &DeviceManager::rescan);

    // Belt and braces in case udev events are missed (e.g. inside sandboxes).
    m_fallbackPoll.setInterval(4000);
    connect(&m_fallbackPoll, &QTimer::timeout, this, &DeviceManager::rescan);
    m_fallbackPoll.start();

    setupUdev();
    QTimer::singleShot(0, this, &DeviceManager::rescan);
}

DeviceManager::~DeviceManager()
{
    for (auto &d : m_live)
        delete d.data();
    m_live.clear();
    if (m_monitor)
        udev_monitor_unref(m_monitor);
    if (m_udev)
        udev_unref(m_udev);
    if (s_instance == this)
        s_instance = nullptr;
}

DeviceManager *DeviceManager::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void DeviceManager::setupUdev()
{
    m_udev = udev_new();
    if (!m_udev)
        return;
    m_monitor = udev_monitor_new_from_netlink(m_udev, "udev");
    if (!m_monitor)
        return;
    udev_monitor_filter_add_match_subsystem_devtype(m_monitor, "hidraw", nullptr);
    if (udev_monitor_enable_receiving(m_monitor) < 0)
        return;
    m_notifier = new QSocketNotifier(udev_monitor_get_fd(m_monitor), QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &DeviceManager::onUdevEvent);
}

void DeviceManager::onUdevEvent()
{
    while (udev_device *dev = udev_monitor_receive_device(m_monitor))
        udev_device_unref(dev);
    m_debounce.start();
}

QObject *DeviceManager::mouse() const
{
    for (auto &d : m_live)
        if (d && d->kind() == QLatin1String("mouse"))
            return d.data();
    return nullptr;
}

QObject *DeviceManager::keyboard() const
{
    for (auto &d : m_live)
        if (d && d->kind() == QLatin1String("keyboard"))
            return d.data();
    return nullptr;
}

QList<QObject *> DeviceManager::devices() const
{
    QList<QObject *> out;
    for (auto &d : m_live)
        if (d)
            out << d.data();
    std::sort(out.begin(), out.end(), [](QObject *a, QObject *b) {
        return static_cast<DeviceBase *>(a)->kind() > static_cast<DeviceBase *>(b)->kind(); // mouse first
    });
    return out;
}

QString DeviceManager::udevInstallCommand() const
{
    // Prefer the packaged copy; fall back to the source tree when running from a build dir.
    QString rules = QStringLiteral("/usr/share/flux/70-flux.rules");
    const QString dev = QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../packaging/70-flux.rules"));
    if (!QFile::exists(rules) && QFile::exists(dev))
        rules = dev;
    return QStringLiteral("sudo install -Dm644 %1 /etc/udev/rules.d/70-flux.rules && "
                          "sudo udevadm control --reload && sudo udevadm trigger --subsystem-match=hidraw").arg(rules);
}

void DeviceManager::enableDemo()
{
    m_demoMode = true;
    m_fallbackPoll.stop();
    for (const auto &m : m_matches) {
        HidInfo info;
        info.path = "demo-" + m.kind.toStdString();
        info.vendorId = m.vendorId;
        info.productId = m.productIds.front();
        DeviceBase *dev = m.create(info, this);
        dev->setDemo(true);
        m_live.insert(QString::fromStdString(info.path), dev);
        dev->start();
    }
    emit devicesChanged();
}

void DeviceManager::rescan()
{
    if (m_demoMode)
        return;
    const auto all = HidDevice::enumerate();
    QHash<QString, std::pair<const DeviceMatch *, HidInfo>> wanted;
    QStringList blocked;

    for (const auto &info : all) {
        for (const auto &m : m_matches) {
            if (info.vendorId != m.vendorId)
                continue;
            if (std::find(m.productIds.begin(), m.productIds.end(), info.productId) == m.productIds.end())
                continue;
            if (info.usagePage != m.usagePage || info.usage != m.usage)
                continue;
            // Only one device per kind; the first (lowest PID priority) wins.
            bool kindTaken = false;
            for (auto it = wanted.cbegin(); it != wanted.cend(); ++it)
                kindTaken |= it.value().first->kind == m.kind;
            if (kindTaken)
                continue;
            if (::access(info.path.c_str(), R_OK | W_OK) != 0) {
                if (!blocked.contains(m.model))
                    blocked << m.model;
                continue;
            }
            const QString key = QStringLiteral("%1:%2:%3")
                                    .arg(info.vendorId, 4, 16, QLatin1Char('0'))
                                    .arg(info.productId, 4, 16, QLatin1Char('0'))
                                    .arg(QString::fromStdString(info.path));
            wanted.insert(key, {&m, info});
        }
    }

    bool changed = false;

    // Remove devices that disappeared.
    for (auto it = m_live.begin(); it != m_live.end();) {
        if (!it.value() || !wanted.contains(it.key())) {
            const QString kind = it.value() ? it.value()->kind() : QString();
            qCInfo(lcDevices) << "device removed" << it.key();
            if (it.value()) {
                emit it.value()->disconnected();
                it.value()->deleteLater();
            }
            it = m_live.erase(it);
            changed = true;
            emit deviceRemoved(kind);
        } else {
            ++it;
        }
    }

    // Add new ones.
    for (auto it = wanted.cbegin(); it != wanted.cend(); ++it) {
        if (m_live.contains(it.key()))
            continue;
        const auto &[match, info] = it.value();
        qCInfo(lcDevices) << "device added" << it.key() << QString::fromStdString(info.product);
        DeviceBase *dev = match->create(info, this);
        if (!dev)
            continue;
        m_live.insert(it.key(), dev);
        dev->start();
        changed = true;
        emit deviceAdded(dev);
    }

    if (changed)
        emit devicesChanged();
    if (blocked != m_blocked) {
        m_blocked = blocked;
        emit blockedChanged();
    }
}

} // namespace flux
