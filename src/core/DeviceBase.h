#pragma once

#include "IoWorker.h"

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <functional>

namespace flux {

// Demo / screenshot runs keep their settings in ~/.config/flux/flux-demo.conf
// so they never leak into the real app.
inline bool g_demoSettings = false;
inline QString settingsApp() { return g_demoSettings ? QStringLiteral("flux-demo") : QStringLiteral("flux"); }

// Common QML-facing surface for every supported device. Subclasses own a
// protocol object + HidDevice that are only touched from m_io's thread.
class DeviceBase : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Devices are created by DeviceManager")

    Q_PROPERTY(QString kind READ kind CONSTANT)
    Q_PROPERTY(QString key READ key CONSTANT)
    Q_PROPERTY(QString displayName READ displayName NOTIFY infoChanged)
    Q_PROPERTY(QString connection READ connection NOTIFY infoChanged)
    Q_PROPERTY(QString firmware READ firmware NOTIFY infoChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(bool online READ online NOTIFY onlineChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    DeviceBase(QString kind, QString key, QObject *parent = nullptr);
    ~DeviceBase() override;

    QString kind() const { return m_kind; }
    QString key() const { return m_key; }
    QString displayName() const { return m_displayName; }
    QString connection() const { return m_connection; }
    QString firmware() const { return m_firmware; }
    bool ready() const { return m_ready; }
    bool online() const { return m_online; }
    bool busy() const { return m_busyCount > 0; }
    QString lastError() const { return m_lastError; }

    // Opens the device and loads its state asynchronously.
    virtual void start() = 0;
    // Re-read everything from the device.
    Q_INVOKABLE virtual void refresh() = 0;

    // Profile snapshot used by ProfileStore (JSON-compatible).
    Q_INVOKABLE virtual QVariantMap exportSettings() const = 0;
    Q_INVOKABLE virtual void importSettings(const QVariantMap &settings) = 0;

signals:
    void infoChanged();
    void readyChanged();
    void onlineChanged();
    void busyChanged();
    void lastErrorChanged();
    // Short user-facing message ("Saved", "Write failed: …").
    void toast(const QString &message, bool isError);
    void disconnected();

protected:
    // Post a job to the I/O thread; busy is true until it finishes.
    void runIo(std::function<void()> job);
    // Run a functor on the GUI thread (safe even if this is deleted meanwhile).
    template <typename F>
    void onGui(F &&f)
    {
        QMetaObject::invokeMethod(this, std::forward<F>(f), Qt::QueuedConnection);
    }
    void stopIo() { m_io.stop(); }
public:
    // Demo devices have no hardware: I/O jobs are skipped.
    void setDemo(bool d) { m_demo = d; }
    bool demo() const { return m_demo; }
protected:
    bool m_demo = false;

    void setInfo(const QString &displayName, const QString &connection, const QString &firmware);
    void setReady(bool r);
    void setOnline(bool o);
    void setLastError(const QString &e);

    IoWorker m_io;

private:
    QString m_kind;
    QString m_key;
    QString m_displayName;
    QString m_connection;
    QString m_firmware;
    QString m_lastError;
    bool m_ready = false;
    bool m_online = false;
    int m_busyCount = 0;
};

} // namespace flux
