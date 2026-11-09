// Idle fade: after a while without keyboard/mouse input the backlight and the
// side light dim, then go dark; the next input brings them straight back.
//
// How each kind of lighting is dimmed:
//   * Flux effects   - the streamed frame is scaled on the host (RAM only).
//   * Per-key design - scaled colours are written without the per-key save,
//                      so only RAM changes; flash keeps the design.
//   * Built-in modes - the hardware brightness is stepped down, and the
//                      effect is switched off at the end.
//   * Side light     - its brightness is stepped down, then mode 0 (off).
// The last two are SETs the keyboard keeps, so they're rate-limited and a
// restore marker in QSettings puts them back if Flux dies while it's dark.

#include "MadKeyboard.h"

#include "app/InputIdle.h"

#include <QLoggingCategory>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <future>

Q_LOGGING_CATEGORY(lcIdle, "flux.idle")

namespace flux {

using namespace mad;

namespace {

constexpr int kHwGapMs = 4000;        // built-in / side-light SETs during a real fade
constexpr int kKeysGapMs = 1000;      // per-key RAM rewrites during a real fade
constexpr int kPreviewFadeMs = 4000;
constexpr int kPreviewHoldMs = 1200;
constexpr int kWakeMs = 450;          // Flux effects fade back in

// LED output is linear, eyes aren't: a squared ramp reads as an even fade.
double fadeCurve(double p)
{
    const double x = 1.0 - std::clamp(p, 0.0, 1.0);
    return x * x;
}

Rgb scaled(const Rgb &c, double k)
{
    auto s = [k](uint8_t v) { return uint8_t(std::clamp<long>(std::lround(v * k), 0, 255)); };
    return Rgb{s(c.r), s(c.g), s(c.b)};
}

bool sameRgb(const Rgb &a, const Rgb &b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

const QString kMarker = QStringLiteral("keyboard-idle/restore");

} // namespace

void MadKeyboard::setupIdleWatch()
{
    if (demo())
        return;
    if (!m_inputIdle) {
        m_inputIdle = new InputIdle(this);
        connect(m_inputIdle, &InputIdle::idled, this, [this] {
            if (m_idleOn)
                beginIdleFade(std::max(0, m_idleOffMin - m_idleDimMin) * 60000, false);
        });
        connect(m_inputIdle, &InputIdle::resumed, this, [this] {
            if (m_idleActive && !m_idlePreview)
                idleWake(false);
        });
    }
    m_inputIdle->setTimeout(m_idleOn ? m_idleDimMin * 60000 : 0);
}

void MadKeyboard::setIdleFade(bool on, int dimAfterMin, int offAfterMin)
{
    dimAfterMin = std::clamp(dimAfterMin, 1, 120);
    offAfterMin = std::clamp(offAfterMin, dimAfterMin, 180);
    if (on == m_idleOn && dimAfterMin == m_idleDimMin && offAfterMin == m_idleOffMin)
        return;
    m_idleOn = on;
    m_idleDimMin = dimAfterMin;
    m_idleOffMin = offAfterMin;
    QSettings st(QStringLiteral("flux"), flux::settingsApp());
    st.setValue(QStringLiteral("keyboard-idle/on"), on);
    st.setValue(QStringLiteral("keyboard-idle/dimAfter"), dimAfterMin);
    st.setValue(QStringLiteral("keyboard-idle/offAfter"), offAfterMin);
    setupIdleWatch();
    emit idleChanged();
}

void MadKeyboard::previewIdleFade()
{
    idleWake(true);
    beginIdleFade(kPreviewFadeMs, true);
    if (!m_idleActive)
        emit toast(QStringLiteral("Nothing is lit right now"), false);
}

void MadKeyboard::beginIdleFade(int fadeMs, bool preview)
{
    if (m_idleWaking) {
        m_idleWaking = false;
        m_dim = 1.f;
    }
    if (m_idleActive || (!online() && !demo()))
        return;
    m_dimPath = !m_fxId.isEmpty() ? DimPath::Flux
              : m_light.effect == 1 ? DimPath::PerKey
              : m_light.effect != 0 ? DimPath::BuiltIn
                                    : DimPath::None;
    m_dimBox = m_box.mode != 0;
    if (m_dimPath == DimPath::None && !m_dimBox)
        return;

    m_idleActive = true;
    m_idlePreview = preview;
    m_idleFadeMs = std::max(0, fadeMs);
    m_dimLastBright = m_dimLastBox = m_dimLastKeys = -1;
    m_dimHwClock.invalidate();
    m_dimBoxClock.invalidate();
    m_dimKeysClock.invalidate();
    if (m_dimPath == DimPath::PerKey)
        runIo([this, c = m_colors] { m_dimKeysSent = c; });
    saveIdleMarker();
    qCDebug(lcIdle) << "fade start, path" << int(m_dimPath) << "side light" << m_dimBox << "over" << m_idleFadeMs << "ms"
                    << (preview ? "(preview)" : "");
    m_idleClock.start();
    m_idleTimer->start(preview ? 40 : 500);
    idleTick();
}

void MadKeyboard::idleTick()
{
    if (m_idleWaking) {
        const double t = std::min(1.0, m_idleClock.elapsed() / double(kWakeMs));
        const double e = 1.0 - std::pow(1.0 - t, 3.0);
        m_dim = float(m_wakeFrom + (1.0 - m_wakeFrom) * e);
        m_idleLevel = m_dim;
        if (t >= 1.0) {
            m_idleWaking = false;
            m_idleTimer->stop();
            m_dim = 1.f;
            m_idleLevel = 1.0;
        }
        emit idleLevelChanged();
        return;
    }
    if (!m_idleActive) {
        m_idleTimer->stop();
        return;
    }
    const double p = m_idleFadeMs > 0 ? m_idleClock.elapsed() / double(m_idleFadeMs) : 1.0;
    const bool final = p >= 1.0;
    applyIdleLevel(fadeCurve(p), final);
    if (final) {
        m_idleTimer->stop();
        qCDebug(lcIdle) << "lights out";
        if (m_idlePreview)
            QTimer::singleShot(kPreviewHoldMs, this, [this] {
                if (m_idlePreview)
                    idleWake(false);
            });
    }
}

void MadKeyboard::applyIdleLevel(double level, bool final)
{
    if (final)
        level = 0;
    m_idleLevel = level;
    emit idleLevelChanged();
    const int hwGap = m_idlePreview ? 150 : kHwGapMs;
    const int keyGap = m_idlePreview ? 60 : kKeysGapMs;
    auto due = [final](QElapsedTimer &clk, int gap) { return final || !clk.isValid() || clk.elapsed() >= gap; };

    switch (m_dimPath) {
    case DimPath::Flux:
        m_dim = float(level);
        break;
    case DimPath::PerKey: {
        const int q = int(std::lround(level * 255));
        if (q != m_dimLastKeys && due(m_dimKeysClock, keyGap)) {
            m_dimLastKeys = q;
            m_dimKeysClock.start();
            std::vector<Rgb> cols(kKeys);
            for (int i = 0; i < kKeys; ++i)
                cols[i] = scaled(m_colors[i], level);
            runIo([this, cols] {
                if (!m_proto)
                    return;
                if (m_dimKeysSent.size() != size_t(kKeys))
                    m_dimKeysSent.assign(kKeys, Rgb{1, 2, 3});
                for (int i = 0; i < kKeys; ++i) {
                    if (!m_present[i] || sameRgb(cols[i], m_dimKeysSent[i]))
                        continue;
                    if (!m_proto->setKeyColor(i / kCols, i % kCols, cols[i]))
                        return;
                    m_dimKeysSent[i] = cols[i];
                }
            });
        }
        break;
    }
    case DimPath::BuiltIn: {
        const int b = final ? 0 : std::max(1, int(std::lround(m_light.brightness * level)));
        if (b != m_dimLastBright && due(m_dimHwClock, hwGap)) {
            m_dimLastBright = b;
            m_dimHwClock.start();
            Lighting l = m_light;
            if (b == 0)
                l.effect = 0;
            else
                l.brightness = uint8_t(b);
            qCDebug(lcIdle) << "backlight brightness" << b;
            runIo([this, l] {
                if (m_proto)
                    m_proto->setLighting(l);
            });
        }
        break;
    }
    case DimPath::None:
        break;
    }

    if (m_dimBox) {
        const int b = final ? 0 : int(std::lround(m_box.brightness * level));
        if (b != m_dimLastBox && due(m_dimBoxClock, hwGap)) {
            m_dimLastBox = b;
            m_dimBoxClock.start();
            BoxLight x = m_box;
            if (b == 0)
                x.mode = 0;
            else
                x.brightness = uint8_t(b);
            qCDebug(lcIdle) << "side light brightness" << b;
            runIo([this, x] {
                if (m_proto)
                    m_proto->setBoxLight(x);
            });
        }
    }
}

void MadKeyboard::idleWake(bool instant)
{
    if (!m_idleActive) {
        if (instant && m_idleWaking) {
            m_idleWaking = false;
            m_idleTimer->stop();
            m_dim = 1.f;
            m_idleLevel = 1.0;
            emit idleLevelChanged();
        }
        return;
    }
    m_idleActive = false;
    m_idleTimer->stop();
    qCDebug(lcIdle) << "wake" << (instant ? "(instant)" : "");

    bool needsRestore = false;
    const bool keys = m_dimPath == DimPath::PerKey && m_dimLastKeys >= 0;
    const bool hw = m_dimPath == DimPath::BuiltIn && m_dimLastBright >= 0;
    const bool box = m_dimBox && m_dimLastBox >= 0;
    if (m_dimPath == DimPath::Flux) {
        if (instant || m_dim.load() >= 1.f) {
            m_dim = 1.f;
        } else {
            m_wakeFrom = m_dim.load();
            m_idleWaking = true;
            m_idleClock.start();
            m_idleTimer->start(16);
        }
    }
    needsRestore = keys || hw || box;
    if (needsRestore) {
        runIo([this, keys, hw, box, colors = m_colors, light = m_light, b = m_box] {
            if (!m_proto)
                return;
            bool ok = true;
            if (box)
                ok = m_proto->setBoxLight(b) && ok;
            if (hw)
                ok = m_proto->setLighting(light) && ok;
            if (keys)
                ok = m_proto->setKeyColors(colors, m_present, false) && ok;
            m_dimKeysSent.clear();
            if (ok)
                onGui([this] {
                    if (!m_idleActive)
                        clearIdleMarker();
                });
        });
    } else {
        clearIdleMarker();
    }
    m_dimPath = DimPath::None;
    m_dimBox = false;
    m_idlePreview = false;
    if (!m_idleWaking)
        m_idleLevel = 1.0;
    emit idleLevelChanged();
}

void MadKeyboard::saveIdleMarker() const
{
    if (demo())
        return;
    QSettings st(QStringLiteral("flux"), flux::settingsApp());
    st.remove(kMarker);
    if (m_dimPath == DimPath::BuiltIn)
        st.setValue(kMarker + QStringLiteral("/light"),
                    QVariantList{m_light.effect, m_light.speed, m_light.r, m_light.g, m_light.b, m_light.brightness});
    if (m_dimPath == DimPath::PerKey) {
        QByteArray raw;
        for (const auto &c : m_colors)
            raw.append(char(c.r)).append(char(c.g)).append(char(c.b));
        st.setValue(kMarker + QStringLiteral("/keys"), QString::fromLatin1(raw.toHex()));
    }
    if (m_dimBox)
        st.setValue(kMarker + QStringLiteral("/box"),
                    QVariantList{m_box.mode, m_box.colorful, m_box.brightness, m_box.speed, m_box.r, m_box.g, m_box.b});
}

void MadKeyboard::clearIdleMarker() const
{
    if (demo())
        return;
    QSettings(QStringLiteral("flux"), flux::settingsApp()).remove(kMarker);
}

void MadKeyboard::restoreIdleMarker()
{
    QSettings st(QStringLiteral("flux"), flux::settingsApp());
    const QVariantList l = st.value(kMarker + QStringLiteral("/light")).toList();
    const QVariantList b = st.value(kMarker + QStringLiteral("/box")).toList();
    const QByteArray k = QByteArray::fromHex(st.value(kMarker + QStringLiteral("/keys")).toString().toLatin1());
    const bool hasLight = l.size() == 6, hasBox = b.size() == 7, hasKeys = k.size() == kKeys * 3;
    if (!hasLight && !hasBox && !hasKeys) {
        st.remove(kMarker);
        return;
    }
    qCInfo(lcIdle) << "Restoring lighting that was dimmed when Flux last exited";
    auto u8 = [](const QVariant &v) { return uint8_t(std::clamp(v.toInt(), 0, 255)); };
    if (hasLight)
        m_light = Lighting{u8(l[0]), u8(l[1]), u8(l[2]), u8(l[3]), u8(l[4]), u8(l[5])};
    if (hasBox)
        m_box = BoxLight{u8(b[0]), u8(b[1]), u8(b[2]), u8(b[3]), u8(b[4]), u8(b[5]), u8(b[6])};
    if (hasKeys)
        for (int i = 0; i < kKeys; ++i)
            m_colors[i] = Rgb{uint8_t(k[i * 3]), uint8_t(k[i * 3 + 1]), uint8_t(k[i * 3 + 2])};
    runIo([this, hasLight, hasBox, hasKeys, light = m_light, box = m_box, colors = m_colors] {
        if (!m_proto)
            return;
        bool ok = true;
        if (hasBox)
            ok = m_proto->setBoxLight(box) && ok;
        if (hasLight)
            ok = m_proto->setLighting(light) && ok;
        if (hasKeys)
            ok = m_proto->setKeyColors(colors, m_present, false) && ok;
        if (ok)
            onGui([this] {
                if (!m_idleActive)
                    clearIdleMarker();
            });
    });
}

// Called from the destructor: put everything back before the I/O thread stops.
// Returns once the writes are done (or after a short timeout).
void MadKeyboard::idleRestoreBlocking()
{
    if (!m_idleActive || demo())
        return;
    m_idleActive = false;
    const bool keys = m_dimPath == DimPath::PerKey && m_dimLastKeys >= 0;
    const bool hw = m_dimPath == DimPath::BuiltIn && m_dimLastBright >= 0;
    const bool box = m_dimBox && m_dimLastBox >= 0;
    m_dim = 1.f;
    if (!keys && !hw && !box) {
        clearIdleMarker();
        return;
    }
    auto done = std::make_shared<std::promise<bool>>();
    auto fut = done->get_future();
    m_io.post([this, done, keys, hw, box, colors = m_colors, light = m_light, b = m_box, present = m_present] {
        bool ok = m_proto != nullptr;
        if (m_proto) {
            if (box)
                ok = m_proto->setBoxLight(b) && ok;
            if (hw)
                ok = m_proto->setLighting(light) && ok;
            if (keys)
                ok = m_proto->setKeyColors(colors, present, false) && ok;
        }
        done->set_value(ok);
    });
    if (fut.wait_for(std::chrono::milliseconds(1500)) == std::future_status::ready && fut.get())
        clearIdleMarker();
}

} // namespace flux
