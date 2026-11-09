#include "AppController.h"

#include "core/DeviceBase.h"
#include "core/DeviceManager.h"

#include <KStatusNotifierItem>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QJSEngine>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QStandardPaths>
#include <QWindow>

namespace flux {

namespace {

QString autostartPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/autostart/dev.flux.Flux.desktop");
}

// Tray glyph: a mouse outline whose body fills up with the battery level.
QIcon trayIcon(int level, bool charging)
{
    QIcon icon;
    for (int s : {16, 22, 32, 44, 64}) {
        QPixmap pm(s, s);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal w = s * 0.50, h = s * 0.82;
        const QRectF body((s - w) / 2, (s - h) / 2, w, h);
        const qreal pen = std::max(1.2, s / 14.0);
        QPainterPath shape;
        shape.addRoundedRect(body, w / 2, w / 2);

        if (level >= 0) {
            QColor fill = charging ? QColor("#30d158") : level <= 10 ? QColor("#ff453a") : level <= 20 ? QColor("#ff9f0a") : QColor(255, 255, 255, 200);
            const qreal fh = body.height() * std::clamp(level, 0, 100) / 100.0;
            p.save();
            p.setClipPath(shape);
            p.fillRect(QRectF(body.left(), body.bottom() - fh, body.width(), fh), fill);
            p.restore();
        }
        p.setPen(QPen(Qt::white, pen));
        p.setBrush(Qt::NoBrush);
        p.drawPath(shape);
        p.drawLine(QPointF(s / 2.0, body.top() + h * 0.14), QPointF(s / 2.0, body.top() + h * 0.30));
        icon.addPixmap(pm);
    }
    return icon;
}

} // namespace

AppController::AppController(DeviceManager *devices, QObject *parent)
    : QObject(parent)
    , m_devices(devices)
    , m_settings(QStringLiteral("flux"), flux::settingsApp())
{
    s_instance = this;
    setupTray();
    connect(m_devices, &DeviceManager::devicesChanged, this, [this] {
        watchMouse();
        if (QObject *kb = m_devices->keyboard())
            connect(kb, SIGNAL(osuChanged()), this, SLOT(onBatteryChanged()), Qt::UniqueConnection);
        updateTray();
    });
    watchMouse();
}

AppController::~AppController()
{
    if (s_instance == this)
        s_instance = nullptr;
}

AppController *AppController::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

bool AppController::closeToTray() const { return m_settings.value("closeToTray", true).toBool(); }
void AppController::setCloseToTray(bool v) { m_settings.setValue("closeToTray", v); emit settingsChanged(); }
bool AppController::batteryAlerts() const { return m_settings.value("batteryAlerts", true).toBool(); }
void AppController::setBatteryAlerts(bool v) { m_settings.setValue("batteryAlerts", v); emit settingsChanged(); }
int AppController::lowBatteryThreshold() const { return m_settings.value("lowBatteryThreshold", 20).toInt(); }
void AppController::setLowBatteryThreshold(int v) { m_settings.setValue("lowBatteryThreshold", v); m_lastAlertLevel = 101; emit settingsChanged(); }
bool AppController::reduceMotion() const { return m_settings.value("reduceMotion", false).toBool(); }
void AppController::setReduceMotion(bool v) { m_settings.setValue("reduceMotion", v); emit settingsChanged(); }
QString AppController::lastPage() const { return m_settings.value("lastPage").toString(); }
void AppController::setLastPage(const QString &p) { m_settings.setValue("lastPage", p); }
QString AppController::version() const { return QCoreApplication::applicationVersion(); }

bool AppController::launchAtLogin() const
{
    return QFile::exists(autostartPath());
}

void AppController::setLaunchAtLogin(bool v)
{
    const QString path = autostartPath();
    if (v) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write("[Desktop Entry]\n"
                    "Type=Application\n"
                    "Name=Flux\n"
                    "Comment=Mouse and keyboard settings\n"
                    "Exec=flux --hidden\n"
                    "Icon=dev.flux.Flux\n"
                    "X-KDE-autostart-phase=2\n"
                    "X-GNOME-Autostart-enabled=true\n");
        }
    } else {
        QFile::remove(path);
    }
    emit settingsChanged();
}

void AppController::setMainWindow(QWindow *w)
{
    m_window = w;
}

void AppController::showWindow()
{
    emit showRequested();
    if (!m_window)
        return;
    m_window->show();
    m_window->raise();
    m_window->requestActivate();
}

void AppController::hideWindow()
{
    if (m_window)
        m_window->hide();
}

void AppController::quit()
{
    QCoreApplication::quit();
}

void AppController::copyToClipboard(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

void AppController::notify(const QString &title, const QString &body, const QString &icon)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.Notifications"),
                                                      QStringLiteral("/org/freedesktop/Notifications"),
                                                      QStringLiteral("org.freedesktop.Notifications"),
                                                      QStringLiteral("Notify"));
    QVariantMap hints;
    hints.insert(QStringLiteral("desktop-entry"), QStringLiteral("dev.flux.Flux"));
    msg << QStringLiteral("Flux") << uint(0) << (icon.isEmpty() ? QStringLiteral("dev.flux.Flux") : icon) << title << body
        << QStringList() << hints << int(6000);
    QDBusConnection::sessionBus().asyncCall(msg);
}

