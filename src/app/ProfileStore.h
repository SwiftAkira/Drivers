#pragma once

#include <QObject>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace flux {

// Named setting snapshots saved as JSON under
// ~/.local/share/flux/profiles/<kind>/<name>.json
class ProfileStore : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Profiles)
    QML_SINGLETON

    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    explicit ProfileStore(QObject *parent); // no default: QML must use create()
    static ProfileStore *create(QQmlEngine *, QJSEngine *);

    int revision() const { return m_revision; }

    Q_INVOKABLE QVariantList list(const QString &kind) const; // [{name, modified}]
    Q_INVOKABLE bool save(const QString &kind, const QString &name, const QVariantMap &data);
    Q_INVOKABLE QVariantMap load(const QString &kind, const QString &name) const;
    Q_INVOKABLE bool remove(const QString &kind, const QString &name);
    Q_INVOKABLE bool rename(const QString &kind, const QString &from, const QString &to);
    Q_INVOKABLE QString folder(const QString &kind) const;
    Q_INVOKABLE void openFolder(const QString &kind) const;

signals:
    void changed();

private:
    QString pathFor(const QString &kind, const QString &name) const;
    int m_revision = 0;
    static inline ProfileStore *s_instance = nullptr;
};

} // namespace flux
