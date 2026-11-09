#pragma once

#include <QObject>
#include <QString>

#include <memory>

namespace flux {

// Emits idled() after `timeout` ms without keyboard/mouse input, and resumed()
// on the next input after that.
//
// On Wayland this uses ext-idle-notify v2 "input idle", which ignores idle
// inhibitors (video players, keep-awake locks): those keep the screen on but
// shouldn't keep the keyboard lit forever. Elsewhere it falls back to
// KIdleTime, which does honour inhibitors.
class InputIdle : public QObject {
    Q_OBJECT
public:
    explicit InputIdle(QObject *parent);
    ~InputIdle() override;

    void setTimeout(int ms);   // <= 0 turns it off
    QString backend() const { return m_backend; }

signals:
    void idled();
    void resumed();

private:
    void rearm();
    void dropWatch();

    struct Wayland;
    std::unique_ptr<Wayland> m_wl;
    int m_timeoutMs = 0;
    int m_kidleId = -1;
    bool m_kidleHooked = false;
    bool m_kidleIdle = false;
    QString m_backend = QStringLiteral("none");
};

} // namespace flux
