#include "LamzuMouse.h"

#include "core/KeyNames.h"

#include <QFileInfo>
#include <QLoggingCategory>
#include <QSettings>

#include <filesystem>

Q_LOGGING_CATEGORY(lcLamzu, "flux.lamzu")

namespace flux {

using namespace lamzu;

namespace {

const std::vector<int> kDefaultStageDpi = {400, 800, 1600, 3200, 6400, 30000};
const std::vector<lamzu::Rgb> kDefaultColors = {{255, 0, 0}, {0, 0, 255}, {0, 255, 0}, {255, 0, 255}, {255, 255, 0}, {255, 255, 255}};

// USB device directory in sysfs for a /dev/hidrawN node (strip interface).
std::string usbParentOf(const std::string &hidrawPath)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const std::string name = fs::path(hidrawPath).filename().string();
    fs::path dev = fs::canonical(fs::path("/sys/class/hidraw") / name / "device", ec);
    if (ec)
        return {};
    // .../1-1/1-1:1.2/0003:37B0:0010.0005 -> .../1-1
    return dev.parent_path().parent_path().string();
}

QString fwString(const std::array<int, 4> &v)
{
    return QStringLiteral("%1.%2.%3.%4").arg(v[0]).arg(v[1]).arg(v[2]).arg(v[3]);
}

QString buttonName(int id)
{
    switch (id) {
    case Button::Left: return QStringLiteral("Left Button");
    case Button::Right: return QStringLiteral("Right Button");
    case Button::Middle: return QStringLiteral("Wheel Click");
    case Button::Back: return QStringLiteral("Back Side Button");
    case Button::Forward: return QStringLiteral("Front Side Button");
    case Button::DpiButton: return QStringLiteral("DPI Button");
    }
    return QStringLiteral("Button %1").arg(id);
}

const int kButtonOrder[] = {Button::Left, Button::Right, Button::Middle, Button::Back, Button::Forward, Button::DpiButton};

} // namespace

DeviceMatch LamzuMouse::match()
{
    return DeviceMatch{
        QStringLiteral("mouse"),
        QStringLiteral("LAMZU mouse"),
        kVendorId,
        {kPid8K, kPidWired, kPid1K, kPid8KAlt, kPid1KAlt},
        kConfigUsagePage,
        kConfigUsage,
        [](const HidInfo &info, QObject *parent) -> DeviceBase * { return new LamzuMouse(info, parent); },
    };
}

LamzuMouse::LamzuMouse(const HidInfo &info, QObject *parent)
    : DeviceBase(QStringLiteral("mouse"), QString::fromStdString(info.path), parent)
    , m_info(info)
{
    m_wireless = info.productId != kPidWired;
    QString conn = info.productId == kPidWired ? QStringLiteral("USB cable")
                 : (info.productId == kPid1K || info.productId == kPid1KAlt) ? QStringLiteral("1K receiver")
                                                                              : QStringLiteral("8K receiver");
    setInfo(QStringLiteral("LAMZU INCA"), conn, QString());

    // Find the sibling interface that pushes events (input report 4).
    const std::string parentDir = usbParentOf(info.path);
    for (const auto &h : HidDevice::enumerate(info.vendorId, info.productId)) {
        if (h.usagePage == kNotifyUsagePage && h.usage == kNotifyUsage && usbParentOf(h.path) == parentDir) {
            m_notifyPath = h.path;
            break;
        }
    }

    m_probeTimer.setInterval(3000);
    connect(&m_probeTimer, &QTimer::timeout, this, [this] {
        if (online() || m_reading)
            return;
        runIo([this] {
            const bool up = probeOnline();
            onGui([this, up] {
                if (up && !online() && !m_reading)
                    fullRead();
            });
        });
    });
    m_batteryTimer.setInterval(60000);
    connect(&m_batteryTimer, &QTimer::timeout, this, [this] {
        if (!online())
            return;
        runIo([this] {
            auto b = m_proto->battery();
            onGui([this, b] {
                if (b) {
                    m_batteryFailures = 0;
                    setBattery(b->second, b->first);
                } else if (++m_batteryFailures >= 2) {
                    setOnline(false);
                    schedulePolls();
                }
            });
        });
    });
}

LamzuMouse::~LamzuMouse()
{
    stopIo();
}

// ── I/O thread ───────────────────────────────────────────────────────────

bool LamzuMouse::openIo()
{
    if (m_cfg.isOpen())
        return true;
    if (!m_cfg.open(m_info.path))
        return false;
    m_proto = std::make_unique<Protocol>(m_cfg);
    if (!m_notifyPath.empty())
        m_ntf.open(m_notifyPath);
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    return true;
}

bool LamzuMouse::probeOnline()
{
    return m_proto && m_proto->firmware(Mouse, true).has_value();
}

LamzuMouse::Ident LamzuMouse::readIdent()
{
    Ident id;
    if (m_wireless) {
        if (auto fw = m_proto->firmware(ReceiverUsb))
            id.receiverFw = fwString(*fw);
    }
    auto fw = m_proto->firmware(Mouse, true);
    if (!fw)
        return id;
    id.ok = true;
    id.mouseFw = fwString(*fw);
    id.fwNum = (*fw)[0] * 10000 + (*fw)[1] * 1000 + (*fw)[2] * 100 + (*fw)[3];
    id.sensor = m_proto->sensorModel().value_or(0);
    return id;
}

