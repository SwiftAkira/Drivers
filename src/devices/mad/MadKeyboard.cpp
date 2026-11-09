#include "MadKeyboard.h"

#include "core/KeyNames.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSettings>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <future>
#include <random>
#include <thread>

Q_LOGGING_CATEGORY(lcMad, "flux.mad")

namespace flux {

using namespace mad;

namespace {

constexpr const char *kLayoutRes = ":/qt/qml/Flux/resources/data/mad60he-layout.json";
constexpr const char *kKeycodesRes = ":/qt/qml/Flux/resources/data/mad60he-keycodes.json";

uint16_t toRaw(double mm) { return uint16_t(std::clamp(std::lround(mm * 100.0), 0L, 65535L)); }

QJsonObject loadJson(const char *path)
{
    QFile f(QString::fromLatin1(path));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

// Standard 61-key ANSI 60% used when the layout resource is missing.
QVariantList fallbackLayout()
{
    struct K { double w; };
    const std::vector<std::vector<double>> rows = {
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2},
        {1.5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1.5},
        {1.75, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2.25},
        {2.25, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2.75},
        {1.25, 1.25, 1.25, 6.25, 1.25, 1.25, 1.25, 1.25},
    };
    const std::vector<std::vector<int>> cols = {
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13},
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13},
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13},
        {0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13},
        {0, 1, 2, 6, 10, 11, 12, 13},
    };
    QVariantList out;
    for (int r = 0; r < 5; ++r) {
        double x = 0;
        for (size_t i = 0; i < rows[r].size(); ++i) {
            const int c = cols[r][i];
            out << QVariantMap{{QStringLiteral("index"), r * kCols + c}, {QStringLiteral("row"), r}, {QStringLiteral("col"), c},
                               {QStringLiteral("x"), x}, {QStringLiteral("y"), double(r)}, {QStringLiteral("w"), rows[r][i]},
                               {QStringLiteral("h"), 1.0}, {QStringLiteral("label"), QString()}};
            x += rows[r][i];
        }
    }
    return out;
}

QVariantList parseLayout(const QJsonObject &root)
{
    QJsonArray keys = root.value(QStringLiteral("keys")).toArray();
    if (keys.isEmpty()) {
        // Accept {"rows":[[{...}], ...]} too.
        for (const auto &row : root.value(QStringLiteral("rows")).toArray())
            for (const auto &k : row.toArray())
                keys.append(k);
    }
    QVariantList out;
    for (const auto &v : keys) {
        const QJsonObject k = v.toObject();
        if (!k.contains(QStringLiteral("row")) || !k.contains(QStringLiteral("col")))
            continue;
        const int r = k.value(QStringLiteral("row")).toInt();
        const int c = k.value(QStringLiteral("col")).toInt();
        if (r < 0 || r >= kRows || c < 0 || c >= kCols)
            continue;
        out << QVariantMap{{QStringLiteral("index"), r * kCols + c},
                           {QStringLiteral("row"), r},
                           {QStringLiteral("col"), c},
                           {QStringLiteral("x"), k.value(QStringLiteral("x")).toDouble()},
                           {QStringLiteral("y"), k.value(QStringLiteral("y")).toDouble()},
                           {QStringLiteral("w"), k.value(QStringLiteral("w")).toDouble(1.0)},
                           {QStringLiteral("h"), k.value(QStringLiteral("h")).toDouble(1.0)},
                           {QStringLiteral("label"), k.value(QStringLiteral("label")).toString()}};
    }
    return out.size() >= 40 ? out : fallbackLayout();
}

QString modsText(int m5)
{
    // QMK 5-bit mod: bit4 = right hand, bits0-3 = Ctrl/Shift/Alt/GUI
    QStringList p;
    const QString side = (m5 & 0x10) ? QStringLiteral("R") : QString();
    if (m5 & 0x01) p << side + QStringLiteral("Ctrl");
    if (m5 & 0x02) p << side + QStringLiteral("Shift");
    if (m5 & 0x04) p << side + QStringLiteral("Alt");
    if (m5 & 0x08) p << side + QStringLiteral("Super");
    return p.join(QLatin1Char('+'));
}

} // namespace

DeviceMatch MadKeyboard::match()
{
    return DeviceMatch{
        QStringLiteral("keyboard"),
        QStringLiteral("MAD LIGHT 60 HE"),
        kVendorId,
        {kPidMadLight60},
        kUsagePage,
        kUsage,
        [](const HidInfo &info, QObject *parent) -> DeviceBase * { return new MadKeyboard(info, parent); },
    };
}

MadKeyboard::MadKeyboard(const HidInfo &info, QObject *parent)
    : DeviceBase(QStringLiteral("keyboard"), QString::fromStdString(info.path), parent)
    , m_info(info)
{
    setInfo(QStringLiteral("MAD LIGHT 60 HE"), QStringLiteral("USB"), QString());
    loadData();
    for (int i = 0; i < kKeys; ++i) {
        m_travel << 0.0;
        m_liveColors << QColor(Qt::black);
    }

    std::vector<fx::KeyPos> pos(kKeys);
    float w = 0, h = 0;
    for (const auto &v : m_layout) {
        const QVariantMap k = v.toMap();
        const int i = k.value(QStringLiteral("index")).toInt();
        const float x = k.value(QStringLiteral("x")).toFloat(), y = k.value(QStringLiteral("y")).toFloat();
        const float kw = k.value(QStringLiteral("w")).toFloat(), kh = k.value(QStringLiteral("h")).toFloat();
        if (i >= 0 && i < kKeys)
            pos[i] = {true, x + kw / 2, y + kh / 2};
        w = std::max(w, x + kw);
        h = std::max(h, y + kh);
    }
    m_engine.setLayout(pos, w, h);
    m_present.assign(kKeys, false);
    for (int i = 0; i < kKeys; ++i)
        m_present[i] = pos[i].present;

    QSettings st(QStringLiteral("flux"), flux::settingsApp());
    m_osuMode = st.value(QStringLiteral("keyboard-osu/on"), false).toBool();
    for (const auto &v : st.value(QStringLiteral("keyboard-osu/keys")).toList())
        m_osuKeys << v.toInt();
    m_fxSpeed = st.value(QStringLiteral("keyboard-fx/speed"), 1.0).toDouble();
    m_fxIntensity = st.value(QStringLiteral("keyboard-fx/intensity"), 0.85).toDouble();
    m_fxId = st.value(QStringLiteral("keyboard-fx/effect")).toString();
    resetFluxPalette();
    const QStringList pal = st.value(QStringLiteral("keyboard-fx/palette/%1").arg(m_fxId)).toStringList();
    for (int i = 0; i < 3 && i < pal.size(); ++i)
        m_fxPalette[i] = QColor(pal[i]);
}

MadKeyboard::~MadKeyboard()
{
    m_liveIo = false;
    if (m_fxIo) {
        // Hand the LEDs back to the firmware effect before we go.
        m_fxIo = false;
        auto done = std::make_shared<std::promise<void>>();
        auto fut = done->get_future();
        const auto light = m_lightBeforeFx;
        const auto colors = m_colors;
        const auto present = m_present;
        m_io.post([this, done, light, colors, present] {
            if (m_proto) {
                m_proto->setKeyColors(colors, present, false);
                m_proto->setLighting(light);
            }
            done->set_value();
        });
        fut.wait_for(std::chrono::milliseconds(1500));
    }
    stopIo();
}

void MadKeyboard::loadData()
{
    const QJsonObject layout = loadJson(kLayoutRes);
    m_layout = parseLayout(layout);

    // Optional limits/effects block in the layout resource.
    const QJsonObject limits = layout.value(QStringLiteral("limits")).toObject();
    auto lim = [&](const char *k, double &dst) {
        if (limits.contains(QLatin1String(k)))
            dst = limits.value(QLatin1String(k)).toDouble(dst);
    };
    lim("travelMax", m_travelMax);
    lim("actuationMin", m_actMin);
    lim("actuationMax", m_actMax);
    lim("rtMin", m_rtMin);
    lim("rtMax", m_rtMax);
    lim("deadZoneMax", m_dzMax);
    if (limits.contains(QStringLiteral("speedMax")))
        m_speedMax = limits.value(QStringLiteral("speedMax")).toInt(m_speedMax);
    if (limits.contains(QStringLiteral("brightnessMax")))
        m_brightnessMax = limits.value(QStringLiteral("brightnessMax")).toInt(m_brightnessMax);

    for (const auto &v : layout.value(QStringLiteral("effects")).toArray()) {
        const QJsonObject e = v.toObject();
        m_effects << QVariantMap{{QStringLiteral("id"), e.value(QStringLiteral("id")).toInt()},
                                 {QStringLiteral("name"), e.value(QStringLiteral("name")).toString()},
                                 {QStringLiteral("usesColor"), e.value(QStringLiteral("usesColor")).toBool(true)},
                                 {QStringLiteral("usesSpeed"), e.value(QStringLiteral("usesSpeed")).toBool(true)}};
    }
    if (m_effects.isEmpty()) {
        struct E { int id; const char *name; bool color; bool speed; };
        static const E list[] = {
            {0, "Off", false, false}, {1, "Per-key colours", false, false}, {2, "Solid", true, false}, {3, "Focus", true, false},
            {4, "Rainbow (vertical)", true, false}, {5, "Rainbow (horizontal)", true, false}, {6, "Breathing", true, true},
            {7, "Ribbon 1", true, true}, {8, "Ribbon 2", true, true}, {9, "Rotating belt 1", true, true}, {11, "Rotating belt 2", true, true},
            {13, "Spectrum cycle", false, true}, {19, "Colourful swirls 1", false, true}, {23, "Colourful swirls 2", false, true},
            {25, "Christmas", false, false}, {26, "Spectrum breathing", true, true}, {28, "Spectrum floating", true, true},
            {30, "Digital rain", false, false}, {31, "Light up on press", true, true}, {36, "Cross on press", true, true},
            {39, "Rainbow on press", false, true},
        };
        for (const auto &e : list)
            m_effects << QVariantMap{{QStringLiteral("id"), e.id}, {QStringLiteral("name"), QString::fromLatin1(e.name)},
                                     {QStringLiteral("usesColor"), e.color}, {QStringLiteral("usesSpeed"), e.speed}};
    }

    // Keycode names (basic + QMK lighting/quantum + vendor extras).
    const QJsonObject kc = loadJson(kKeycodesRes);
    for (const char *sect : {"basic", "lighting", "quantum", "vendor"}) {
        for (const auto &v : kc.value(QLatin1String(sect)).toArray()) {
            const QJsonObject o = v.toObject();
            if (o.value(QStringLiteral("code")).isNull() && !o.contains(QStringLiteral("hex")))
                continue;
            int code = o.value(QStringLiteral("code")).toInt(-1);
            if (code < 0 && o.contains(QStringLiteral("hex")))
                code = o.value(QStringLiteral("hex")).toString().toInt(nullptr, 16);
            QString label = o.value(QStringLiteral("short")).toString();
            if (label.isEmpty())
                label = o.value(QStringLiteral("label")).toString();
            if (label.isEmpty())
                label = o.value(QStringLiteral("name")).toString();
            if (code >= 0 && !label.isEmpty())
                m_extraNames.insert(code, label);
        }
    }
}

