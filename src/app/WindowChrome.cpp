#include "WindowChrome.h"

#include <KWindowEffects>
#include <KWindowShadow>

#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QTimer>

#include <cmath>
#include <vector>

namespace flux {

namespace {

// Separable box blur on the alpha channel, 3 passes ≈ gaussian.
void blurAlpha(QImage &img, int radius)
{
    const int w = img.width(), h = img.height();
    std::vector<int> a(w * h), tmp(w * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            a[y * w + x] = qAlpha(img.pixel(x, y));
    for (int pass = 0; pass < 3; ++pass) {
        for (int y = 0; y < h; ++y) {
            int acc = 0;
            for (int x = -radius; x <= radius; ++x)
                acc += a[y * w + std::clamp(x, 0, w - 1)];
            for (int x = 0; x < w; ++x) {
                tmp[y * w + x] = acc / (2 * radius + 1);
                acc += a[y * w + std::min(x + radius + 1, w - 1)] - a[y * w + std::max(x - radius, 0)];
            }
        }
        for (int x = 0; x < w; ++x) {
            int acc = 0;
            for (int y = -radius; y <= radius; ++y)
                acc += tmp[std::clamp(y, 0, h - 1) * w + x];
            for (int y = 0; y < h; ++y) {
                a[y * w + x] = acc / (2 * radius + 1);
                acc += tmp[std::min(y + radius + 1, h - 1) * w + x] - tmp[std::max(y - radius, 0) * w + x];
            }
        }
    }
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            img.setPixel(x, y, qRgba(0, 0, 0, a[y * w + x]));
}

KWindowShadowTile::Ptr makeTile(const QImage &img)
{
    auto t = KWindowShadowTile::Ptr::create();
    t->setImage(img);
    t->create();
    return t;
}

} // namespace

WindowChrome::WindowChrome(QObject *parent)
    : QObject(parent)
{
}

WindowChrome::~WindowChrome() = default;

void WindowChrome::setWindow(QWindow *w)
{
    if (w == m_window)
        return;
    m_window = w;
    emit windowChanged();
    if (m_window) {
        connect(m_window, &QWindow::visibleChanged, this, [this] { QTimer::singleShot(0, this, &WindowChrome::apply); });
        connect(m_window, &QWindow::windowStateChanged, this, [this] { QTimer::singleShot(0, this, &WindowChrome::apply); });
    }
    apply();
}

void WindowChrome::setBlurRect(const QRect &r)
{
    if (r == m_blurRect)
        return;
    m_blurRect = r;
    emit blurRectChanged();
    apply();
}

void WindowChrome::setRadius(int r)
{
    if (r == m_radius)
        return;
    m_radius = r;
    emit radiusChanged();
    apply();
}

void WindowChrome::setShadowEnabled(bool e)
{
    if (e == m_shadowEnabled)
        return;
    m_shadowEnabled = e;
    emit shadowEnabledChanged();
    applyShadow();
}

bool WindowChrome::blurAvailable() const
{
    return KWindowEffects::isEffectAvailable(KWindowEffects::BlurBehind);
}

void WindowChrome::apply()
{
    if (!m_window || !m_window->isVisible())
        return;
    const bool maximized = m_window->windowStates() & (Qt::WindowMaximized | Qt::WindowFullScreen);
    const int r = maximized ? 0 : m_radius;

    if (blurAvailable() && m_blurRect.isValid()) {
        QPainterPath path;
        // Round only the outer (left) corners of the sidebar.
        const QRectF br = m_blurRect;
        path.moveTo(br.left() + r, br.top());
        path.lineTo(br.right() + 1, br.top());
        path.lineTo(br.right() + 1, br.bottom() + 1);
        path.lineTo(br.left() + r, br.bottom() + 1);
        if (r > 0)
            path.arcTo(QRectF(br.left(), br.bottom() + 1 - 2 * r, 2 * r, 2 * r), 270, -90);
        path.lineTo(br.left(), br.top() + r);
        if (r > 0)
            path.arcTo(QRectF(br.left(), br.top(), 2 * r, 2 * r), 180, -90);
        path.closeSubpath();
        const QRegion region(path.toFillPolygon().toPolygon());
        KWindowEffects::enableBlurBehind(m_window, true, region);
    } else {
        KWindowEffects::enableBlurBehind(m_window, false);
    }
    applyShadow();
}

void WindowChrome::applyShadow()
{
    if (!m_window)
        return;
    const bool maximized = m_window->windowStates() & (Qt::WindowMaximized | Qt::WindowFullScreen);
    if (!m_shadowEnabled || maximized || !m_window->isVisible()) {
        if (m_shadow)
            m_shadow->destroy();
        return;
    }
    if (m_shadow && m_shadow->isCreated())
        return;

    // Shadow image: a rounded "window" in the middle, blurred, then the
    // window shape itself punched out so translucent areas stay clean.
    const int blur = 14;        // box radius per pass
    const int spread = 34;      // how far the shadow reaches outside
    const int offsetY = 8;
    const int r = m_radius;
    const int edge = spread + r + 2;
    const int size = edge * 2 + 1;

    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 150));
        p.drawRoundedRect(QRectF(spread, spread + offsetY, size - 2 * spread, size - 2 * spread - offsetY / 2.0), r, r);
    }
    blurAlpha(img, blur);
    {
        // A crisp 1px dark outline helps separate the window from dark wallpapers.
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor(0, 0, 0, 110), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(spread - 0.5, spread - 0.5, size - 2 * spread + 1, size - 2 * spread + 1), r + 0.5, r + 0.5);
        p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::black);
        p.drawRoundedRect(QRectF(spread, spread, size - 2 * spread, size - 2 * spread), r, r);
    }

    auto sub = [&](int x, int y, int w, int h) { return makeTile(img.copy(x, y, w, h)); };

    if (!m_shadow)
        m_shadow = new KWindowShadow(this);
    m_shadow->setTopLeftTile(sub(0, 0, edge, edge));
    m_shadow->setTopTile(sub(edge, 0, 1, edge));
    m_shadow->setTopRightTile(sub(edge + 1, 0, edge, edge));
    m_shadow->setRightTile(sub(edge + 1, edge, edge, 1));
    m_shadow->setBottomRightTile(sub(edge + 1, edge + 1, edge, edge));
    m_shadow->setBottomTile(sub(edge, edge + 1, 1, edge));
    m_shadow->setBottomLeftTile(sub(0, edge + 1, edge, edge));
    m_shadow->setLeftTile(sub(0, edge, edge, 1));
    m_shadow->setPadding(QMargins(spread, spread, spread, spread));
    m_shadow->setWindow(m_window);
    m_shadow->create();
}

} // namespace flux