LamzuMouse::State LamzuMouse::readState(int sensor, bool tracking)
{
    State s;
    Protocol &p = *m_proto;
    s.profile = p.profile().value_or(1);
    const int pp = s.profile;
    s.polling = p.pollingRate(pp).value_or(1000);
    s.debounce = p.debounce(pp).value_or(0);
    s.sleep = p.sleepSeconds(pp).value_or(60);
    s.lod = p.lod(pp, sensor).value_or(1.0);
    s.tune = p.angleTune(pp).value_or(0);
    s.snap = p.flag(pp, SetCmd::AngleSnap).value_or(false);
    s.xy = p.flag(pp, SetCmd::DpiXY).value_or(false);
    s.motion = p.flag(pp, SetCmd::MotionSync).value_or(false);
    s.ripple = p.flag(pp, SetCmd::Ripple).value_or(false);
    s.hyper = p.flag(pp, SetCmd::Hyper).value_or(false);
    if (tracking)
        s.tracking = p.flag(pp, SetCmd::Tracking).value_or(false);
    for (int b : kButtonOrder) {
        if (auto a = p.button(pp, uint8_t(b)))
            s.buttons[b] = *a;
    }
    if (auto st = p.stages(pp))
        s.stages = *st;
    s.active = std::clamp(p.activeStage(pp).value_or(1), 1, int(s.stages.size()));
    if (auto c = p.stageColors(pp, int(s.stages.size())))
        s.colors = *c;
    return s;
}

std::map<int, std::vector<uint8_t>> LamzuMouse::readMacros()
{
    std::map<int, std::vector<uint8_t>> out;
    for (int id = 1; id <= kMaxMacros; ++id) {
        auto data = m_proto->readMacro(id);
        if (data && !data->empty())
            out[id] = *data;
    }
    return out;
}

void LamzuMouse::ioIdle()
{
    if (!m_ntf.isOpen()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return;
    }
    uint8_t buf[64];
    const int n = m_ntf.read({buf, sizeof buf}, 40);
    if (n <= 0)
        return;
    const Event e = Protocol::parseEvent(buf, n);
    if (e.kind != Event::None)
        onGui([this, e] { handleEvent(e); });
}

// ── GUI thread ───────────────────────────────────────────────────────────

void LamzuMouse::start()
{
    if (demo()) {
        m_mouseFw = QStringLiteral("0.0.0.15");
        m_receiverFw = QStringLiteral("0.0.0.15");
        m_fwNum = 15;
        m_sensor = 2;
        State s;
        s.polling = 4000;
        s.stages = {{400, 400}, {800, 800}, {1600, 1600}, {3200, 3200}};
        s.colors = {{255, 0, 0}, {0, 0, 255}, {0, 255, 0}, {255, 0, 255}};
        s.active = 2;
        s.lod = 0.7;
        s.motion = true;
        s.sleep = 60;
        applyState(s);
        m_macros[1] = encodeMacro({QVariantMap{{"type", "key"}, {"usage", 0x06}, {"down", true}}, QVariantMap{{"type", "delay"}, {"ms", 30}},
                                   QVariantMap{{"type", "key"}, {"usage", 0x06}, {"down", false}}});
        m_macrosLoaded = true;
        setBattery(76, false);
        setInfo(displayName(), connection(), m_mouseFw);
        emit infoChanged();
        emit macrosChanged();
        setOnline(true);
        setReady(true);
        return;
    }
    runIo([this] {
        if (!openIo()) {
            const QString err = QString::fromStdString(m_cfg.lastError());
            onGui([this, err] { setLastError(QStringLiteral("Couldn't open the receiver: %1").arg(err)); });
            return;
        }
        m_io.setIdleHook([this] { ioIdle(); });
        onGui([this] { fullRead(); });
    });
}

void LamzuMouse::refresh()
{
    fullRead();
}

void LamzuMouse::fullRead()
{
    if (demo())
        return;
    if (m_reading)
        return;
    m_reading = true;
    runIo([this] {
        Ident id = readIdent();
        if (!id.ok) {
            onGui([this, id] {
                m_reading = false;
                applyIdent(id);
                setOnline(false);
                setReady(true);
                schedulePolls();
            });
            return;
        }
        auto bat = m_proto->battery();
        State s = readState(id.sensor, id.fwNum >= 16);
        onGui([this, id, s, bat] {
            applyIdent(id);
            applyState(s);
            if (bat)
                setBattery(bat->second, bat->first);
            setOnline(true);
            setReady(true);
            schedulePolls();
        });
        auto macros = readMacros();
        onGui([this, macros] {
            m_macros = macros;
            m_macrosLoaded = true;
            m_reading = false;
            emit macrosChanged();
        });
    });
}

void LamzuMouse::schedulePolls()
{
    if (online()) {
        m_probeTimer.stop();
        if (m_wireless && !m_batteryTimer.isActive())
            m_batteryTimer.start();
    } else {
        m_batteryTimer.stop();
        if (!m_probeTimer.isActive())
            m_probeTimer.start();
    }
}

void LamzuMouse::applyIdent(const Ident &id)
{
    if (!id.receiverFw.isEmpty())
        m_receiverFw = id.receiverFw;
    if (id.ok) {
        m_mouseFw = id.mouseFw;
        m_fwNum = id.fwNum;
        m_sensor = id.sensor;
        switch (m_sensor) {
        case 1: m_dpiMax = 26000; m_dpiStep = 50; break;
        case 4: m_dpiMax = 40000; m_dpiStep = 1; break;
        default: m_dpiMax = 30000; m_dpiStep = 50; break;
        }
    }
    setInfo(displayName(), connection(), m_mouseFw.isEmpty() ? m_receiverFw : m_mouseFw);
    emit infoChanged();
}

void LamzuMouse::applyState(const State &s)
{
    m_s = s;
    if (m_s.stages.empty())
        m_s.stages = {{400, 400}, {800, 800}, {1600, 1600}, {3200, 3200}};
    while (m_s.colors.size() < m_s.stages.size())
        m_s.colors.push_back(defaultColor(int(m_s.colors.size())));
    emit settingsChanged();
    emit buttonsChanged();
}

void LamzuMouse::setBattery(int level, bool charging)
{
    if (level == m_battery && charging == m_charging)
        return;
    m_battery = level;
    m_charging = charging;
    emit batteryChanged();
}

