#pragma once

#include <QObject>
#include <QPointer>
#include <QRect>
#include <QWindow>
#include <QtQml/qqmlregistration.h>

class KWindowShadow;

namespace flux {

// Gives the frameless main window a macOS feel on KWin: a soft native drop
// shadow around the rounded window and real blur behind the sidebar.
class WindowChrome : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QWindow *window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(QRect blurRect READ blurRect WRITE setBlurRect NOTIFY blurRectChanged)
    Q_PROPERTY(int radius READ radius WRITE setRadius NOTIFY radiusChanged)
    Q_PROPERTY(bool shadowEnabled READ shadowEnabled WRITE setShadowEnabled NOTIFY shadowEnabledChanged)
    Q_PROPERTY(bool blurAvailable READ blurAvailable NOTIFY blurAvailableChanged)

public:
    explicit WindowChrome(QObject *parent = nullptr);
    ~WindowChrome() override;

    QWindow *window() const { return m_window; }
    void setWindow(QWindow *w);
    QRect blurRect() const { return m_blurRect; }
    void setBlurRect(const QRect &r);
    int radius() const { return m_radius; }
    void setRadius(int r);
    bool shadowEnabled() const { return m_shadowEnabled; }
    void setShadowEnabled(bool e);
    bool blurAvailable() const;

signals:
    void windowChanged();
    void blurRectChanged();
    void radiusChanged();
    void shadowEnabledChanged();
    void blurAvailableChanged();

private:
    void apply();
    void applyShadow();

    QPointer<QWindow> m_window;
    QRect m_blurRect;
    int m_radius = 12;
    bool m_shadowEnabled = true;
    KWindowShadow *m_shadow = nullptr;
};

} // namespace flux
