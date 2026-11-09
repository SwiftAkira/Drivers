#pragma once

#include <QQuickImageProvider>

namespace flux {

// image://icon/<name>/<rrggbb[aa]> — renders a Lucide SVG tinted to a colour.
// Works on every scene-graph backend (no shader effects needed).
class IconProvider : public QQuickImageProvider {
public:
    IconProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};

} // namespace flux