void LamzuMouse::handleEvent(const Event &e)
{
    switch (e.kind) {
    case Event::StageChanged:
        if (e.a >= 1 && e.a <= int(m_s.stages.size())) {
            m_s.active = e.a;
            if (e.b > 0) {
                m_s.stages[e.a - 1].x = e.b;
                m_s.stages[e.a - 1].y = e.c > 0 ? e.c : e.b;
            }
            emit settingsChanged();
        }
        break;
    case Event::ProfileChanged:
        fullRead();
        break;
    case Event::Battery:
        setBattery(e.a, e.b == 1);
        break;
    case Event::Link:
        if (e.a == 0) {
            setOnline(false);
            schedulePolls();
        } else {
            fullRead();
        }
        break;
    case Event::Lod:
        m_s.lod = Protocol::decodeLod(uint8_t(e.a), m_sensor);
        emit settingsChanged();
        break;
    case Event::Polling:
        if (e.a) {
            m_s.polling = e.a;
            emit settingsChanged();
        }
        break;
    case Event::Hyper:
        m_s.hyper = e.a == 1;
        m_s.tracking = e.b == 1;
        emit settingsChanged();
        break;
    default:
        break;
    }
}

void LamzuMouse::write(std::function<bool(Protocol &)> fn, const QString &what)
{
    runIo([this, fn = std::move(fn), what] {
        const bool ok = m_proto && fn(*m_proto);
        if (!ok) {
            const QString err = m_proto ? QString::fromStdString(m_proto->lastError()) : QStringLiteral("not open");
            onGui([this, what, err] {
                setLastError(QStringLiteral("%1 didn't save (%2)").arg(what, err));
                fullRead();
            });
        }
    });
}

// ── Capability lists ─────────────────────────────────────────────────────

QString LamzuMouse::sensorName() const
{
    switch (m_sensor) {
    case 1: return QStringLiteral("PixArt PAW3395");
    case 2: return QStringLiteral("PixArt PAW3950");
    case 4: return QStringLiteral("PixArt PAW3955");
    default: return QStringLiteral("PixArt PAW3950");
    }
}

QVariantList LamzuMouse::lodOptions() const
{
    if (m_sensor == 1)
        return {1.0, 2.0};
    if (m_sensor == 4)
        return {0.7, 1.0, 1.3, 1.5, 1.7};
    return {0.7, 1.0, 2.0};
}

QVariantList LamzuMouse::pollingRates() const
{
    if (!m_wireless || m_info.productId == kPid1K || m_info.productId == kPid1KAlt)
        return {125, 250, 500, 1000};
    return {500, 1000, 2000, 4000, 8000};
}

QVariantList LamzuMouse::sleepOptions() const
{
    return {10, 30, 60, 300, 600, 1800};
}

QVariantList LamzuMouse::stages() const
{
    QVariantList out;
    for (size_t i = 0; i < m_s.stages.size(); ++i) {
        const Rgb c = i < m_s.colors.size() ? m_s.colors[i] : defaultColor(int(i));
        out << QVariantMap{{QStringLiteral("x"), m_s.stages[i].x},
                           {QStringLiteral("y"), m_s.stages[i].y},
                           {QStringLiteral("color"), QColor(c.r, c.g, c.b)}};
    }
    return out;
}

lamzu::Rgb LamzuMouse::defaultColor(int index)
{
    return kDefaultColors[std::clamp(index, 0, int(kDefaultColors.size()) - 1)];
}

// ── Setters ──────────────────────────────────────────────────────────────

void LamzuMouse::setProfile(int p)
{
    p = std::clamp(p, 1, profileCount());
    if (p == m_s.profile)
        return;
    m_s.profile = p;
    emit settingsChanged();
    runIo([this, p] {
        const bool ok = m_proto && m_proto->setProfile(p);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        onGui([this, ok] {
            if (!ok)
                setLastError(QStringLiteral("Couldn't switch profile"));
            else
                emit toast(QStringLiteral("Switched to profile %1").arg(m_s.profile), false);
            fullRead();
        });
    });
}

void LamzuMouse::setPollingRate(int hz)
{
    if (hz == m_s.polling)
        return;
    m_s.polling = hz;
    emit settingsChanged();
    const int p = m_s.profile;
    write([p, hz](Protocol &pr) { return pr.setPollingRate(p, hz); }, QStringLiteral("Polling rate"));
}

void LamzuMouse::writeStages()
{
    const int p = m_s.profile;
    auto st = m_s.stages;
    if (!m_s.xy)
        for (auto &s : st)
            s.y = s.x;
    auto colors = m_s.colors;
    colors.resize(st.size());
    const int active = m_s.active;
    write([p, st, colors, active](Protocol &pr) {
        return pr.setActiveStage(p, std::min<int>(active, int(st.size()))) && pr.setStages(p, st) && pr.setStageColors(p, colors);
    }, QStringLiteral("DPI stages"));
}

void LamzuMouse::setStageCount(int n)
{
    n = std::clamp(n, 1, maxStages());
    if (n == int(m_s.stages.size()))
        return;
    const int old = int(m_s.stages.size());
    m_s.stages.resize(n);
    for (int i = old; i < n; ++i) {
        const int d = std::min(kDefaultStageDpi[i], m_dpiMax);
        m_s.stages[i] = {d, d};
    }
    while (int(m_s.colors.size()) < n)
        m_s.colors.push_back(defaultColor(int(m_s.colors.size())));
    m_s.active = std::min(m_s.active, n);
    emit settingsChanged();
    writeStages();
}

void LamzuMouse::setStage(int index, int x, int y)
{
    if (index < 0 || index >= int(m_s.stages.size()))
        return;
    auto snap = [this](int v) { return std::clamp(v / m_dpiStep * m_dpiStep, dpiMin(), m_dpiMax); };
    x = snap(x);
    y = m_s.xy ? snap(y) : x;
    if (m_s.stages[index].x == x && m_s.stages[index].y == y)
        return;
    m_s.stages[index] = {x, y};
    emit settingsChanged();
    writeStages();
}

