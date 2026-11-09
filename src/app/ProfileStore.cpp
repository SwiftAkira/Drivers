#include "ProfileStore.h"

#include "core/DeviceBase.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

namespace flux {

namespace {
QString sanitize(QString name)
{
    name = name.trimmed();
    static const QString bad = QStringLiteral("/\\:*?\"<>|");
    for (QChar &c : name)
        if (bad.contains(c) || c.unicode() < 32)
            c = QLatin1Char('_');
    return name.left(64);
}
} // namespace

ProfileStore::ProfileStore(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
}

ProfileStore *ProfileStore::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

QString ProfileStore::folder(const QString &kind) const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1Char('/') + flux::settingsApp() + QStringLiteral("/profiles/") + kind;
}

QString ProfileStore::pathFor(const QString &kind, const QString &name) const
{
    return folder(kind) + QLatin1Char('/') + sanitize(name) + QStringLiteral(".json");
}

QVariantList ProfileStore::list(const QString &kind) const
{
    QVariantList out;
    QDir dir(folder(kind));
    const auto files = dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Time);
    for (const QFileInfo &fi : files) {
        QVariantMap m;
        m.insert(QStringLiteral("name"), fi.completeBaseName());
        m.insert(QStringLiteral("modified"), fi.lastModified().toString(QStringLiteral("d MMM yyyy, HH:mm")));
        out << m;
    }
    return out;
}

bool ProfileStore::save(const QString &kind, const QString &name, const QVariantMap &data)
{
    if (sanitize(name).isEmpty())
        return false;
    QDir().mkpath(folder(kind));
    QSaveFile f(pathFor(kind, name));
    if (!f.open(QIODevice::WriteOnly))
        return false;
    QJsonObject root = QJsonObject::fromVariantMap(data);
    root.insert(QStringLiteral("_flux"), QJsonObject{{QStringLiteral("kind"), kind}, {QStringLiteral("version"), 1}});
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    const bool ok = f.commit();
    ++m_revision;
    emit changed();
    return ok;
}

QVariantMap ProfileStore::load(const QString &kind, const QString &name) const
{
    QFile f(pathFor(kind, name));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QVariantMap m = QJsonDocument::fromJson(f.readAll()).object().toVariantMap();
    m.remove(QStringLiteral("_flux"));
    return m;
}

bool ProfileStore::remove(const QString &kind, const QString &name)
{
    const bool ok = QFile::remove(pathFor(kind, name));
    ++m_revision;
    emit changed();
    return ok;
}

bool ProfileStore::rename(const QString &kind, const QString &from, const QString &to)
{
    if (sanitize(to).isEmpty())
        return false;
    const bool ok = QFile::rename(pathFor(kind, from), pathFor(kind, to));
    ++m_revision;
    emit changed();
    return ok;
}

void ProfileStore::openFolder(const QString &kind) const
{
    QDir().mkpath(folder(kind));
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder(kind)));
}

} // namespace flux