void AppController::setupTray()
{
    m_tray = new KStatusNotifierItem(QStringLiteral("dev.flux.Flux"), this);
    m_tray->setTitle(QStringLiteral("Flux"));
    m_tray->setCategory(KStatusNotifierItem::Hardware);
    m_tray->setStatus(KStatusNotifierItem::Active);
    m_tray->setStandardActionsEnabled(false);
    m_tray->setIconByPixmap(trayIcon(-1, false));

    m_menu = new QMenu();
    m_batteryAction = m_menu->addAction(QStringLiteral("Mouse not connected"));
    m_batteryAction->setEnabled(false);
    m_kbAction = m_menu->addAction(QStringLiteral("Keyboard not connected"));
    m_kbAction->setEnabled(false);
    m_osuAction = m_menu->addAction(QStringLiteral("osu! mode"));
    m_osuAction->setCheckable(true);
    connect(m_osuAction, &QAction::triggered, this, [this](bool on) {
        if (QObject *kb = m_devices->keyboard())
            kb->setProperty("osuMode", on);
    });
    m_menu->addSeparator();
    m_menu->addAction(QIcon::fromTheme(QStringLiteral("window")), QStringLiteral("Open Flux"), this, &AppController::showWindow);
    m_menu->addSeparator();
    m_menu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")), QStringLiteral("Quit Flux"), this, &AppController::quit);
    m_tray->setContextMenu(m_menu);

    connect(m_tray, &KStatusNotifierItem::activateRequested, this, [this](bool, const QPoint &) {
        if (m_window && m_window->isVisible() && m_window->isActive())
            hideWindow();
        else
            showWindow();
    });
    updateTray();
}

void AppController::watchMouse()
{
    QObject *mouse = m_devices->mouse();
    if (mouse == m_mouse)
        return;
    if (m_mouse)
        disconnect(m_mouse, nullptr, this, nullptr);
    m_mouse = mouse;
    m_lastAlertLevel = 101;
    if (m_mouse) {
        connect(m_mouse, SIGNAL(batteryChanged()), this, SLOT(onBatteryChanged()));
        connect(m_mouse, SIGNAL(onlineChanged()), this, SLOT(onBatteryChanged()));
        connect(m_mouse, SIGNAL(infoChanged()), this, SLOT(onBatteryChanged()));
    }
}

void AppController::onBatteryChanged()
{
    updateTray();
    if (!m_mouse || !batteryAlerts())
        return;
    const bool online = m_mouse->property("online").toBool();
    const int level = m_mouse->property("batteryLevel").toInt();
    const bool charging = m_mouse->property("charging").toBool();
    if (!online || level < 0)
        return;
    if (charging) {
        m_lastAlertLevel = 101;
        return;
    }
    const int threshold = lowBatteryThreshold();
    // Alert once when crossing the threshold, then at 10% and 5%.
    for (int mark : {threshold, 10, 5}) {
        if (level <= mark && m_lastAlertLevel > mark) {
            m_lastAlertLevel = level;
            const QString name = m_mouse->property("displayName").toString();
            notify(QStringLiteral("%1 battery low").arg(name),
                   QStringLiteral("%1% remaining. Plug in the cable to charge.").arg(level),
                   QStringLiteral("battery-low"));
            break;
        }
    }
    if (level > threshold + 5)
        m_lastAlertLevel = 101;
}

void AppController::updateTray()
{
    if (!m_tray)
        return;
    QString tip;
    if (m_mouse && m_mouse->property("online").toBool()) {
        const int level = m_mouse->property("batteryLevel").toInt();
        const bool charging = m_mouse->property("charging").toBool();
        const QString name = m_mouse->property("displayName").toString();
        m_tray->setIconByPixmap(trayIcon(level, charging));
        const QString line = level >= 0 ? QStringLiteral("%1 — %2%%3").arg(name).arg(level).arg(charging ? QStringLiteral(" (charging)") : QString())
                                        : name;
        m_batteryAction->setText(line);
        tip = line;
    } else if (m_mouse) {
        m_tray->setIconByPixmap(trayIcon(-1, false));
        m_batteryAction->setText(QStringLiteral("Mouse asleep"));
        tip = QStringLiteral("Mouse asleep");
    } else {
        m_tray->setIconByPixmap(trayIcon(-1, false));
        m_batteryAction->setText(QStringLiteral("Mouse not connected"));
        tip = QStringLiteral("Mouse not connected");
    }
    if (QObject *kb = m_devices->keyboard()) {
        m_kbAction->setText(kb->property("displayName").toString());
        m_osuAction->setEnabled(true);
        m_osuAction->setChecked(kb->property("osuMode").toBool());
        m_osuAction->setText(QStringLiteral("osu! mode (%1)").arg(kb->property("osuKeysText").toString()));
    } else {
        m_kbAction->setText(QStringLiteral("Keyboard not connected"));
        m_osuAction->setEnabled(false);
        m_osuAction->setChecked(false);
    }
    m_tray->setToolTip(QStringLiteral("dev.flux.Flux"), QStringLiteral("Flux"), tip);
}

} // namespace flux