void LamzuMouse::setActiveStage(int stage1)
{
    stage1 = std::clamp(stage1, 1, int(m_s.stages.size()));
    if (stage1 == m_s.active)
        return;
    m_s.active = stage1;
    emit settingsChanged();
    const int p = m_s.profile;
    write([p, stage1](Protocol &pr) { return pr.setActiveStage(p, stage1); }, QStringLiteral("Active DPI stage"));
}

void LamzuMouse::setDpiXY(bool on)
{
    if (on == m_s.xy)
        return;
    m_s.xy = on;
    if (!on)
        for (auto &s : m_s.stages)
            s.y = s.x;
    emit settingsChanged();
    const int p = m_s.profile;
    write([p, on](Protocol &pr) { return pr.setFlag(p, SetCmd::DpiXY, on); }, QStringLiteral("X/Y DPI"));
    writeStages();
}

void LamzuMouse::setStageColor(int index, const QColor &c)
{
    if (index < 0 || index >= int(m_s.stages.size()))
        return;
    while (int(m_s.colors.size()) <= index)
        m_s.colors.push_back(defaultColor(int(m_s.colors.size())));
    m_s.colors[index] = {uint8_t(c.red()), uint8_t(c.green()), uint8_t(c.blue())};
    emit settingsChanged();
    const int p = m_s.profile;
    auto colors = m_s.colors;
    colors.resize(m_s.stages.size());
    write([p, colors](Protocol &pr) { return pr.setStageColors(p, colors); }, QStringLiteral("Stage colour"));
}

void LamzuMouse::setLod(double mm)
{
    if (std::abs(mm - m_s.lod) < 0.01)
        return;
    m_s.lod = mm;
    emit settingsChanged();
    const int p = m_s.profile;
    write([p, mm](Protocol &pr) { return pr.setLod(p, mm); }, QStringLiteral("Lift-off distance"));
}

#define FLUX_FLAG_SETTER(fn, member, cmd, label)                                                           \
    void LamzuMouse::fn(bool on)                                                                           \
    {                                                                                                      \
        if (on == m_s.member)                                                                              \
            return;                                                                                        \
        m_s.member = on;                                                                                   \
        emit settingsChanged();                                                                            \
        const int p = m_s.profile;                                                                         \
        write([p, on](Protocol &pr) { return pr.setFlag(p, cmd, on); }, QStringLiteral(label));           \
    }

FLUX_FLAG_SETTER(setMotionSync, motion, SetCmd::MotionSync, "Motion Sync")
FLUX_FLAG_SETTER(setAngleSnap, snap, SetCmd::AngleSnap, "Angle snapping")
FLUX_FLAG_SETTER(setRippleControl, ripple, SetCmd::Ripple, "Ripple control")
FLUX_FLAG_SETTER(setTrackingMode, tracking, SetCmd::Tracking, "20K FPS mode")
#undef FLUX_FLAG_SETTER

void LamzuMouse::setHyperMode(bool on)
{
    if (on == m_s.hyper)
        return;
    m_s.hyper = on;
    const bool dropTracking = !on && m_s.tracking;
    if (dropTracking)
        m_s.tracking = false;
    emit settingsChanged();
    const int p = m_s.profile;
    write([p, on, dropTracking](Protocol &pr) {
        bool ok = pr.setFlag(p, SetCmd::Hyper, on);
        if (ok && dropTracking)
            ok = pr.setFlag(p, SetCmd::Tracking, false);
        return ok;
    }, QStringLiteral("Competition mode"));
}

void LamzuMouse::setAngleTune(int deg)
{
    deg = std::clamp(deg, -30, 30);
    if (deg == m_s.tune)
        return;
    m_s.tune = deg;
    emit settingsChanged();
    const int p = m_s.profile;
    write([p, deg](Protocol &pr) { return pr.setAngleTune(p, deg); }, QStringLiteral("Angle tune"));
}

void LamzuMouse::setDebounce(int ms)
{
    ms = std::clamp(ms, 0, 15);
    if (ms == m_s.debounce)
        return;
    m_s.debounce = ms;
    emit settingsChanged();
    const int p = m_s.profile;
    write([p, ms](Protocol &pr) { return pr.setDebounce(p, ms); }, QStringLiteral("Debounce"));
}

void LamzuMouse::setSleepSeconds(int s)
{
    if (s == m_s.sleep)
        return;
    m_s.sleep = s;
    emit settingsChanged();
    const int p = m_s.profile;
    write([p, s](Protocol &pr) { return pr.setSleepSeconds(p, s); }, QStringLiteral("Sleep timer"));
}

// ── Buttons ──────────────────────────────────────────────────────────────