QStringList MadKeyboard::layerNames() const
{
    return {QStringLiteral("Base"), QStringLiteral("Fn"), QStringLiteral("Mac Base"), QStringLiteral("Mac Fn")};
}

// ── Lifecycle ────────────────────────────────────────────────────────────

void MadKeyboard::start()
{
    if (demo()) {
        Snapshot s;
        s.version = "1.05";
        s.ok = true;
        s.act.assign(kKeys, 120);
        RtInfo rt{true, 15, 15};
        s.rt.assign(kKeys, rt);
        s.dead = DeadZones{10, 10};
        s.keymap.assign(kLayers * kKeys, 1);
        for (const auto &v : m_layout) {
            const QVariantMap k = v.toMap();
            const int i = k.value(QStringLiteral("index")).toInt();
            static const QHash<QString, int> names = [] {
                QHash<QString, int> h;
                for (const auto &kv : KeyNames::hidKeys()) {
                    const QVariantMap m = kv.toMap();
                    h.insert(m.value(QStringLiteral("short")).toString().toLower(), m.value(QStringLiteral("usage")).toInt());
                    h.insert(m.value(QStringLiteral("name")).toString().toLower(), m.value(QStringLiteral("usage")).toInt());
                }
                h.insert(QStringLiteral("lshift"), 0xE1); h.insert(QStringLiteral("rshift"), 0xE5); h.insert(QStringLiteral("lctrl"), 0xE0);
                h.insert(QStringLiteral("rctrl"), 0xE4); h.insert(QStringLiteral("lalt"), 0xE2); h.insert(QStringLiteral("ralt"), 0xE6);
                h.insert(QStringLiteral("lwin"), 0xE3); h.insert(QStringLiteral("win"), 0xE3); h.insert(QStringLiteral("left win"), 0xE3); h.insert(QStringLiteral("backspace"), 0x2A);
                h.insert(QStringLiteral("caps lock"), 0x39); h.insert(QStringLiteral("fn"), 0x5221);
                return h;
            }();
            const QString label = k.value(QStringLiteral("label")).toString().toLower();
            s.keymap[i] = uint16_t(names.value(label, 0x04));
            s.keymap[2 * kKeys + i] = s.keymap[i];
        }
        s.features.nkro = true;
        s.light = Lighting{13, 128, 10, 132, 255, 120};
        s.colors.assign(kKeys, Rgb{10, 132, 255});
        s.macroCount = 16;
        s.macroSize = 1024;
        s.box = BoxLight{1, 1, 80, 20, 255, 255, 255};
        applySnapshot(s);
        m_io.setIdleHook([this] { ioIdle(); });
        return;
    }
    runIo([this] {
        if (!m_dev.open(m_info.path)) {
            const QString err = QString::fromStdString(m_dev.lastError());
            onGui([this, err] { setLastError(QStringLiteral("Couldn't open the keyboard: %1").arg(err)); });
            return;
        }
        m_proto = std::make_unique<Protocol>(m_dev);
        m_dev.drainInput();
        m_io.setIdleHook([this] { ioIdle(); });
        onGui([this] { refresh(); });
    });
}

void MadKeyboard::refresh()
{
    if (demo())
        return;
    runIo([this] {
        Snapshot s;
        Protocol &p = *m_proto;
        auto v = p.version();
        if (!v) {
            const QString err = QString::fromStdString(p.lastError());
            onGui([this, err] {
                setLastError(QStringLiteral("Keyboard didn't respond: %1").arg(err));
                setReady(true);
                setOnline(false);
            });
            return;
        }
        s.version = *v;
        s.ok = true;
        if (auto a = p.actuationAll()) s.act = *a;
        if (auto r = p.rtAll()) s.rt = *r;
        if (auto d = p.deadZones()) s.dead = *d;
        if (auto k = p.keymapAll()) s.keymap = *k;
        if (auto f = p.features()) s.features = *f;
        s.bottom = p.bottomOptimize().value_or(false);
        s.game = p.gameMode().value_or(false);
        s.defaultLayer = p.defaultLayer().value_or(0);
        if (auto l = p.lighting()) s.light = *l;
        if (auto c = p.keyColors()) s.colors = *c;
        if (auto b = p.boxLight()) s.box = *b;
        s.macroCount = p.macroCount().value_or(0);
        s.macroSize = p.macroBufferSize().value_or(0);
        if (s.macroCount > 0 && s.macroSize > 0)
            if (auto b = p.macroBuffer(s.macroCount, s.macroSize))
                s.macroBuf = *b;
        onGui([this, s] { applySnapshot(s); });
    });
}

void MadKeyboard::applySnapshot(const Snapshot &s)
{
    setInfo(displayName(), connection(), QString::fromStdString(s.version));
    if (s.act.size() == size_t(kKeys)) m_act = s.act;
    if (s.rt.size() == size_t(kKeys)) m_rt = s.rt;
    m_dead = s.dead;
    if (s.keymap.size() == size_t(kLayers * kKeys)) m_keymap = s.keymap;
    m_features = s.features;
    m_bottomOptimize = s.bottom;
    m_gameMode = s.game;
    m_defaultLayer = s.defaultLayer;
    m_light = s.light;
    if (s.colors.size() == size_t(kKeys)) m_colors = s.colors;
    m_box = s.box;
    const bool resumeFx = !m_fxId.isEmpty() && !m_fxIo;
    m_macroCount = s.macroCount;
    m_macroBufSize = s.macroSize;
    m_macros = parseMacros(s.macroBuf, s.macroCount);
    setOnline(true);
    setReady(true);
    emit travelSettingsChanged();
    emit keymapChanged();
    emit featuresChanged();
    emit lightingChanged();
    emit macrosChanged();
    emit osuChanged();
    if (resumeFx)
        startFluxEffect(m_fxId);
}

