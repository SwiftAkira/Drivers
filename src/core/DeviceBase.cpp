#include "DeviceBase.h"

namespace flux {

DeviceBase::DeviceBase(QString kind, QString key, QObject *parent)
    : QObject(parent)
    , m_io(("flux-" + kind).toStdString())
    , m_kind(std::move(kind))
    , m_key(std::move(key))
{
    m_io.start();
}

DeviceBase::~DeviceBase()
{
    m_io.stop();
}

void DeviceBase::runIo(std::function<void()> job)
{
    if (m_demo)
        return;
    if (++m_busyCount == 1)
        emit busyChanged();
    m_io.post([this, job = std::move(job)] {
        job();
        onGui([this] {
            if (--m_busyCount == 0)
                emit busyChanged();
        });
    });
}

void DeviceBase::setInfo(const QString &displayName, const QString &connection, const QString &firmware)
{
    if (displayName == m_displayName && connection == m_connection && firmware == m_firmware)
        return;
    m_displayName = displayName;
    m_connection = connection;
    m_firmware = firmware;
    emit infoChanged();
}

void DeviceBase::setReady(bool r)
{
    if (r == m_ready)
        return;
    m_ready = r;
    emit readyChanged();
}

void DeviceBase::setOnline(bool o)
{
    if (o == m_online)
        return;
    m_online = o;
    emit onlineChanged();
}

void DeviceBase::setLastError(const QString &e)
{
    if (e == m_lastError)
        return;
    m_lastError = e;
    emit lastErrorChanged();
    if (!e.isEmpty())
        emit toast(e, true);
}

} // namespace flux
