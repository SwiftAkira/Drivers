#pragma once

#include "HidDevice.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSocketNotifier>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <functional>

struct udev;
struct udev_monitor;
class QQmlEngine;
class QJSEngine;

namespace flux {

class DeviceBase;

struct DeviceMatch {
    QString kind;     // "mouse" | "keyboard"
    QString model;    // human readable, used when access is denied
    uint16_t vendorId;
    std::vector<uint16_t> productIds;
    uint16_t usagePage;
    uint16_t usage;
    std::function<DeviceBase *(const HidInfo &, QObject *parent)> create;
};

// Watches hidraw hot-plug events and keeps one live object per supported
// device. Exposed to QML as the `Devices` singleton.
class DeviceManager : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Devices)
    QML_SINGLETON

    Q_PROPERTY(QObject *mouse READ mouse NOTIFY devicesChanged)
    Q_PROPERTY(QObject *keyboard READ keyboard NOTIFY devicesChanged)
    Q_PROPERTY(QList<QObject *> devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(QStringList blocked READ blocked NOTIFY blockedChanged)
    Q_PROPERTY(QString udevInstallCommand READ udevInstallCommand CONSTANT)

public:
    explicit DeviceManager(QObject *parent); // no default: QML must use create()
    ~DeviceManager() override;

    static DeviceManager *create(QQmlEngine *, QJSEngine *);
    static DeviceManager *instance() { return s_instance; }

    QObject *mouse() const;
    QObject *keyboard() const;
    QList<QObject *> devices() const;
    QStringList blocked() const { return m_blocked; }
    QString udevInstallCommand() const;

    Q_INVOKABLE void rescan();
    void enableDemo();

signals:
    void devicesChanged();
    void blockedChanged();
    void deviceAdded(QObject *device);
    void deviceRemoved(const QString &kind);

private:
    void setupUdev();
    void onUdevEvent();

    std::vector<DeviceMatch> m_matches;
    QHash<QString, QPointer<DeviceBase>> m_live; // key -> device
    QStringList m_blocked;
    bool m_demoMode = false;
    QTimer m_debounce;
    QTimer m_fallbackPoll;
    struct udev *m_udev = nullptr;
    struct udev_monitor *m_monitor = nullptr;
    QSocketNotifier *m_notifier = nullptr;

    static inline DeviceManager *s_instance = nullptr;
};

} // namespace flux