QVariantMap LamzuMouse::actionToMap(const ButtonAction &a)
{
    const auto &p = a.params;
    auto at = [&](size_t i) -> int { return i < p.size() ? p[i] : 0; };
    QVariantMap m;
    switch (a.type) {
    case ActionType::Disabled: m[QStringLiteral("kind")] = QStringLiteral("disabled"); break;
    case ActionType::Mouse: m[QStringLiteral("kind")] = QStringLiteral("mouse"); m[QStringLiteral("code")] = at(0); break;
    case ActionType::XClick:
        m[QStringLiteral("kind")] = QStringLiteral("xclick");
        m[QStringLiteral("button")] = at(1) ? at(1) : 1;
        m[QStringLiteral("count")] = at(2);
        m[QStringLiteral("interval")] = (at(3) << 8) | at(4);
        break;
    case ActionType::Keyboard:
        m[QStringLiteral("kind")] = QStringLiteral("key");
        m[QStringLiteral("modifiers")] = at(0);
        m[QStringLiteral("usage")] = at(1);
        break;
    case ActionType::Consumer:
        m[QStringLiteral("kind")] = QStringLiteral("media");
        m[QStringLiteral("usage")] = (at(0) << 8) | at(1);
        break;
    case ActionType::Dpi: {
        m[QStringLiteral("kind")] = QStringLiteral("dpi");
        const int op = at(0);
        m[QStringLiteral("op")] = op == 1 ? QStringLiteral("up") : op == 2 ? QStringLiteral("down") : op == 5 ? QStringLiteral("lock")
                                 : op == 7 ? QStringLiteral("cycledown") : QStringLiteral("cycle");
        if (op == 5)
            m[QStringLiteral("dpi")] = (at(1) << 8) | at(2);
        break;
    }
    case ActionType::ProfileCycle: m[QStringLiteral("kind")] = QStringLiteral("profilecycle"); break;
    case ActionType::PollingCycle: m[QStringLiteral("kind")] = QStringLiteral("pollcycle"); break;
    case ActionType::LodCycle: m[QStringLiteral("kind")] = QStringLiteral("lodcycle"); break;
    case ActionType::HyperCycle: m[QStringLiteral("kind")] = QStringLiteral("hypercycle"); break;
    case ActionType::MacroTimes:
    case ActionType::MacroHold:
    case ActionType::MacroToggle:
        m[QStringLiteral("kind")] = QStringLiteral("macro");
        m[QStringLiteral("id")] = (at(0) << 8) | at(1);
        m[QStringLiteral("mode")] = a.type == ActionType::MacroTimes ? QStringLiteral("times")
                                  : a.type == ActionType::MacroHold ? QStringLiteral("hold") : QStringLiteral("toggle");
        m[QStringLiteral("count")] = a.type == ActionType::MacroTimes ? std::max(1, at(2)) : 1;
        break;
    default: {
        m[QStringLiteral("kind")] = QStringLiteral("raw");
        m[QStringLiteral("type")] = a.type;
        QVariantList raw;
        for (uint8_t b : p)
            raw << int(b);
        m[QStringLiteral("params")] = raw;
    }
    }
    return m;
}

ButtonAction LamzuMouse::mapToAction(const QVariantMap &m)
{
    const QString kind = m.value(QStringLiteral("kind")).toString();
    auto i = [&](const char *k, int d = 0) { return m.value(QString::fromLatin1(k), d).toInt(); };
    ButtonAction a;
    if (kind == QLatin1String("mouse")) {
        a.type = ActionType::Mouse;
        a.params = {uint8_t(i("code", 1))};
    } else if (kind == QLatin1String("xclick")) {
        const int iv = std::max(10, i("interval", 10));
        a.type = ActionType::XClick;
        a.params = {0x01, uint8_t(i("button", 1)), uint8_t(std::clamp(i("count", 3), 1, 255)), uint8_t(iv >> 8), uint8_t(iv)};
    } else if (kind == QLatin1String("key")) {
        a.type = ActionType::Keyboard;
        a.params = {uint8_t(i("modifiers")), uint8_t(i("usage"))};
    } else if (kind == QLatin1String("media")) {
        const int u = i("usage");
        a.type = ActionType::Consumer;
        a.params = {uint8_t(u >> 8), uint8_t(u)};
    } else if (kind == QLatin1String("dpi")) {
        const QString op = m.value(QStringLiteral("op")).toString();
        a.type = ActionType::Dpi;
        if (op == QLatin1String("lock")) {
            const int d = std::clamp(i("dpi", 400), 50, 40000);
            a.params = {0x05, uint8_t(d >> 8), uint8_t(d), uint8_t(d >> 8), uint8_t(d)};
        } else {
            a.params = {uint8_t(op == QLatin1String("up") ? 1 : op == QLatin1String("down") ? 2 : op == QLatin1String("cycledown") ? 7 : 6)};
        }
    } else if (kind == QLatin1String("profilecycle")) {
        a.type = ActionType::ProfileCycle;
        a.params = {0x03};
    } else if (kind == QLatin1String("pollcycle")) {
        a.type = ActionType::PollingCycle;
        a.params = {0x03};
    } else if (kind == QLatin1String("lodcycle")) {
        a.type = ActionType::LodCycle;
        a.params = {0x03};
    } else if (kind == QLatin1String("hypercycle")) {
        a.type = ActionType::HyperCycle;
        a.params = {0x00};
    } else if (kind == QLatin1String("macro")) {
        const int id = i("id", 1);
        const QString mode = m.value(QStringLiteral("mode")).toString();
        if (mode == QLatin1String("hold")) {
            a.type = ActionType::MacroHold;
            a.params = {uint8_t(id >> 8), uint8_t(id)};
        } else if (mode == QLatin1String("toggle")) {
            a.type = ActionType::MacroToggle;
            a.params = {uint8_t(id >> 8), uint8_t(id)};
        } else {
            a.type = ActionType::MacroTimes;
            a.params = {uint8_t(id >> 8), uint8_t(id), uint8_t(std::clamp(i("count", 1), 1, 255))};
        }
    } else if (kind == QLatin1String("raw")) {
        a.type = uint8_t(i("type"));
        for (const auto &v : m.value(QStringLiteral("params")).toList())
            a.params.push_back(uint8_t(v.toInt()));
    } else {
        a.type = ActionType::Disabled;
    }
    return a;
}

ButtonAction LamzuMouse::defaultAction(int buttonId)
{
    if (buttonId == Button::DpiButton)
        return {ActionType::Dpi, {0x06}};
    return {ActionType::Mouse, {uint8_t(buttonId)}};
}

