#pragma once

#include <QObject>
#include <QPointer>
#include <QSettings>
#include <QtQml/qqmlregistration.h>

class KStatusNotifierItem;
class QMenu;
class QAction;
class QQmlEngine;
class QJSEngine;
class QWindow;

namespace flux {

class DeviceManager;

// App-wide state exposed to QML as the `App` singleton: preferences, the
// tray icon, battery notifications and window show/hide.
class AppController : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(App)
    QML_SINGLETON

    Q_PROPERTY(bool closeToTray READ closeToTray WRITE setCloseToTray NOTIFY settingsChanged)
    Q_PROPERTY(bool launchAtLogin READ launchAtLogin WRITE setLaunchAtLogin NOTIFY settingsChanged)
    Q_PROPERTY(bool batteryAlerts READ batteryAlerts WRITE setBatteryAlerts NOTIFY settingsChanged)
    Q_PROPERTY(int lowBatteryThreshold READ lowBatteryThreshold WRITE setLowBatteryThreshold NOTIFY settingsChanged)
    Q_PROPERTY(bool reduceMotion READ reduceMotion WRITE setReduceMotion NOTIFY settingsChanged)
    Q_PROPERTY(QString lastPage READ lastPage WRITE setLastPage NOTIFY settingsChanged)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    AppController(DeviceManager *devices, QObject *parent = nullptr);
    ~AppController() override;

    static AppController *create(QQmlEngine *, QJSEngine *);
    static AppController *instance() { return s_instance; }

    bool closeToTray() const;
    void setCloseToTray(bool v);
    bool launchAtLogin() const;
    void setLaunchAtLogin(bool v);
    bool batteryAlerts() const;
    void setBatteryAlerts(bool v);
    int lowBatteryThreshold() const;
    void setLowBatteryThreshold(int v);
    bool reduceMotion() const;
    void setReduceMotion(bool v);
    QString lastPage() const;
    void setLastPage(const QString &p);
    QString version() const;

    void setMainWindow(QWindow *w);

    Q_INVOKABLE void showWindow();
    Q_INVOKABLE void hideWindow();
    Q_INVOKABLE void quit();
    Q_INVOKABLE void notify(const QString &title, const QString &body, const QString &icon = QString());
    Q_INVOKABLE void copyToClipboard(const QString &text);

signals:
    void settingsChanged();
    void showRequested();

private slots:
    void onBatteryChanged();

private:
    void setupTray();
    void updateTray();
    void watchMouse();

    DeviceManager *m_devices;
    mutable QSettings m_settings;
    KStatusNotifierItem *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_batteryAction = nullptr;
    QAction *m_kbAction = nullptr;
    QAction *m_osuAction = nullptr;
    QPointer<QObject> m_mouse;
    QPointer<QWindow> m_window;
    int m_lastAlertLevel = 101;

    static inline AppController *s_instance = nullptr;
};

} // namespace flux
