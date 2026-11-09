// Offline contact sheet of every Flux effect: simulated presses + synthetic audio.
//   fxpreview <layout.json> <out.png>
#include "devices/mad/LightEngine.h"

#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>

#include <cmath>

using namespace flux::fx;

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QFile f(argv[1]);
    f.open(QIODevice::ReadOnly);
    const auto keys = QJsonDocument::fromJson(f.readAll()).object().value("keys").toArray();
    std::vector<KeyPos> pos(70);
    struct R { int idx; double x, y, w, h; QString label; };
    std::vector<R> rects;
    float W = 0, H = 0;
    int idxA = 0, idxL = 0, idxG = 0, idxK = 0, idxD = 0;
    for (const auto &v : keys) {
        const auto k = v.toObject();
        const int i = k["row"].toInt() * 14 + k["col"].toInt();
        const double x = k["x"].toDouble(), y = k["y"].toDouble(), w = k["w"].toDouble(1), h = k["h"].toDouble(1);
        pos[i] = {true, float(x + w / 2), float(y + h / 2)};
        rects.push_back({i, x, y, w, h, k["label"].toString()});
        W = std::max(W, float(x + w));
        H = std::max(H, float(y + h));
        const QString l = k["label"].toString();
        if (l == "A") idxA = i; if (l == "L") idxL = i; if (l == "G") idxG = i; if (l == "K") idxK = i; if (l == "D") idxD = i;
    }
    const int cols = 6, u = 14, pad = 10, labelW = 130;
    const int kbW = int(W * u), kbH = int(H * u);
    const auto &fxList = effects();
    QImage img(labelW + cols * (kbW + pad) + pad, int(fxList.size()) * (kbH + pad) + pad, QImage::Format_RGB32);
    img.fill(QColor("#151517"));
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QColor("#dddddd"));
    p.setFont(QFont("Inter Variable", 9));
    int row = 0;
    for (const auto &e : fxList) {
        Engine eng;
        eng.setLayout(pos, W, H);
        Params pr;
        pr.effect = e.id;
        for (int k = 0; k < 3; ++k) {
            const uint32_t c = e.palette[k];
            pr.palette[k] = {((c >> 16) & 255) / 255.f, ((c >> 8) & 255) / 255.f, (c & 255) / 255.f};
        }
        pr.actuationMm = 1.0f;
        eng.setParams(pr);
        const double shots[cols] = {0.45, 0.75, 1.05, 1.5, 2.05, 2.6};
        int shot = 0;
        for (int fr = 0; fr <= 2.6 * 40; ++fr) {
            const double t = fr / 40.0;
            // Typing script: A, D, L, G, K taps (0.15 s each) + A&L held 1.7–2.3 s.
            std::vector<float> travel(70, 0.f);
            auto tap = [&](int idx, double at, float depth) { if (t >= at && t < at + 0.15) travel[idx] = depth; };
            tap(idxA, 0.30, 3.4f); tap(idxD, 0.55, 2.0f); tap(idxL, 0.70, 3.4f); tap(idxG, 0.95, 1.6f); tap(idxK, 1.25, 3.4f);
            tap(idxA, 1.45, 3.4f); tap(idxD, 1.60, 3.4f);
            if (t > 1.7 && t < 2.3) { travel[idxA] = 3.0f; travel[idxL] = 3.0f; }
            // Synthetic music: 120 BPM kick, a melody bump sweeping the bands.
            AudioFrame a;
            a.active = true;
            const double beat = std::fmod(t, 0.5);
            a.bass = beat < 0.08 ? 1.0f : 0.2f;
            a.beats = uint32_t(t / 0.5);
            a.level = 0.6f + 0.4f * float(beat < 0.1);
            a.treble = 0.4f;
            for (int b = 0; b < 14; ++b)
                a.bands[b] = float(std::clamp(0.25 + 0.7 * std::exp(-std::pow(b - 7 - 5 * std::sin(t * 2.0), 2) / 6.0) + (b < 3 ? a.bass * 0.6 : 0), 0.0, 1.0));
            for (int k = 0; k < 28; ++k)
                a.wave[k] = float(0.8 * std::sin(k * 0.45 + t * 9) * std::sin(k * 0.11 + t * 2));
            eng.setAudio(a);
            const auto frame = eng.render(t, travel);
            if (shot < cols && t >= shots[shot] - 1e-6) {
                const int ox = labelW + pad + shot * (kbW + pad), oy = pad + row * (kbH + pad);
                for (const auto &r : rects) {
                    const auto &c = frame[r.idx];
                    QColor q(int(std::min(1.f, c.r) * 255), int(std::min(1.f, c.g) * 255), int(std::min(1.f, c.b) * 255));
                    p.setPen(Qt::NoPen);
                    p.setBrush(q);
                    p.drawRoundedRect(QRectF(ox + r.x * u + 1, oy + r.y * u + 1, r.w * u - 2, r.h * u - 2), 2, 2);
                }
                ++shot;
            }
        }
        p.setPen(QColor("#dddddd"));
        p.drawText(QRect(8, pad + row * (kbH + pad), labelW - 10, kbH), Qt::AlignVCenter | Qt::AlignLeft,
                   QString::fromUtf8(e.name) + (e.usesAudio ? "  ♪" : e.usesTravel ? "  ⚡" : ""));
        ++row;
    }
    p.end();
    img.save(argv[2]);
    return 0;
}