QString LamzuMouse::describeAction(const QVariantMap &m) const
{
    const QString kind = m.value(QStringLiteral("kind")).toString();
    if (kind == QLatin1String("disabled"))
        return QStringLiteral("Disabled");
    if (kind == QLatin1String("mouse")) {
        switch (m.value(QStringLiteral("code")).toInt()) {
        case 1: return QStringLiteral("Left Click");
        case 2: return QStringLiteral("Right Click");
        case 3: return QStringLiteral("Middle Click");
        case 4: return QStringLiteral("Back");
        case 5: return QStringLiteral("Forward");
        case 0x10: return QStringLiteral("Scroll Up");
        case 0x11: return QStringLiteral("Scroll Down");
        default: return QStringLiteral("Mouse Button %1").arg(m.value(QStringLiteral("code")).toInt());
        }
    }
    if (kind == QLatin1String("xclick")) {
        const int c = m.value(QStringLiteral("count")).toInt();
        return c == 2 ? QStringLiteral("Double Click") : QStringLiteral("%1× Click · %2 ms").arg(c).arg(m.value(QStringLiteral("interval")).toInt());
    }
    if (kind == QLatin1String("key")) {
        const QString mods = KeyNames::modifierText(m.value(QStringLiteral("modifiers")).toInt());
        const int u = m.value(QStringLiteral("usage")).toInt();
        const QString key = u ? KeyNames::hidName(u) : QString();
        if (mods.isEmpty())
            return key.isEmpty() ? QStringLiteral("Key") : key;
        return key.isEmpty() ? mods : mods + QStringLiteral(" + ") + key;
    }
    if (kind == QLatin1String("media"))
        return KeyNames::consumerName(m.value(QStringLiteral("usage")).toInt());
    if (kind == QLatin1String("dpi")) {
        const QString op = m.value(QStringLiteral("op")).toString();
        if (op == QLatin1String("up")) return QStringLiteral("DPI Up");
        if (op == QLatin1String("down")) return QStringLiteral("DPI Down");
        if (op == QLatin1String("lock")) return QStringLiteral("DPI Lock · %1").arg(m.value(QStringLiteral("dpi")).toInt());
        if (op == QLatin1String("cycledown")) return QStringLiteral("DPI Cycle (Down)");
        return QStringLiteral("DPI Cycle");
    }
    if (kind == QLatin1String("profilecycle")) return QStringLiteral("Profile Cycle");
    if (kind == QLatin1String("pollcycle")) return QStringLiteral("Polling Rate Cycle");
    if (kind == QLatin1String("lodcycle")) return QStringLiteral("LOD Cycle");
    if (kind == QLatin1String("hypercycle")) return QStringLiteral("Competition Mode Toggle");
    if (kind == QLatin1String("macro")) {
        const int id = m.value(QStringLiteral("id")).toInt();
        const QString mode = m.value(QStringLiteral("mode")).toString();
        const QString name = macroName(id);
        if (mode == QLatin1String("hold")) return QStringLiteral("%1 · while held").arg(name);
        if (mode == QLatin1String("toggle")) return QStringLiteral("%1 · toggle").arg(name);
        const int c = m.value(QStringLiteral("count")).toInt();
        return c > 1 ? QStringLiteral("%1 · ×%2").arg(name).arg(c) : name;
    }
    return QStringLiteral("Custom (0x%1)").arg(m.value(QStringLiteral("type")).toInt(), 2, 16, QLatin1Char('0'));
}

QVariantList LamzuMouse::buttons() const
{
    QVariantList out;
    for (int id : kButtonOrder) {
        const auto it = m_s.buttons.find(id);
        const ButtonAction a = it != m_s.buttons.end() ? it->second : defaultAction(id);
        const QVariantMap action = actionToMap(a);
        out << QVariantMap{{QStringLiteral("id"), id},
                           {QStringLiteral("name"), buttonName(id)},
                           {QStringLiteral("action"), action},
                           {QStringLiteral("label"), describeAction(action)},
                           {QStringLiteral("isDefault"), action == actionToMap(defaultAction(id))}};
    }
    return out;
}

bool LamzuMouse::setButtonAction(int buttonId, const QVariantMap &action)
{
    const ButtonAction a = mapToAction(action);
    // Never leave the mouse without a left click (same rule as the vendor app).
    int lefts = 0;
    for (int id : kButtonOrder) {
        const auto it = m_s.buttons.find(id);
        const ButtonAction cur = id == buttonId ? a : (it != m_s.buttons.end() ? it->second : defaultAction(id));
        if (cur.type == ActionType::Mouse && !cur.params.empty() && cur.params[0] == 1)
            ++lefts;
    }
    if (lefts == 0) {
        emit toast(QStringLiteral("At least one button has to stay Left Click"), true);
        return false;
    }
    m_s.buttons[buttonId] = a;
    emit buttonsChanged();
    const int p = m_s.profile;
    write([p, buttonId, a](Protocol &pr) { return pr.setButton(p, uint8_t(buttonId), a); }, buttonName(buttonId));
    return true;
}

void LamzuMouse::resetButton(int buttonId)
{
    setButtonAction(buttonId, actionToMap(defaultAction(buttonId)));
}

// ── Macros ───────────────────────────────────────────────────────────────

QVariantList LamzuMouse::decodeMacro(const std::vector<uint8_t> &d)
{
    QVariantList ev;
    int mouseState = 0;
    size_t i = 0;
    auto push = [&](QVariantMap m) { ev << m; };
    while (i < d.size()) {
        const uint8_t op = d[i];
        auto need = [&](size_t n) { return i + n < d.size() + 1 && i + n <= d.size(); };
        if ((op == 0x02 || op == 0x03) && need(2)) {
            push({{QStringLiteral("type"), QStringLiteral("key")}, {QStringLiteral("usage"), int(d[i + 1])}, {QStringLiteral("down"), op == 0x02}});
            i += 2;
        } else if ((op == 0x09 || op == 0x0A) && need(2)) {
            // Express modifiers as keys (0xE0..) so the editor treats them alike.
            const int mask = d[i + 1];
            for (int b = 0; b < 8; ++b)
                if (mask & (1 << b))
                    push({{QStringLiteral("type"), QStringLiteral("key")}, {QStringLiteral("usage"), 0xE0 + b}, {QStringLiteral("down"), op == 0x09}});
            i += 2;
        } else if (op == 0x01 && need(2)) {
            const int ns = d[i + 1];
            const int changed = ns ^ mouseState;
            for (int b = 0; b < 5; ++b)
                if (changed & (1 << b))
                    push({{QStringLiteral("type"), QStringLiteral("mouse")}, {QStringLiteral("button"), b + 1}, {QStringLiteral("down"), bool(ns & (1 << b))}});
            mouseState = ns;
            i += 2;
        } else if (op == 0x10 && need(2)) {
            push({{QStringLiteral("type"), QStringLiteral("wheel")}, {QStringLiteral("up"), d[i + 1] == 0x01}});
            i += 2;
        } else if (op >= 0x20 && op <= 0x23 && need(size_t(op - 0x20 + 2))) {
            const int n = op - 0x20 + 1;
            uint32_t ms = 0;
            for (int k = 0; k < n; ++k)
                ms = (ms << 8) | d[i + 1 + k];
            push({{QStringLiteral("type"), QStringLiteral("delay")}, {QStringLiteral("ms"), int(ms)}});
            i += 1 + n;
        } else {
            break; // unknown opcode: stop rather than misparse
        }
    }
    return ev;
}

