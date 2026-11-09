#include "InputIdle.h"

#include "qwayland-ext-idle-notify-v1.h"

#include <KIdleTime>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QtGui/qguiapplication_platform.h>
#include <QtWaylandClient/QWaylandClientExtensionTemplate>

#include <functional>

Q_LOGGING_CATEGORY(lcInputIdle, "flux.idle")

namespace flux {

namespace {

class Notifier : public QWaylandClientExtensionTemplate<Notifier>, public QtWayland::ext_idle_notifier_v1 {
public:
    Notifier() : QWaylandClientExtensionTemplate<Notifier>(2) { initialize(); }
    ~Notifier() override
    {
        if (isActive())
            destroy();
    }
    uint32_t boundVersion()
    {
        auto *obj = QtWayland::ext_idle_notifier_v1::object();
        return obj ? wl_proxy_get_version(reinterpret_cast<wl_proxy *>(obj)) : 0;
    }
};

class Notification : public QtWayland::ext_idle_notification_v1 {
public:
    Notification(::ext_idle_notification_v1 *obj, std::function<void()> idled, std::function<void()> resumed)
        : QtWayland::ext_idle_notification_v1(obj), m_idled(std::move(idled)), m_resumed(std::move(resumed))
    {
    }
    ~Notification() override { destroy(); }

protected:
    void ext_idle_notification_v1_idled() override { m_idled(); }
    void ext_idle_notification_v1_resumed() override { m_resumed(); }

private:
    std::function<void()> m_idled, m_resumed;
};

wl_seat *waylandSeat()
{
    if (auto *app = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>() : nullptr)
        return app->seat();
    return nullptr;
}

} // namespace

struct InputIdle::Wayland {
    std::unique_ptr<Notifier> notifier;
    std::unique_ptr<Notification> note;
};

InputIdle::InputIdle(QObject *parent)
    : QObject(parent)
{
    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        m_wl = std::make_unique<Wayland>();
        m_wl->notifier = std::make_unique<Notifier>();
        // The global can show up after we're created; switch over when it does.
        connect(m_wl->notifier.get(), &QWaylandClientExtension::activeChanged, this, [this] {
            if (m_timeoutMs > 0)
                rearm();
        });
    }
}

InputIdle::~InputIdle()
{
    if (m_kidleId >= 0 && !QCoreApplication::closingDown())
        KIdleTime::instance()->removeIdleTimeout(m_kidleId);
}

void InputIdle::setTimeout(int ms)
{
    m_timeoutMs = std::max(0, ms);
    rearm();
}

void InputIdle::dropWatch()
{
    if (m_wl)
        m_wl->note.reset();
    if (m_kidleId >= 0) {
        KIdleTime::instance()->removeIdleTimeout(m_kidleId);
        m_kidleId = -1;
    }
    m_kidleIdle = false;
    m_backend = QStringLiteral("none");
}

void InputIdle::rearm()
{
    dropWatch();
    if (m_timeoutMs <= 0)
        return;

    if (m_wl && m_wl->notifier->isActive() && m_wl->notifier->boundVersion() >= 2) {
        if (wl_seat *seat = waylandSeat()) {
            m_wl->note = std::make_unique<Notification>(
                m_wl->notifier->get_input_idle_notification(uint32_t(m_timeoutMs), seat),
                [this] { emit idled(); },
                [this] { emit resumed(); });
            m_backend = QStringLiteral("wayland input-idle");
            qCInfo(lcInputIdle) << "Watching input idle via ext-idle-notify v2," << m_timeoutMs / 1000 << "s";
            return;
        }
    }

    // Fallback: KIdleTime (X11, or a compositor without input-idle support).
    KIdleTime *k = KIdleTime::instance();
    if (!m_kidleHooked) {
        m_kidleHooked = true;
        connect(k, &KIdleTime::timeoutReached, this, [this, k](int id, int) {
            if (id != m_kidleId)
                return;
            m_kidleIdle = true;
            k->catchNextResumeEvent();
            emit idled();
        });
        connect(k, &KIdleTime::resumingFromIdle, this, [this] {
            if (!m_kidleIdle)
                return;
            m_kidleIdle = false;
            emit resumed();
        });
    }
    m_kidleId = k->addIdleTimeout(m_timeoutMs);
    m_backend = QStringLiteral("KIdleTime");
    qCInfo(lcInputIdle) << "Watching idle via KIdleTime (idle inhibitors apply)," << m_timeoutMs / 1000 << "s";
}

} // namespace flux