void MadKeyboard::ioIdle()
{
    if (!m_proto) {
        if (!demo()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return;
        }
        // Simulated typist: random keys go down and come back up.
        static std::mt19937 rng(7);
        static std::vector<float> fake(kKeys, 0.f);
        static std::vector<float> phase(kKeys, -1.f);
        const auto now = std::chrono::steady_clock::now();
        const double secs = std::chrono::duration<double>(now - m_fxEpoch).count();
        std::vector<int> present;
        for (const auto &v : m_layout)
            present.push_back(v.toMap().value(QStringLiteral("index")).toInt());
        if (!present.empty() && std::uniform_real_distribution<float>(0, 1)(rng) < 0.12f)
            phase[present[rng() % present.size()]] = 0.f;
        for (int i = 0; i < kKeys; ++i) {
            if (phase[i] < 0) { fake[i] = 0; continue; }
            phase[i] += 0.02f;
            fake[i] = phase[i] < 0.25f ? 3.5f * std::sin(phase[i] / 0.25f * 3.14159f / 2) : std::max(0.f, 3.5f * (1 - (phase[i] - 0.25f) / 0.15f));
            if (phase[i] > 0.4f) phase[i] = -1;
        }
        if (m_fxIo && m_engine.active()) {
            if (m_engine.needsAudio()) {
                // 120 BPM kick with a melody sweeping across the bands.
                fx::AudioFrame a;
                a.active = true;
                const double beat = std::fmod(secs, 0.5);
                a.bass = beat < 0.08 ? 1.0f : 0.25f;
                a.beats = uint32_t(secs / 0.5);
                a.level = beat < 0.1 ? 1.0f : 0.65f;
                a.treble = 0.4f;
                for (int b = 0; b < 14; ++b)
                    a.bands[b] = float(std::clamp(0.2 + 0.75 * std::exp(-std::pow(b - 7 - 5 * std::sin(secs * 1.7), 2) / 5.0) + (b < 3 ? a.bass * 0.6 : 0.0), 0.0, 1.0));
                for (int k = 0; k < 28; ++k)
                    a.wave[k] = float(0.85 * std::sin(k * 0.5 + secs * 11) * (0.6 + 0.4 * std::sin(k * 0.13 + secs * 2.3)));
                m_engine.setAudio(a);
            }
            const auto frame = m_engine.render(secs, fake);
            QVariantList cols;
            for (const auto &c : frame)
                cols << QColor::fromRgbF(c.r, c.g, c.b);
            std::vector<Rgb> rgb;
            for (const auto &c : frame)
                rgb.push_back(Rgb{uint8_t(c.r * 255), uint8_t(c.g * 255), uint8_t(c.b * 255)});
            onGui([this, cols, rgb] { m_liveColors = cols; m_lastFrame = rgb; m_fxFps = 50; emit liveColorsChanged(); });
        }
        if (m_liveIo) {
            QVariantList mm;
            for (float f : fake) mm << double(f);
            onGui([this, mm] { m_travel = mm; m_travelFps = 50; emit travelChanged(); });
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        return;
    }
    const auto frameStart = std::chrono::steady_clock::now();
    const bool fx = m_fxIo && m_engine.active();
    const bool wantTravel = m_liveIo || (fx && m_engine.needsTravel());
    std::vector<float> travelMm;
    if (wantTravel) {
        if (auto t = m_proto->travelAll()) {
            travelMm.reserve(kKeys);
            for (const auto &k : *t)
                travelMm.push_back(k.trip / 100.f);
        }
    }
    if (fx) {
        const double secs = std::chrono::duration<double>(frameStart - m_fxEpoch).count();
        streamFrame(m_engine.render(secs, travelMm));
    }
    if (wantTravel && m_liveIo && !travelMm.empty()) {
        QVariantList mm;
        mm.reserve(kKeys);
        for (float v : travelMm)
            mm << double(v);
        onGui([this, mm] {
            m_travel = mm;
            ++m_fpsFrames;
            if (!m_fpsClock.isValid())
                m_fpsClock.start();
            if (m_fpsClock.elapsed() >= 500) {
                m_travelFps = m_fpsFrames * 1000.0 / m_fpsClock.restart();
                m_fpsFrames = 0;
            }
            emit travelChanged();
        });
    }
    if (fx || wantTravel) {
        // Cap at ~50 fps so we don't hog the USB link or a CPU core.
        const auto spent = std::chrono::steady_clock::now() - frameStart;
        // Ambient effects at ~30 fps, reactive/audio at ~40 fps.
        const auto budget = std::chrono::milliseconds(m_engine.needsTravel() || m_engine.needsAudio() || wantTravel ? 25 : 33);
        if (spent < budget)
            std::this_thread::sleep_for(budget - spent);
    } else {
        m_proto->pumpEvents(40);
    }
    auto events = m_proto->takeEvents();
    if (!events.empty())
        onGui([this] {
            // Layer / WASD / Mac / Win-lock / NKRO toggled on the keyboard.
            runIo([this] {
                auto f = m_proto->features();
                auto l = m_proto->defaultLayer();
                onGui([this, f, l] {
                    if (f) m_features = *f;
                    if (l) m_defaultLayer = *l;
                    emit featuresChanged();
                });
            });
        });
}

void MadKeyboard::setLiveTravel(bool on)
{
    if (on == m_live)
        return;
    m_live = on;
    m_liveIo = on;
    m_fpsClock.invalidate();
    m_fpsFrames = 0;
    if (!on) {
        m_travelFps = 0;
        QVariantList zero;
        for (int i = 0; i < kKeys; ++i)
            zero << 0.0;
        m_travel = zero;
        emit travelChanged();
    }
    emit liveTravelChanged();
}

void MadKeyboard::write(std::function<bool(Protocol &)> fn, const QString &what)
{
    runIo([this, fn = std::move(fn), what] {
        const bool ok = m_proto && fn(*m_proto);
        if (!ok) {
            const QString err = m_proto ? QString::fromStdString(m_proto->lastError()) : QStringLiteral("not open");
            onGui([this, what, err] {
                setLastError(QStringLiteral("%1 didn't save (%2)").arg(what, err));
                refresh();
            });
        }
    });
}

std::vector<int> MadKeyboard::toIndices(const QVariantList &l) const
{
    std::vector<int> out;
    for (const auto &v : l) {
        const int i = v.toInt();
        if (i >= 0 && i < kKeys)
            out.push_back(i);
    }
    return out;
}

// ── Travel ───────────────────────────────────────────────────────────────

QVariantList MadKeyboard::actuation() const
{
    QVariantList out;
    out.reserve(kKeys);
    for (uint16_t v : m_act)
        out << v / 100.0;
    return out;
}

QVariantList MadKeyboard::rapidTrigger() const
{
    QVariantList out;
    out.reserve(kKeys);
    for (const auto &r : m_rt)
        out << QVariantMap{{QStringLiteral("on"), r.on}, {QStringLiteral("press"), r.press / 100.0}, {QStringLiteral("release"), r.release / 100.0}};
    return out;
}

void MadKeyboard::setActuation(const QVariantList &indices, double mm)
{
    const auto idx = toIndices(indices);
    if (idx.empty())
        return;
    const uint16_t raw = toRaw(std::clamp(mm, m_actMin, m_actMax));
    for (int i : idx)
        m_act[i] = raw;
    emit travelSettingsChanged();
    // Always rewrite all keys with the erase/write sequence, like the vendor app.
    auto all = m_act;
    write([all](Protocol &p) { return p.setActuationAll(all); }, QStringLiteral("Actuation point"));
}

void MadKeyboard::setRapidTrigger(const QVariantList &indices, bool on, double pressMm, double releaseMm)
{
    const auto idx = toIndices(indices);
    if (idx.empty())
        return;
    const RtInfo rt{on, toRaw(std::clamp(releaseMm, m_rtMin, m_rtMax)), toRaw(std::clamp(pressMm, m_rtMin, m_rtMax))};
    for (int i : idx)
        m_rt[i] = rt;
    emit travelSettingsChanged();
    auto all = m_rt;
    write([all](Protocol &p) { return p.setRtAll(all); }, QStringLiteral("Rapid Trigger"));
}

void MadKeyboard::setDeadZones(double topMm, double bottomMm)
{
    m_dead = DeadZones{toRaw(std::clamp(topMm, 0.0, m_dzMax)), toRaw(std::clamp(bottomMm, 0.0, m_dzMax))};
    emit travelSettingsChanged();
    const auto dz = m_dead;
    write([dz](Protocol &p) { return p.setDeadZones(dz); }, QStringLiteral("Dead zones"));
}

// ── Keymap ───────────────────────────────────────────────────────────────

QVariantList MadKeyboard::keymap() const
{
    QVariantList layers;
    for (int l = 0; l < kLayers; ++l) {
        QVariantList keys;
        keys.reserve(kKeys);
        for (int i = 0; i < kKeys; ++i)
            keys << int(m_keymap[l * kKeys + i]);
        layers << QVariant(keys);
    }
    return layers;
}

void MadKeyboard::setKeycode(int layer, int index, int keycode)
{
    if (layer < 0 || layer >= kLayers || index < 0 || index >= kKeys)
        return;
    const uint16_t fn = layer >= 2 ? 0x5223 : 0x5221;
    const bool hadFn = std::find(m_keymap.begin() + layer * kKeys, m_keymap.begin() + (layer + 1) * kKeys, fn) != m_keymap.begin() + (layer + 1) * kKeys;
    m_keymap[layer * kKeys + index] = uint16_t(keycode);
    emit keymapChanged();
    const bool hasFn = std::find(m_keymap.begin() + layer * kKeys, m_keymap.begin() + (layer + 1) * kKeys, fn) != m_keymap.begin() + (layer + 1) * kKeys;
    if ((layer == 0 || layer == 2) && hadFn && !hasFn)
        emit toast(QStringLiteral("Heads up: no Fn key left on this layer. Assign Fn somewhere to reach the Fn layer."), true);
    write([layer, index, keycode](Protocol &p) { return p.setKey(layer, index / kCols, index % kCols, uint16_t(keycode)); },
          QStringLiteral("Key"));
}

void MadKeyboard::resetKeymap()
{
    runIo([this] {
        const bool ok = m_proto->keymapReset();
        auto k = m_proto->keymapAll();
        onGui([this, ok, k] {
            if (k && k->size() == size_t(kLayers * kKeys))
                m_keymap = *k;
            emit keymapChanged();
            emit toast(ok ? QStringLiteral("Keymap restored to default") : QStringLiteral("Keymap reset failed"), !ok);
        });
    });
}

QString MadKeyboard::keyName(int index) const
{
    if (index < 0 || index >= kKeys)
        return {};
    return keycodeShort(m_keymap[index]);
}

QString MadKeyboard::keycodeShort(int kc) const
{
    if (kc == 0)
        return QString();
    if (kc == 1)
        return QStringLiteral("▽");
    if (kc == 0x5221 || kc == 0x5223)
        return QStringLiteral("Fn");
    if (kc == 0x5261 || kc == 0x5263)
        return QStringLiteral("Fn Lock");
    if (kc <= 0xFF) {
        const QString hid = KeyNames::hidShortName(kc);
        if (!hid.startsWith(QLatin1String("0x")))
            return hid;
        static const QHash<int, QString> media = {
            {0xA8, QStringLiteral("Mute")}, {0xA9, QStringLiteral("Vol+")}, {0xAA, QStringLiteral("Vol−")},
            {0xAB, QStringLiteral("Next")}, {0xAC, QStringLiteral("Prev")}, {0xAD, QStringLiteral("Stop")},
            {0xAE, QStringLiteral("Play")}, {0xAF, QStringLiteral("Media")}, {0xB0, QStringLiteral("Eject")},
            {0xB1, QStringLiteral("Mail")}, {0xB2, QStringLiteral("Calc")}, {0xB3, QStringLiteral("PC")},
            {0xB4, QStringLiteral("Search")}, {0xB5, QStringLiteral("Home")}, {0xB6, QStringLiteral("Back")},
            {0xB7, QStringLiteral("Fwd")}, {0xBD, QStringLiteral("Bri+")}, {0xBE, QStringLiteral("Bri−")},
            {0xA5, QStringLiteral("Power")}, {0xA6, QStringLiteral("Sleep")}, {0xA7, QStringLiteral("Wake")},
        };
        if (media.contains(kc))
            return media.value(kc);
    }
    return keycodeLabel(kc);
}

QString MadKeyboard::keycodeLabel(int kc) const
{
    if (kc == 0)
        return QStringLiteral("None");
    if (kc == 1)
        return QStringLiteral("Transparent");
    if (m_extraNames.contains(kc) && kc > 0xFF)
        return m_extraNames.value(kc);
    if (kc <= 0xFF) {
        if (KeyNames::hidName(kc).startsWith(QLatin1String("Key 0x")) && m_extraNames.contains(kc))
            return m_extraNames.value(kc);
        return KeyNames::hidName(kc);
    }
    const int hi = kc >> 8, lo = kc & 0xFF;
    if (kc < 0x2000)
        return modsText(hi & 0x1F) + QLatin1Char('+') + keycodeShort(lo);
    if (kc < 0x4000)
        return QStringLiteral("%1 / %2").arg(keycodeShort(lo), modsText((kc >> 8) & 0x1F));
    if (kc < 0x4400)
        return QStringLiteral("L%1 / %2").arg(hi & 0x03).arg(keycodeShort(lo));
    if (hi == 0x52) {
        const int op = lo & 0xE0, l = lo & 0x1F;
        switch (op) {
        case 0x00: return QStringLiteral("TO %1").arg(l);
        case 0x20: return l == 1 ? QStringLiteral("Fn") : l == 3 ? QStringLiteral("Mac Fn") : QStringLiteral("MO %1").arg(l);
        case 0x40: return QStringLiteral("DF %1").arg(l);
        case 0x60: return QStringLiteral("TG %1").arg(l);
        case 0x80: return QStringLiteral("OSL %1").arg(l);
        case 0xA0: return QStringLiteral("OSM %1").arg(modsText(l));
        case 0xC0: return QStringLiteral("TT %1").arg(l);
        }
    }
    if (hi == 0x57)
        return lo < 0x14 ? QStringLiteral("Mod-Tap %1").arg(lo + 1) : QStringLiteral("Toggle %1").arg(lo - 0x14 + 1);
    if (hi == 0x77)
        return macroName(lo);
    if (hi == 0x7E) {
        switch (lo) {
        case 0x01: return QStringLiteral("WASD ⇄ Arrows");
        case 0x02: return QStringLiteral("Win / Mac");
        case 0x03: return QStringLiteral("Win Lock");
        case 0x04: return QStringLiteral("NKRO / 6KRO");
        }
        if (lo >= 0x05 && lo < 0x05 + 20)
            return QStringLiteral("DKS %1").arg(lo - 0x05 + 1);
        return lo < 0x40 ? QStringLiteral("KB %1").arg(lo) : QStringLiteral("User %1").arg(lo - 0x40);
    }
    return QStringLiteral("0x%1").arg(kc, 4, 16, QLatin1Char('0'));
}

QVariantList MadKeyboard::keycodeGroups() const
{
    QVariantList groups;
    auto key = [](int code, const QString &name) { return QVariantMap{{QStringLiteral("code"), code}, {QStringLiteral("name"), name}}; };
    // Fn is handled inside the keyboard and never reaches the PC, so it can't
    // be "pressed to bind". Offer it (and the vendor toggles) up front.
    groups << QVariantMap{{QStringLiteral("title"), QStringLiteral("Fn & keyboard functions")},
                          {QStringLiteral("keys"), QVariantList{
                               key(0x5221, QStringLiteral("Fn")),
                               key(0x5223, QStringLiteral("Mac Fn")),
                               key(0x5261, QStringLiteral("Fn Lock")),
                               key(0x7E02, QStringLiteral("Win ⇄ Mac")),
                               key(0x7E01, QStringLiteral("WASD ⇄ Arrows")),
                               key(0x7E03, QStringLiteral("Win Lock")),
                               key(0x7E04, QStringLiteral("NKRO ⇄ 6KRO")),
                           }}};
    QHash<QString, QVariantList> byGroup;
    QStringList order;
    for (const auto &v : KeyNames::hidKeys()) {
        const QVariantMap k = v.toMap();
        const QString g = k.value(QStringLiteral("group")).toString();
        if (!byGroup.contains(g))
            order << g;
        byGroup[g] << QVariantMap{{QStringLiteral("code"), k.value(QStringLiteral("usage"))}, {QStringLiteral("name"), k.value(QStringLiteral("short"))}};
    }
    for (const auto &g : order)
        groups << QVariantMap{{QStringLiteral("title"), g}, {QStringLiteral("keys"), byGroup.value(g)}};

    QVariantList media;
    for (int c : {0xAE, 0xAB, 0xAC, 0xAD, 0xA8, 0xA9, 0xAA, 0xBD, 0xBE, 0xB2, 0xB3, 0xB1, 0xB4, 0xB5, 0xB6, 0xB7})
        media << QVariantMap{{QStringLiteral("code"), c}, {QStringLiteral("name"), keycodeShort(c)}};
    groups << QVariantMap{{QStringLiteral("title"), QStringLiteral("Media")}, {QStringLiteral("keys"), media}};

    QVariantList layers;
    for (int l = 1; l < kLayers; ++l)
        layers << QVariantMap{{QStringLiteral("code"), 0x5220 | l}, {QStringLiteral("name"), QStringLiteral("MO %1").arg(l)}};
    for (int l = 0; l < kLayers; ++l)
        layers << QVariantMap{{QStringLiteral("code"), 0x5260 | l}, {QStringLiteral("name"), QStringLiteral("TG %1").arg(l)}};
    for (int l = 0; l < kLayers; ++l)
        layers << QVariantMap{{QStringLiteral("code"), 0x5200 | l}, {QStringLiteral("name"), QStringLiteral("TO %1").arg(l)}};
    for (int l = 0; l < kLayers; ++l)
        layers << QVariantMap{{QStringLiteral("code"), 0x5240 | l}, {QStringLiteral("name"), QStringLiteral("DF %1").arg(l)}};
    groups << QVariantMap{{QStringLiteral("title"), QStringLiteral("Layers")}, {QStringLiteral("keys"), layers}};

    QVariantList light;
    for (auto it = m_extraNames.cbegin(); it != m_extraNames.cend(); ++it)
        if ((it.key() >> 8) == 0x78)
            light << QVariantMap{{QStringLiteral("code"), it.key()}, {QStringLiteral("name"), it.value()}};
    std::sort(light.begin(), light.end(), [](const QVariant &a, const QVariant &b) { return a.toMap().value(QStringLiteral("code")).toInt() < b.toMap().value(QStringLiteral("code")).toInt(); });
    if (!light.isEmpty())
        groups << QVariantMap{{QStringLiteral("title"), QStringLiteral("Lighting")}, {QStringLiteral("keys"), light}};

    QVariantList macros;
    for (int i = 0; i < m_macroCount; ++i)
        macros << QVariantMap{{QStringLiteral("code"), 0x7700 | i}, {QStringLiteral("name"), macroName(i)}};
    if (!macros.isEmpty())
        groups << QVariantMap{{QStringLiteral("title"), QStringLiteral("Macros")}, {QStringLiteral("keys"), macros}};

    QVariantList special{QVariantMap{{QStringLiteral("code"), 1}, {QStringLiteral("name"), QStringLiteral("Transparent")}},
                         QVariantMap{{QStringLiteral("code"), 0}, {QStringLiteral("name"), QStringLiteral("None")}}};
    groups << QVariantMap{{QStringLiteral("title"), QStringLiteral("Special")}, {QStringLiteral("keys"), special}};
    return groups;
}

// ── Features ─────────────────────────────────────────────────────────────

void MadKeyboard::writeFeatures()
{
    emit featuresChanged();
    const auto f = m_features;
    write([f](Protocol &p) { return p.setFeatures(f); }, QStringLiteral("Keyboard setting"));
}

void MadKeyboard::setWasdSwap(bool on) { if (m_features.wasdSwap != on) { m_features.wasdSwap = on; writeFeatures(); } }
void MadKeyboard::setMacMode(bool on) { if (m_features.macMode != on) { m_features.macMode = on; writeFeatures(); } }
void MadKeyboard::setWinLock(bool on) { if (m_features.winLock != on) { m_features.winLock = on; writeFeatures(); } }
void MadKeyboard::setNkro(bool on) { if (m_features.nkro != on) { m_features.nkro = on; writeFeatures(); } }

void MadKeyboard::setBottomOptimize(bool on)
{
    if (on == m_bottomOptimize)
        return;
    m_bottomOptimize = on;
    emit featuresChanged();
    write([on](Protocol &p) { return p.setBottomOptimize(on); }, QStringLiteral("Bottom-out trigger"));
}

void MadKeyboard::setGameMode(bool on)
{
    if (on == m_gameMode)
        return;
    m_gameMode = on;
    emit featuresChanged();
    write([on](Protocol &p) { return p.setGameMode(on); }, QStringLiteral("Competitive mode"));
}

// ── Lighting ─────────────────────────────────────────────────────────────

QVariantList MadKeyboard::keyColors() const
{
    QVariantList out;
    out.reserve(kKeys);
    for (const auto &c : m_colors)
        out << QColor(c.r, c.g, c.b);
    return out;
}

void MadKeyboard::setLighting(int effect, int speed, int brightness, const QColor &color)
{
    if (!m_fxId.isEmpty())
        stopFluxEffect();
    m_light.effect = uint8_t(effect);
    m_light.speed = uint8_t(std::clamp(speed, 0, m_speedMax));
    m_light.brightness = uint8_t(std::clamp(brightness, 0, m_brightnessMax));
    m_light.r = uint8_t(color.red());
    m_light.g = uint8_t(color.green());
    m_light.b = uint8_t(color.blue());
    emit lightingChanged();
    const auto l = m_light;
    write([l](Protocol &p) { return p.setLighting(l); }, QStringLiteral("Lighting"));
}

void MadKeyboard::setKeyColor(const QVariantList &indices, const QColor &color)
{
    const auto idx = toIndices(indices);
    if (idx.empty())
        return;
    const Rgb c{uint8_t(color.red()), uint8_t(color.green()), uint8_t(color.blue())};
    for (int i : idx)
        m_colors[i] = c;
    emit lightingChanged();
    if (idx.size() > 6) {
        auto all = m_colors;
        auto present = m_present;
        write([all, present](Protocol &p) { return p.setKeyColors(all, present); }, QStringLiteral("Key colours"));
    } else {
        write([idx, c](Protocol &p) {
            for (int i : idx)
                if (!p.setKeyColor(i / kCols, i % kCols, c))
                    return false;
            return p.saveKeyColors();
        }, QStringLiteral("Key colours"));
    }
}

// ── Side light ───────────────────────────────────────────────────────────

void MadKeyboard::setBoxLight(int mode, bool rainbow, int brightness, int speed, const QColor &color)
{
    m_box = BoxLight{uint8_t(std::clamp(mode, 0, 4)), uint8_t(rainbow), uint8_t(std::clamp(brightness, 0, 160)), uint8_t(std::clamp(speed, 0, 40)),
                     uint8_t(color.red()), uint8_t(color.green()), uint8_t(color.blue())};
    emit lightingChanged();
    const auto b = m_box;
    write([b](Protocol &p) { return p.setBoxLight(b); }, QStringLiteral("Side light"));
}

// ── Flux live lighting ───────────────────────────────────────────────────

QVariantList MadKeyboard::fluxEffects() const
{
    QVariantList out;
    for (const auto &e : fx::effects()) {
        QVariantList pal;
        for (uint32_t c : e.palette)
            pal << QColor::fromRgb(c);
        out << QVariantMap{{QStringLiteral("id"), QString::fromLatin1(e.id)},
                           {QStringLiteral("name"), QString::fromLatin1(e.name)},
                           {QStringLiteral("blurb"), QString::fromUtf8(e.blurb)},
                           {QStringLiteral("reactive"), e.usesTravel},
                           {QStringLiteral("audio"), e.usesAudio},
                           {QStringLiteral("palette"), pal}};
    }
    return out;
}

QVariantList MadKeyboard::fluxPalette() const
{
    return {m_fxPalette[0], m_fxPalette[1], m_fxPalette[2]};
}

void MadKeyboard::resetFluxPalette()
{
    for (const auto &e : fx::effects())
        if (m_fxId == QLatin1String(e.id) || m_fxId.isEmpty()) {
            for (int i = 0; i < 3; ++i)
                m_fxPalette[i] = QColor::fromRgb(e.palette[i]);
            break;
        }
    pushFluxParams();
    saveFluxSettings();
    emit fluxChanged();
}

void MadKeyboard::updateAudioTap()
{
    if (!m_fxId.isEmpty() && m_engine.needsAudio()) {
        m_audioTap.setSink([this](const AudioTap::Frame &f) {
            fx::AudioFrame a;
            a.bands = f.bands;
            a.wave = f.wave;
            a.level = f.level;
            a.bass = f.bass;
            a.treble = f.treble;
            a.beats = f.beats;
            a.active = f.active;
            m_engine.setAudio(a);
        });
        m_audioTap.start();
    } else {
        m_audioTap.stop();
    }
}

void MadKeyboard::pushFluxParams()
{
    fx::Params p;
    p.effect = m_fxIo || !m_fxId.isEmpty() ? m_fxId.toStdString() : std::string();
    p.speed = float(m_fxSpeed);
    p.intensity = float(m_fxIntensity);
    for (int i = 0; i < 3; ++i)
        p.palette[i] = {float(m_fxPalette[i].redF()), float(m_fxPalette[i].greenF()), float(m_fxPalette[i].blueF())};
    // Reactive effects trigger a bit before the actuation point so they feel instant.
    double sum = 0;
    for (uint16_t a : m_act)
        sum += a;
    p.actuationMm = float(std::max(0.4, sum / std::max<size_t>(1, m_act.size()) / 100.0 * 0.8));
    m_engine.setParams(p);
}

void MadKeyboard::saveFluxSettings() const
{
    QSettings st(QStringLiteral("flux"), flux::settingsApp());
    st.setValue(QStringLiteral("keyboard-fx/effect"), m_fxId);
    st.setValue(QStringLiteral("keyboard-fx/speed"), m_fxSpeed);
    st.setValue(QStringLiteral("keyboard-fx/intensity"), m_fxIntensity);
    if (!m_fxId.isEmpty())
        st.setValue(QStringLiteral("keyboard-fx/palette/%1").arg(m_fxId),
                    QStringList{m_fxPalette[0].name(), m_fxPalette[1].name(), m_fxPalette[2].name()});
}

void MadKeyboard::startFluxEffect(const QString &id)
{
    const bool switching = !m_fxIo;
    if (id != m_fxId || switching) {
        // Load the remembered palette for this effect (or its defaults).
        const QString prev = m_fxId;
        m_fxId = id;
        const QStringList pal = QSettings(QStringLiteral("flux"), flux::settingsApp())
                                    .value(QStringLiteral("keyboard-fx/palette/%1").arg(id)).toStringList();
        if (pal.size() == 3) {
            for (int i = 0; i < 3; ++i)
                m_fxPalette[i] = QColor(pal[i]);
        } else if (prev != id) {
            for (const auto &e : fx::effects())
                if (id == QLatin1String(e.id))
                    for (int i = 0; i < 3; ++i)
                        m_fxPalette[i] = QColor::fromRgb(e.palette[i]);
        }
    }
    pushFluxParams();
    saveFluxSettings();
    if (switching && demo()) {
        m_lightBeforeFx = m_light;
        m_fxEpoch = std::chrono::steady_clock::now();
        m_fxIo = true;
    } else if (switching) {
        m_lightBeforeFx = m_light.effect == 1 && m_lightBeforeFx.effect != 0 ? m_lightBeforeFx : m_light;
        // Per-key mode at a safe hardware brightness (vendor warns above 60 %).
        Lighting l = m_light;
        l.effect = 1;
        l.brightness = 120;
        m_fxFrames = 0;
        m_fxClock.invalidate();
        runIo([this, l] {
            m_proto->setLighting(l);
            m_lastSent.assign(kKeys, Rgb{1, 2, 3}); // force a full first frame
            m_fxEpoch = std::chrono::steady_clock::now();
            m_fxIo = true;
        });
    }
    updateAudioTap();
    emit fluxChanged();
}

void MadKeyboard::stopFluxEffect()
{
    if (m_fxId.isEmpty() && !m_fxIo)
        return;
    m_fxId.clear();
    saveFluxSettings();
    m_engine.setParams(fx::Params{});
    m_fxFps = 0;
    const auto light = m_lightBeforeFx.effect == 1 && m_light.effect != 1 ? m_light : m_lightBeforeFx;
    const auto colors = m_colors;
    runIo([this, light, colors] {
        m_fxIo = false;
        restoreAfterFlux(colors);
        m_proto->setLighting(light);
    });
    m_light = light;
    updateAudioTap();
    emit fluxChanged();
    emit lightingChanged();
}

void MadKeyboard::restoreAfterFlux(const std::vector<Rgb> &colors)
{
    // Put the stored per-key design back into RAM (it is unchanged in flash).
    m_proto->setKeyColors(colors, m_present, false);
    m_lastSent.clear();
}

void MadKeyboard::setFluxSpeed(double s)
{
    m_fxSpeed = std::clamp(s, 0.2, 3.0);
    pushFluxParams();
    saveFluxSettings();
    emit fluxChanged();
}

void MadKeyboard::setFluxIntensity(double i)
{
    m_fxIntensity = std::clamp(i, 0.05, 1.0);
    pushFluxParams();
    saveFluxSettings();
    emit fluxChanged();
}

void MadKeyboard::setFluxPaletteColor(int index, const QColor &c)
{
    if (index < 0 || index > 2)
        return;
    m_fxPalette[index] = c;
    pushFluxParams();
    saveFluxSettings();
    emit fluxChanged();
}

void MadKeyboard::streamFrame(const std::vector<fx::Color> &frame)
{
    // Temporal smoothing kills frame-to-frame dithering. Ambient effects get
    // more of it; reactive/audio ones stay snappy.
    if (m_smooth.size() != size_t(kKeys * 3))
        m_smooth.assign(kKeys * 3, 0.f);
    const bool snappy = m_engine.needsTravel() || m_engine.needsAudio();
    const float alpha = snappy ? 0.6f : 0.35f;
    std::vector<Rgb> rgb(kKeys);
    for (int i = 0; i < kKeys && i < int(frame.size()); ++i) {
        const float in[3] = {frame[i].r, frame[i].g, frame[i].b};
        uint8_t out[3];
        for (int c = 0; c < 3; ++c) {
            float &s = m_smooth[i * 3 + c];
            s += (in[c] - s) * alpha;
            int v = int(std::lround(s * 255));
            if (v < 3)
                v = 0; // below this LEDs flicker between off and barely-on
            out[c] = uint8_t(std::clamp(v, 0, 255));
        }
        rgb[i] = Rgb{out[0], out[1], out[2]};
    }
    if (m_lastSent.size() != size_t(kKeys))
        m_lastSent.assign(kKeys, Rgb{1, 2, 3});
    auto differs = [](const Rgb &a, const Rgb &b) {
        // Skip changes too small to see (relative dead-band), but always
        // send a change to/from black.
        const bool blackA = !a.r && !a.g && !a.b, blackB = !b.r && !b.g && !b.b;
        auto big = [](int x, int y) { return std::abs(x - y) > std::max(2, std::max(x, y) / 14); };
        return blackA != blackB || big(a.r, b.r) || big(a.g, b.g) || big(a.b, b.b);
    };
    // One key per packet, only keys that changed. (8-key packets spill past
    // the end of a row onto the next row's first keys on this firmware.)
    for (int i = 0; i < kKeys; ++i) {
        if (!m_present[i] || !differs(rgb[i], m_lastSent[i]))
            continue;
        if (!m_proto->setKeyColor(i / kCols, i % kCols, rgb[i]))
            return;
        m_lastSent[i] = rgb[i];
    }
    // Throttled preview for the UI (~25 fps).
    static thread_local int every = 0;
    if (++every % 2)
        return;
    onGui([this, rgb] {
        m_lastFrame = rgb;
        QVariantList cols;
        cols.reserve(kKeys);
        for (const auto &c : rgb)
            cols << QColor(c.r, c.g, c.b);
        m_liveColors = cols;
        ++m_fxFrames;
        if (!m_fxClock.isValid())
            m_fxClock.start();
        if (m_fxClock.elapsed() >= 1000) {
            m_fxFps = m_fxFrames * 2 * 1000.0 / m_fxClock.restart();
            m_fxFrames = 0;
        }
        emit liveColorsChanged();
    });
}

void MadKeyboard::bakeFluxFrame()
{
    if (m_lastFrame.size() != size_t(kKeys)) {
        emit toast(QStringLiteral("Start an effect first, then bake the frame you like"), true);
        return;
    }
    const auto frame = m_lastFrame;
    stopFluxEffect();
    m_colors = frame;
    m_light.effect = 1;
    m_lightBeforeFx = m_light;
    emit lightingChanged();
    const auto l = m_light;
    runIo([this, frame, l] {
        const bool ok = m_proto->setKeyColors(frame, m_present) && m_proto->setLighting(l);
        onGui([this, ok] { emit toast(ok ? QStringLiteral("Saved to the keyboard. It stays without Flux running.") : QStringLiteral("Couldn't save the design"), !ok); });
    });
}

// ── Advanced keys ────────────────────────────────────────────────────────

void MadKeyboard::loadAdvanced()
{
    if (demo()) {
        m_socd.assign(socdSlotCount(), SocdSlot{});
        m_socd[0] = SocdSlot{2, 0, true, false, 2, 1, 2, 3, 0};
        m_dks.assign(dksSlotCount(), std::array<DksAction, 4>{});
        m_td.assign(tapDanceSlotCount(), TapDance{});
        m_advancedLoaded = true;
        emit advancedChanged();
        return;
    }
    const int ns = socdSlotCount(), nd = dksSlotCount(), nt = tapDanceSlotCount();
    runIo([this, ns, nd, nt] {
        std::vector<SocdSlot> socd;
        std::vector<std::array<DksAction, 4>> dks;
        std::vector<TapDance> td;
        for (int i = 0; i < ns; ++i)
            socd.push_back(m_proto->socd(i).value_or(SocdSlot{}));
        for (int i = 0; i < nd; ++i)
            dks.push_back(m_proto->dks(i).value_or(std::array<DksAction, 4>{}));
        for (int i = 0; i < nt; ++i)
            td.push_back(m_proto->tapDance(i).value_or(TapDance{}));
        onGui([this, socd, dks, td] {
            m_socd = socd;
            m_dks = dks;
            m_td = td;
            m_advancedLoaded = true;
            emit advancedChanged();
        });
    });
}

QVariantList MadKeyboard::socdSlots() const
{
    QVariantList out;
    for (const auto &s : m_socd)
        out << QVariantMap{{QStringLiteral("mode"), s.mode},
                           {QStringLiteral("level"), s.level / 100.0},
                           {QStringLiteral("globalApc"), s.globalApc},
                           {QStringLiteral("useRt"), s.useRt},
                           {QStringLiteral("key1"), s.k1Row * kCols + s.k1Col},
                           {QStringLiteral("key2"), s.k2Row * kCols + s.k2Col},
                           {QStringLiteral("layer"), s.layer}};
    return out;
}

void MadKeyboard::setSocd(int slot, const QVariantMap &m)
{
    if (slot < 0 || slot >= int(m_socd.size()))
        return;
    SocdSlot s;
    s.mode = uint8_t(m.value(QStringLiteral("mode")).toInt());
    s.level = toRaw(m.value(QStringLiteral("level"), 0.0).toDouble());
    s.globalApc = m.value(QStringLiteral("globalApc"), true).toBool();
    s.useRt = m.value(QStringLiteral("useRt")).toBool();
    const int k1 = m.value(QStringLiteral("key1")).toInt(), k2 = m.value(QStringLiteral("key2")).toInt();
    s.k1Row = uint8_t(k1 / kCols);
    s.k1Col = uint8_t(k1 % kCols);
    s.k2Row = uint8_t(k2 / kCols);
    s.k2Col = uint8_t(k2 % kCols);
    s.layer = uint8_t(m.value(QStringLiteral("layer")).toInt());
    if (s.mode == 0)
        s = SocdSlot{};
    m_socd[slot] = s;
    emit advancedChanged();
    write([slot, s](Protocol &p) { return p.setSocd(slot, s); }, QStringLiteral("SOCD / Rappy Snappy"));
}

QVariantList MadKeyboard::dksSlots() const
{
    QVariantList out;
    for (const auto &slot : m_dks) {
        QVariantList acts;
        for (const auto &a : slot)
            acts << QVariantMap{{QStringLiteral("mask"), a.mask}, {QStringLiteral("keycode"), a.keycode}};
        out << QVariant(acts);
    }
    return out;
}

void MadKeyboard::setDks(int slot, const QVariantList &actions)
{
    if (slot < 0 || slot >= int(m_dks.size()))
        return;
    std::array<DksAction, 4> a{};
    for (int k = 0; k < 4 && k < actions.size(); ++k) {
        const QVariantMap m = actions[k].toMap();
        a[k] = DksAction{uint16_t(m.value(QStringLiteral("mask")).toInt() & 0x3FF), uint16_t(m.value(QStringLiteral("keycode")).toInt())};
    }
    m_dks[slot] = a;
    emit advancedChanged();
    write([slot, a](Protocol &p) { return p.setDks(slot, a); }, QStringLiteral("Dynamic keystroke"));
}

QVariantList MadKeyboard::tapDances() const
{
    QVariantList out;
    for (const auto &t : m_td)
        out << QVariantMap{{QStringLiteral("tap"), t.tap}, {QStringLiteral("hold"), t.hold}, {QStringLiteral("term"), t.termMs}};
    return out;
}

void MadKeyboard::setTapDance(int slot, int tap, int hold, int termMs)
{
    if (slot < 0 || slot >= int(m_td.size()))
        return;
    const TapDance td{uint16_t(tap), uint16_t(hold), uint16_t(std::clamp(termMs, 10, 1000))};
    m_td[slot] = td;
    emit advancedChanged();
    write([slot, td](Protocol &p) { return p.setTapDance(slot, td); }, QStringLiteral("Mod-tap"));
}

// ── Macros ───────────────────────────────────────────────────────────────

std::vector<QVariantList> MadKeyboard::parseMacros(const std::vector<uint8_t> &buf, int count)
{
    std::vector<QVariantList> out(std::max(0, count));
    int m = 0;
    size_t i = 0;
    QString text;
    auto flushText = [&] {
        if (!text.isEmpty() && m < count) {
            out[m] << QVariantMap{{QStringLiteral("type"), QStringLiteral("text")}, {QStringLiteral("text"), text}};
            text.clear();
        }
    };
    while (i < buf.size() && m < count) {
        const uint8_t b = buf[i];
        if (b == 0x00) {
            flushText();
            ++m;
            ++i;
            continue;
        }
        if (b == 0x01 && i + 1 < buf.size()) {
            flushText();
            const uint8_t t = buf[i + 1];
            if ((t == 1 || t == 2 || t == 3) && i + 2 < buf.size()) {
                const int kc = buf[i + 2];
                if (t == 1)
                    out[m] << QVariantMap{{QStringLiteral("type"), QStringLiteral("tap")}, {QStringLiteral("usage"), kc}};
                else
                    out[m] << QVariantMap{{QStringLiteral("type"), QStringLiteral("key")}, {QStringLiteral("usage"), kc}, {QStringLiteral("down"), t == 2}};
                i += 3;
            } else if (t == 4 && i + 3 < buf.size()) {
                const int ms = (buf[i + 2] - 1) + 256 * (buf[i + 3] - 1);
                // Merge consecutive delays (we split long ones when writing).
                if (!out[m].isEmpty() && out[m].last().toMap().value(QStringLiteral("type")) == QLatin1String("delay")) {
                    QVariantMap last = out[m].takeLast().toMap();
                    last[QStringLiteral("ms")] = last.value(QStringLiteral("ms")).toInt() + ms;
                    out[m] << last;
                } else {
                    out[m] << QVariantMap{{QStringLiteral("type"), QStringLiteral("delay")}, {QStringLiteral("ms"), ms}};
                }
                i += 4;
            } else if ((t == 5 || t == 6 || t == 7) && i + 3 < buf.size()) {
                const int kc = buf[i + 2] | (buf[i + 3] << 8);
                if (t == 5)
                    out[m] << QVariantMap{{QStringLiteral("type"), QStringLiteral("tap")}, {QStringLiteral("usage"), kc}};
                else
                    out[m] << QVariantMap{{QStringLiteral("type"), QStringLiteral("key")}, {QStringLiteral("usage"), kc}, {QStringLiteral("down"), t == 6}};
                i += 4;
            } else {
                i += 2;
            }
            continue;
        }
        text.append(QChar(b));
        ++i;
    }
    flushText();
    return out;
}

std::vector<uint8_t> MadKeyboard::encodeMacro(const QVariantList &events)
{
    std::vector<uint8_t> out;
    auto keyOp = [&](int type, int kc) {
        if (kc <= 0xFF && kc > 0) {
            out.insert(out.end(), {0x01, uint8_t(type), uint8_t(kc)});
        } else if ((kc & 0xFF) != 0 && (kc >> 8) != 0) {
            out.insert(out.end(), {0x01, uint8_t(type + 4), uint8_t(kc & 0xFF), uint8_t(kc >> 8)});
        }
    };
    for (const auto &v : events) {
        const QVariantMap e = v.toMap();
        const QString t = e.value(QStringLiteral("type")).toString();
        if (t == QLatin1String("tap")) {
            keyOp(1, e.value(QStringLiteral("usage")).toInt());
        } else if (t == QLatin1String("key")) {
            keyOp(e.value(QStringLiteral("down")).toBool() ? 2 : 3, e.value(QStringLiteral("usage")).toInt());
        } else if (t == QLatin1String("delay")) {
            // Steps ≤ 254 ms decode identically with base 255 and 256 firmware.
            int ms = std::max(0, e.value(QStringLiteral("ms")).toInt());
            while (ms > 0) {
                const int step = std::min(ms, 254);
                out.insert(out.end(), {0x01, 0x04, uint8_t(step + 1), 0x01});
                ms -= step;
            }
        } else if (t == QLatin1String("text")) {
            for (QChar c : e.value(QStringLiteral("text")).toString())
                if (c.unicode() >= 0x20 && c.unicode() < 0x7F)
                    out.push_back(uint8_t(c.unicode()));
        }
    }
    return out;
}

QVariantList MadKeyboard::macros() const
{
    QVariantList out;
    for (int i = 0; i < int(m_macros.size()); ++i)
        out << QVariantMap{{QStringLiteral("id"), i}, {QStringLiteral("name"), macroName(i)}, {QStringLiteral("events"), m_macros[i]},
                           {QStringLiteral("empty"), m_macros[i].isEmpty()}};
    return out;
}

QString MadKeyboard::macroName(int index) const
{
    QSettings s(QStringLiteral("flux"), flux::settingsApp());
    const QString n = s.value(QStringLiteral("mad-macros/%1").arg(index)).toString();
    return n.isEmpty() ? QStringLiteral("Macro %1").arg(index + 1) : n;
}

void MadKeyboard::writeMacroBuffer()
{
    std::vector<uint8_t> buf;
    for (const auto &m : m_macros) {
        const auto enc = encodeMacro(m);
        buf.insert(buf.end(), enc.begin(), enc.end());
        buf.push_back(0x00);
    }
    if (int(buf.size()) > m_macroBufSize) {
        emit toast(QStringLiteral("Macros are too long for the keyboard's memory (%1 / %2 bytes)").arg(buf.size()).arg(m_macroBufSize), true);
        return;
    }
    write([buf](Protocol &p) { return p.setMacroBuffer(buf); }, QStringLiteral("Macros"));
}

void MadKeyboard::saveMacro(int index, const QString &name, const QVariantList &events)
{
    if (index < 0 || index >= m_macroCount)
        return;
    if (int(m_macros.size()) < m_macroCount)
        m_macros.resize(m_macroCount);
    QSettings(QStringLiteral("flux"), flux::settingsApp()).setValue(QStringLiteral("mad-macros/%1").arg(index), name);
    // Normalise through encode/parse so the UI shows what the keyboard stores.
    m_macros[index] = parseMacros([&] { auto e = encodeMacro(events); e.push_back(0); return e; }(), 1).front();
    emit macrosChanged();
    emit keymapChanged();
    writeMacroBuffer();
}

void MadKeyboard::deleteMacro(int index)
{
    if (index < 0 || index >= int(m_macros.size()))
        return;
    m_macros[index].clear();
    QSettings(QStringLiteral("flux"), flux::settingsApp()).remove(QStringLiteral("mad-macros/%1").arg(index));
    emit macrosChanged();
    writeMacroBuffer();
}

// ── Calibration / reset ──────────────────────────────────────────────────

void MadKeyboard::startCalibration()
{
    m_calibrating = true;
    emit calibratingChanged();
    setLiveTravel(true);
    write([](Protocol &p) { return p.calibrationStart(); }, QStringLiteral("Calibration"));
}

void MadKeyboard::finishCalibration()
{
    runIo([this] {
        const bool ok = m_proto->calibrationFinish();
        onGui([this, ok] {
            m_calibrating = false;
            emit calibratingChanged();
            emit toast(ok ? QStringLiteral("Calibration saved") : QStringLiteral("Calibration failed"), !ok);
        });
    });
}

void MadKeyboard::factoryReset()
{
    runIo([this] {
        const bool ok = m_proto->factoryReset();
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        onGui([this, ok] {
            emit toast(ok ? QStringLiteral("Keyboard restored to factory settings") : QStringLiteral("Factory reset failed"), !ok);
            refresh();
        });
    });
}

// ── Presets ──────────────────────────────────────────────────────────────

QVariantList MadKeyboard::indicesForKeycodes(const QVariantList &codes) const
{
    QVariantList out;
    for (const auto &c : codes) {
        const int code = c.toInt();
        for (int i = 0; i < kKeys; ++i)
            if (m_keymap[i] == code) {
                out << i;
                break;
            }
    }
    return out;
}

QString MadKeyboard::applyOsuPreset(const QString &variant)
{
    QVariantList codes;
    QString keysText;
    if (variant == QLatin1String("mania4")) {
        codes = {0x07, 0x09, 0x0D, 0x0E};
        keysText = QStringLiteral("D F J K");
    } else if (variant == QLatin1String("mania7")) {
        codes = {0x16, 0x07, 0x09, 0x2C, 0x0D, 0x0E, 0x0F};
        keysText = QStringLiteral("S D F Space J K L");
    } else {
        codes = {0x04, 0x07};
        keysText = QStringLiteral("A D");
    }
    const QVariantList idx = indicesForKeycodes(codes);
    if (idx.isEmpty()) {
        emit toast(QStringLiteral("Couldn't find %1 on the base layer").arg(keysText), true);
        return {};
    }
    // Tuned for tapping: hair-trigger actuation, very tight RT, slightly
    // larger release so a relaxed finger doesn't drop slider holds.
    setGameMode(true);
    for (const auto &v : idx) {
        const int i = v.toInt();
        m_act[i] = 15;                       // 0.15 mm
        m_rt[i] = RtInfo{true, 8, 5};        // release 0.08 mm, press 0.05 mm
    }
    emit travelSettingsChanged();
    auto act = m_act;
    auto rt = m_rt;
    write([act, rt](Protocol &p) { return p.setActuationAll(act) && p.setRtAll(rt); }, QStringLiteral("osu! preset"));
    emit toast(QStringLiteral("osu! preset applied to %1").arg(keysText), false);
    return keysText;
}

// ── osu! mode (switchable) ────────────────────────────────────────────────

static QVariantList codesFor(const QString &variant)
{
    if (variant == QLatin1String("mania4"))
        return {0x07, 0x09, 0x0D, 0x0E};
    if (variant == QLatin1String("mania7"))
        return {0x16, 0x07, 0x09, 0x2C, 0x0D, 0x0E, 0x0F};
    return {0x04, 0x07}; // A D
}

QVariantList MadKeyboard::osuKeys() const
{
    if (!m_osuKeys.isEmpty()) {
        QVariantList out;
        for (int i : m_osuKeys)
            out << i;
        return out;
    }
    return indicesForKeycodes(codesFor(QStringLiteral("std")));
}

QString MadKeyboard::osuKeysText() const
{
    QStringList names;
    for (const auto &v : osuKeys())
        names << keyName(v.toInt());
    return names.join(QLatin1Char(' '));
}

void MadKeyboard::setOsuKeys(const QVariantList &indices)
{
    const bool wasOn = m_osuMode;
    if (wasOn)
        setOsuMode(false); // restore, then re-apply to the new keys
    m_osuKeys.clear();
    for (const auto &v : indices)
        if (v.toInt() >= 0 && v.toInt() < kKeys)
            m_osuKeys << v.toInt();
    QSettings st(QStringLiteral("flux"), flux::settingsApp());
    st.setValue(QStringLiteral("keyboard-osu/keys"), QVariant(indices));
    emit osuChanged();
    if (wasOn)
        setOsuMode(true);
}

void MadKeyboard::setOsuKeySet(const QString &variant)
{
    setOsuKeys(indicesForKeycodes(codesFor(variant)));
}

void MadKeyboard::setOsuMode(bool on)
{
    if (on == m_osuMode)
        return;
    QSettings st(QStringLiteral("flux"), flux::settingsApp());
    if (on) {
        const QVariantList keys = osuKeys();
        if (keys.isEmpty()) {
            emit toast(QStringLiteral("Pick the keys you tap with first"), true);
            emit osuChanged();
            return;
        }
        // Remember exactly what to put back.
        QVariantList act, rt;
        for (int i = 0; i < kKeys; ++i) {
            act << int(m_act[i]);
            rt << QVariantList{int(m_rt[i].on), int(m_rt[i].release), int(m_rt[i].press)};
        }
        st.setValue(QStringLiteral("keyboard-osu/saved/actuation"), act);
        st.setValue(QStringLiteral("keyboard-osu/saved/rt"), rt);
        st.setValue(QStringLiteral("keyboard-osu/saved/gameMode"), m_gameMode);
        for (const auto &v : keys) {
            const int i = v.toInt();
            m_act[i] = 15;                  // 0.15 mm
            m_rt[i] = RtInfo{true, 8, 5};   // release 0.08, press 0.05 mm
        }
        if (!m_gameMode)
            setGameMode(true);
        m_osuMode = true;
        emit toast(QStringLiteral("osu! mode on · %1").arg(osuKeysText()), false);
    } else {
        const QVariantList act = st.value(QStringLiteral("keyboard-osu/saved/actuation")).toList();
        const QVariantList rt = st.value(QStringLiteral("keyboard-osu/saved/rt")).toList();
        if (act.size() == kKeys && rt.size() == kKeys) {
            for (int i = 0; i < kKeys; ++i) {
                m_act[i] = uint16_t(act[i].toInt());
                const QVariantList r = rt[i].toList();
                if (r.size() == 3)
                    m_rt[i] = RtInfo{r[0].toBool(), uint16_t(r[1].toInt()), uint16_t(r[2].toInt())};
            }
        }
        setGameMode(st.value(QStringLiteral("keyboard-osu/saved/gameMode"), m_gameMode).toBool());
        m_osuMode = false;
        emit toast(QStringLiteral("osu! mode off · settings restored"), false);
    }
    st.setValue(QStringLiteral("keyboard-osu/on"), m_osuMode);
    emit travelSettingsChanged();
    emit osuChanged();
    auto actAll = m_act;
    auto rtAll = m_rt;
    write([actAll, rtAll](Protocol &p) { return p.setActuationAll(actAll) && p.setRtAll(rtAll); }, QStringLiteral("osu! mode"));
}

// ── Profiles ─────────────────────────────────────────────────────────────

QVariantMap MadKeyboard::exportSettings() const
{
    QVariantList macros;
    for (int i = 0; i < int(m_macros.size()); ++i)
        macros << QVariantMap{{QStringLiteral("name"), macroName(i)}, {QStringLiteral("events"), m_macros[i]}};
    return {
        {QStringLiteral("device"), QStringLiteral("mad-light-60-he")},
        {QStringLiteral("actuation"), actuation()},
        {QStringLiteral("rapidTrigger"), rapidTrigger()},
        {QStringLiteral("deadZoneTop"), deadZoneTop()},
        {QStringLiteral("deadZoneBottom"), deadZoneBottom()},
        {QStringLiteral("keymap"), keymap()},
        {QStringLiteral("wasdSwap"), wasdSwap()},
        {QStringLiteral("winLock"), winLock()},
        {QStringLiteral("nkro"), nkro()},
        {QStringLiteral("bottomOptimize"), bottomOptimize()},
        {QStringLiteral("gameMode"), gameMode()},
        {QStringLiteral("lighting"), QVariantMap{{QStringLiteral("effect"), effect()}, {QStringLiteral("speed"), speed()},
                                                 {QStringLiteral("brightness"), brightness()}, {QStringLiteral("color"), lightColor().name()}}},
        {QStringLiteral("keyColors"), [this] { QVariantList l; for (const auto &c : m_colors) l << QColor(c.r, c.g, c.b).name(); return l; }()},
        {QStringLiteral("socd"), socdSlots()},
        {QStringLiteral("macros"), macros},
    };
}

void MadKeyboard::importSettings(const QVariantMap &s)
{
    if (s.value(QStringLiteral("device")).toString() != QLatin1String("mad-light-60-he")) {
        emit toast(QStringLiteral("That profile is for a different device"), true);
        return;
    }
    const QVariantList act = s.value(QStringLiteral("actuation")).toList();
    if (act.size() == kKeys) {
        for (int i = 0; i < kKeys; ++i)
            m_act[i] = toRaw(act[i].toDouble());
        auto all = m_act;
        write([all](Protocol &p) { return p.setActuationAll(all); }, QStringLiteral("Actuation point"));
    }
    const QVariantList rt = s.value(QStringLiteral("rapidTrigger")).toList();
    if (rt.size() == kKeys) {
        for (int i = 0; i < kKeys; ++i) {
            const QVariantMap m = rt[i].toMap();
            m_rt[i] = RtInfo{m.value(QStringLiteral("on")).toBool(), toRaw(m.value(QStringLiteral("release")).toDouble()),
                             toRaw(m.value(QStringLiteral("press")).toDouble())};
        }
        auto all = m_rt;
        write([all](Protocol &p) { return p.setRtAll(all); }, QStringLiteral("Rapid Trigger"));
    }
    setDeadZones(s.value(QStringLiteral("deadZoneTop"), deadZoneTop()).toDouble(), s.value(QStringLiteral("deadZoneBottom"), deadZoneBottom()).toDouble());

    const QVariantList km = s.value(QStringLiteral("keymap")).toList();
    if (km.size() == kLayers) {
        std::vector<std::tuple<int, int, uint16_t>> changes;
        for (int l = 0; l < kLayers; ++l) {
            const QVariantList keys = km[l].toList();
            for (int i = 0; i < kKeys && i < keys.size(); ++i) {
                const uint16_t kc = uint16_t(keys[i].toInt());
                if (m_keymap[l * kKeys + i] != kc) {
                    m_keymap[l * kKeys + i] = kc;
                    changes.emplace_back(l, i, kc);
                }
            }
        }
        if (!changes.empty())
            write([changes](Protocol &p) {
                for (auto [l, i, kc] : changes)
                    if (!p.setKey(l, i / kCols, i % kCols, kc))
                        return false;
                return true;
            }, QStringLiteral("Keymap"));
    }
    m_features.wasdSwap = s.value(QStringLiteral("wasdSwap"), m_features.wasdSwap).toBool();
    m_features.winLock = s.value(QStringLiteral("winLock"), m_features.winLock).toBool();
    m_features.nkro = s.value(QStringLiteral("nkro"), m_features.nkro).toBool();
    writeFeatures();
    setBottomOptimize(s.value(QStringLiteral("bottomOptimize"), m_bottomOptimize).toBool());
    setGameMode(s.value(QStringLiteral("gameMode"), m_gameMode).toBool());

    const QVariantMap l = s.value(QStringLiteral("lighting")).toMap();
    if (!l.isEmpty())
        setLighting(l.value(QStringLiteral("effect")).toInt(), l.value(QStringLiteral("speed")).toInt(), l.value(QStringLiteral("brightness")).toInt(),
                    QColor(l.value(QStringLiteral("color")).toString()));
    const QVariantList kcol = s.value(QStringLiteral("keyColors")).toList();
    if (kcol.size() == kKeys) {
        for (int i = 0; i < kKeys; ++i) {
            const QColor c(kcol[i].toString());
            m_colors[i] = Rgb{uint8_t(c.red()), uint8_t(c.green()), uint8_t(c.blue())};
        }
        auto all = m_colors;
        auto present = m_present;
        write([all, present](Protocol &p) { return p.setKeyColors(all, present); }, QStringLiteral("Key colours"));
    }
    const QVariantList socd = s.value(QStringLiteral("socd")).toList();
    if (!m_socd.empty())
        for (int i = 0; i < socd.size() && i < int(m_socd.size()); ++i)
            setSocd(i, socd[i].toMap());
    const QVariantList macros = s.value(QStringLiteral("macros")).toList();
    if (!macros.isEmpty() && m_macroCount > 0) {
        m_macros.assign(m_macroCount, {});
        for (int i = 0; i < macros.size() && i < m_macroCount; ++i) {
            const QVariantMap m = macros[i].toMap();
            QSettings(QStringLiteral("flux"), flux::settingsApp()).setValue(QStringLiteral("mad-macros/%1").arg(i), m.value(QStringLiteral("name")));
            m_macros[i] = m.value(QStringLiteral("events")).toList();
        }
        writeMacroBuffer();
    }
    emit travelSettingsChanged();
    emit keymapChanged();
    emit lightingChanged();
    emit macrosChanged();
}

} // namespace flux