std::vector<uint8_t> LamzuMouse::encodeMacro(const QVariantList &events)
{
    std::vector<uint8_t> out;
    int mouseState = 0;
    for (const auto &v : events) {
        const QVariantMap e = v.toMap();
        const QString t = e.value(QStringLiteral("type")).toString();
        if (t == QLatin1String("key")) {
            const int u = e.value(QStringLiteral("usage")).toInt();
            const bool down = e.value(QStringLiteral("down")).toBool();
            if (u >= 0xE0 && u <= 0xE7) {
                out.push_back(down ? 0x09 : 0x0A);
                out.push_back(uint8_t(1 << (u - 0xE0)));
            } else {
                out.push_back(down ? 0x02 : 0x03);
                out.push_back(uint8_t(u));
            }
        } else if (t == QLatin1String("mouse")) {
            const int bit = 1 << std::clamp(e.value(QStringLiteral("button")).toInt() - 1, 0, 4);
            mouseState = e.value(QStringLiteral("down")).toBool() ? (mouseState | bit) : (mouseState & ~bit);
            out.push_back(0x01);
            out.push_back(uint8_t(mouseState));
        } else if (t == QLatin1String("wheel")) {
            out.push_back(0x10);
            out.push_back(e.value(QStringLiteral("up")).toBool() ? 0x01 : 0xFF);
        } else if (t == QLatin1String("delay")) {
            const uint32_t ms = uint32_t(std::max(0, e.value(QStringLiteral("ms")).toInt()));
            if (ms <= 0xFF) {
                out.insert(out.end(), {0x20, uint8_t(ms)});
            } else if (ms <= 0xFFFF) {
                out.insert(out.end(), {0x21, uint8_t(ms >> 8), uint8_t(ms)});
            } else if (ms <= 0xFFFFFF) {
                out.insert(out.end(), {0x22, uint8_t(ms >> 16), uint8_t(ms >> 8), uint8_t(ms)});
            } else {
                out.insert(out.end(), {0x23, uint8_t(ms >> 24), uint8_t(ms >> 16), uint8_t(ms >> 8), uint8_t(ms)});
            }
        }
    }
    return out;
}

QString LamzuMouse::macroName(int id) const
{
    QSettings s(QStringLiteral("flux"), flux::settingsApp());
    const QString n = s.value(QStringLiteral("lamzu-macros/%1").arg(id)).toString();
    return n.isEmpty() ? QStringLiteral("Macro %1").arg(id) : n;
}

QVariantList LamzuMouse::macros() const
{
    QVariantList out;
    for (const auto &[id, raw] : m_macros)
        out << QVariantMap{{QStringLiteral("id"), id},
                           {QStringLiteral("name"), macroName(id)},
                           {QStringLiteral("events"), decodeMacro(raw)},
                           {QStringLiteral("bytes"), int(raw.size())}};
    return out;
}

int LamzuMouse::freeMacroId() const
{
    for (int id = 1; id <= kMaxMacros; ++id)
        if (!m_macros.count(id))
            return id;
    return -1;
}

void LamzuMouse::saveMacro(int id, const QString &name, const QVariantList &events)
{
    if (id < 1 || id > kMaxMacros)
        return;
    const auto raw = encodeMacro(events);
    if (raw.empty()) {
        emit toast(QStringLiteral("A macro needs at least one step"), true);
        return;
    }
    QSettings(QStringLiteral("flux"), flux::settingsApp()).setValue(QStringLiteral("lamzu-macros/%1").arg(id), name);
    m_macros[id] = raw;
    emit macrosChanged();
    emit buttonsChanged(); // labels may include the macro name
    runIo([this, id, raw] {
        const bool ok = m_proto && m_proto->writeMacro(id, raw);
        onGui([this, ok] {
            if (ok)
                emit toast(QStringLiteral("Macro saved to the mouse"), false);
            else
                setLastError(QStringLiteral("Macro didn't save"));
        });
    });
}

void LamzuMouse::deleteMacro(int id)
{
    if (!m_macros.erase(id))
        return;
    QSettings(QStringLiteral("flux"), flux::settingsApp()).remove(QStringLiteral("lamzu-macros/%1").arg(id));
    emit macrosChanged();
    // Unbind buttons that pointed at it (firmware doesn't).
    for (int b : kButtonOrder) {
        const auto it = m_s.buttons.find(b);
        if (it == m_s.buttons.end())
            continue;
        const auto &a = it->second;
        const bool isMacro = a.type == ActionType::MacroTimes || a.type == ActionType::MacroHold || a.type == ActionType::MacroToggle;
        if (isMacro && a.params.size() >= 2 && ((a.params[0] << 8) | a.params[1]) == id)
            resetButton(b);
    }
    write([id](Protocol &pr) { return pr.deleteMacro(id); }, QStringLiteral("Delete macro"));
}

// ── Presets ──────────────────────────────────────────────────────────────

