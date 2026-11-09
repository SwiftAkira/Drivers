#include "IconProvider.h"

#include <QColor>
#include <QFile>
#include <QHash>
#include <QMutex>
#include <QPainter>
#include <QSvgRenderer>

namespace flux {

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage IconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    // id = "<name>/<hex colour>"
    const int slash = id.lastIndexOf(QLatin1Char('/'));
    const QString name = slash > 0 ? id.left(slash) : id;
    QString hex = slash > 0 ? id.mid(slash + 1) : QStringLiteral("ffffff");
    QColor color;
    if (hex.size() == 8) // aarrggbb (QML colour toString with alpha)
        color = QColor::fromRgba(hex.toUInt(nullptr, 16));
    else
        color = QColor(QLatin1Char('#') + hex);
    if (!color.isValid())
        color = Qt::white;

    static QMutex mutex;
    static QHash<QString, QByteArray> svgCache;
    QByteArray svg;
    {
        QMutexLocker lock(&mutex);
        auto it = svgCache.find(name);
        if (it == svgCache.end()) {
            QFile f(QStringLiteral(":/qt/qml/Flux/resources/icons/%1.svg").arg(name));
            it = svgCache.insert(name, f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray());
        }
        svg = it.value();
    }

    const QSize out = requestedSize.isValid() && !requestedSize.isEmpty() ? requestedSize : QSize(48, 48);
    if (size)
        *size = out;
    QImage img(out, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    if (svg.isEmpty())
        return img;

    // Icons are drawn in white; swap in the colour (alpha applied after).
    const QColor opaque(color.red(), color.green(), color.blue());
    svg.replace("#ffffff", opaque.name().toLatin1());
    QSvgRenderer renderer(svg);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(color.alphaF());
    renderer.render(&p, QRectF(QPointF(0, 0), out));
    return img;
}

} // namespace flux