void LamzuMouse::applyOsuPreset()
{
    // Smoothest cursor and lowest latency: top polling rate, raw sensor
    // output (no motion sync / angle snapping / ripple), lowest LOD.
    const QVariantList rates = pollingRates();
    setPollingRate(rates.last().toInt());
    setMotionSync(false);
    setAngleSnap(false);
    setRippleControl(false);
    setAngleTune(0);
    const QVariantList lods = lodOptions();
    setLod(lods.first().toDouble());
    setHyperMode(true);
    setDebounce(2);
    emit toast(QStringLiteral("osu! preset applied to the mouse"), false);
}

// ── Reset / profiles ─────────────────────────────────────────────────────

void LamzuMouse::factoryReset()
{
    const bool wl = m_wireless;
    runIo([this, wl] {
        const bool ok = m_proto && m_proto->factoryReset(wl);
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        onGui([this, ok] {
            if (ok)
                emit toast(QStringLiteral("Mouse restored to factory settings"), false);
            else
                setLastError(QStringLiteral("Factory reset failed"));
            fullRead();
        });
    });
}

QVariantMap LamzuMouse::exportSettings() const
{
    QVariantList st;
    for (size_t i = 0; i < m_s.stages.size(); ++i) {
        const Rgb c = i < m_s.colors.size() ? m_s.colors[i] : defaultColor(int(i));
        st << QVariantMap{{QStringLiteral("x"), m_s.stages[i].x}, {QStringLiteral("y"), m_s.stages[i].y},
                          {QStringLiteral("color"), QColor(c.r, c.g, c.b).name()}};
    }
    QVariantList btns;
    for (int id : kButtonOrder) {
        const auto it = m_s.buttons.find(id);
        btns << QVariantMap{{QStringLiteral("id"), id},
                            {QStringLiteral("action"), actionToMap(it != m_s.buttons.end() ? it->second : defaultAction(id))}};
    }
    QVariantList macros;
    for (const auto &[id, raw] : m_macros)
        macros << QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("name"), macroName(id)}, {QStringLiteral("events"), decodeMacro(raw)}};

    return {
        {QStringLiteral("device"), QStringLiteral("lamzu-inca")},
        {QStringLiteral("pollingRate"), m_s.polling},
        {QStringLiteral("stages"), st},
        {QStringLiteral("activeStage"), m_s.active},
        {QStringLiteral("dpiXY"), m_s.xy},
        {QStringLiteral("lod"), m_s.lod},
        {QStringLiteral("motionSync"), m_s.motion},
        {QStringLiteral("angleSnap"), m_s.snap},
        {QStringLiteral("rippleControl"), m_s.ripple},
        {QStringLiteral("hyperMode"), m_s.hyper},
        {QStringLiteral("trackingMode"), m_s.tracking},
        {QStringLiteral("angleTune"), m_s.tune},
        {QStringLiteral("debounce"), m_s.debounce},
        {QStringLiteral("sleepSeconds"), m_s.sleep},
        {QStringLiteral("buttons"), btns},
        {QStringLiteral("macros"), macros},
    };
}

void LamzuMouse::importSettings(const QVariantMap &s)
{
    if (s.value(QStringLiteral("device")).toString() != QLatin1String("lamzu-inca")) {
        emit toast(QStringLiteral("That profile is for a different device"), true);
        return;
    }
    // Macros first so buttons can reference them.
    for (const auto &v : s.value(QStringLiteral("macros")).toList()) {
        const QVariantMap m = v.toMap();
        saveMacro(m.value(QStringLiteral("id")).toInt(), m.value(QStringLiteral("name")).toString(), m.value(QStringLiteral("events")).toList());
    }
    const QVariantList st = s.value(QStringLiteral("stages")).toList();
    if (!st.isEmpty()) {
        m_s.stages.clear();
        m_s.colors.clear();
        for (const auto &v : st) {
            const QVariantMap m = v.toMap();
            m_s.stages.push_back({m.value(QStringLiteral("x")).toInt(), m.value(QStringLiteral("y")).toInt()});
            const QColor c(m.value(QStringLiteral("color")).toString());
            m_s.colors.push_back({uint8_t(c.red()), uint8_t(c.green()), uint8_t(c.blue())});
        }
        m_s.xy = s.value(QStringLiteral("dpiXY")).toBool();
        m_s.active = std::clamp(s.value(QStringLiteral("activeStage"), 1).toInt(), 1, int(m_s.stages.size()));
        const int p = m_s.profile;
        const bool xy = m_s.xy;
        write([p, xy](Protocol &pr) { return pr.setFlag(p, SetCmd::DpiXY, xy); }, QStringLiteral("X/Y DPI"));
        writeStages();
    }
    setPollingRate(s.value(QStringLiteral("pollingRate"), m_s.polling).toInt());
    setLod(s.value(QStringLiteral("lod"), m_s.lod).toDouble());
    setMotionSync(s.value(QStringLiteral("motionSync"), m_s.motion).toBool());
    setAngleSnap(s.value(QStringLiteral("angleSnap"), m_s.snap).toBool());
    setRippleControl(s.value(QStringLiteral("rippleControl"), m_s.ripple).toBool());
    setHyperMode(s.value(QStringLiteral("hyperMode"), m_s.hyper).toBool());
    if (trackingSupported())
        setTrackingMode(s.value(QStringLiteral("trackingMode"), m_s.tracking).toBool());
    setAngleTune(s.value(QStringLiteral("angleTune"), m_s.tune).toInt());
    setDebounce(s.value(QStringLiteral("debounce"), m_s.debounce).toInt());
    setSleepSeconds(s.value(QStringLiteral("sleepSeconds"), m_s.sleep).toInt());
    for (const auto &v : s.value(QStringLiteral("buttons")).toList()) {
        const QVariantMap b = v.toMap();
        const int id = b.value(QStringLiteral("id")).toInt();
        const ButtonAction a = mapToAction(b.value(QStringLiteral("action")).toMap());
        m_s.buttons[id] = a;
        const int p = m_s.profile;
        write([p, id, a](Protocol &pr) { return pr.setButton(p, uint8_t(id), a); }, buttonName(id));
    }
    emit settingsChanged();
    emit buttonsChanged();
}

} // namespace flux
