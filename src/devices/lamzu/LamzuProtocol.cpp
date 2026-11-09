#include "LamzuProtocol.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <thread>

namespace flux::lamzu {

namespace {
constexpr uint8_t kStatusOk = 0xA1;
constexpr uint8_t kStatusAlt = 0x02;
} // namespace

Protocol::Protocol(HidDevice &config)
    : m_dev(config)
{
}

void Protocol::sleepMs(int ms)
{
    if (ms > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

Packet Protocol::make(Target t, uint8_t len, uint8_t cls, uint8_t cmd, std::initializer_list<uint8_t> data)
{
    Packet p{};
    p[2] = t;
    p[3] = len;
    p[4] = cls;
    p[5] = cmd;
    size_t i = 6;
    for (uint8_t b : data) {
        if (i >= p.size())
            break;
        p[i++] = b;
    }
    return p;
}

bool Protocol::setFeature(const Packet &req)
{
    uint8_t buf[65];
    buf[0] = 0x00;
    std::memcpy(buf + 1, req.data(), 64);
    const int r = m_dev.sendFeature({buf, sizeof buf});
    if (r < 0) {
        m_lastError = "write failed: " + m_dev.lastError();
        return false;
    }
    return true;
}

bool Protocol::getFeature(Packet &out)
{
    uint8_t buf[65] = {};
    buf[0] = 0x00;
    const int r = m_dev.getFeature({buf, sizeof buf});
    if (r < 2) {
        m_lastError = "read failed: " + m_dev.lastError();
        return false;
    }
    // hidraw keeps the (zero) report number in buf[0] for unnumbered reports.
    std::memcpy(out.data(), buf + 1, 64);
    return true;
}

std::optional<Packet> Protocol::transact(const Packet &req, int firstDelayMs, int maxResends, int pollsPerSend, int pollDelayMs)
{
    auto matches = [&](const Packet &r) {
        return r[0] == kStatusOk && r[4] == req[4] && r[5] == req[5];
    };
    for (int send = 0; send <= maxResends; ++send) {
        if (!setFeature(req))
            return std::nullopt;
        sleepMs(send == 0 ? firstDelayMs : pollDelayMs);
        Packet r{};
        for (int poll = 0; poll < pollsPerSend; ++poll) {
            if (!getFeature(r))
                return std::nullopt;
            if (matches(r))
                return r;
            // Some firmware answers 0x02 with a valid payload (bootloader-ish paths).
            if (r[0] == kStatusAlt && r[4] == req[4] && r[5] == req[5])
                return r;
            if (r[0] > kStatusOk)
                break; // error/busy: resend
            sleepMs(pollDelayMs);
        }
    }
    m_lastError = "no reply from device";
    return std::nullopt;
}

bool Protocol::sendOnly(const Packet &req)
{
    return setFeature(req);
}

// ── Identification ───────────────────────────────────────────────────────

std::optional<std::array<int, 4>> Protocol::firmware(Target t, bool quick)
{
    auto r = quick ? transact(make(t, 0x10, 0x00, 0x81), 20, 1, 8, 20) : transact(make(t, 0x10, 0x00, 0x81), 30, 3, 20, 20);
    if (!r)
        return std::nullopt;
    std::array<int, 4> v{(*r)[6], (*r)[7], (*r)[8], (*r)[9]};
    if (v[0] == 0 && v[1] == 0 && v[2] == 0 && v[3] == 0)
        return std::nullopt;
    return v;
}

std::optional<int> Protocol::sensorModel()
{
    auto r = transact(make(Mouse, 0x01, 0x01, 0x8F));
    if (!r)
        return std::nullopt;
    return (*r)[6];
}

std::optional<int> Protocol::eid()
{
    auto r = transact(make(Mouse, 0x01, 0x00, 0x82));
    if (!r)
        return std::nullopt;
    return (*r)[6] == 0xFF ? 0 : (*r)[6];
}

std::optional<std::pair<uint16_t, uint16_t>> Protocol::pairedVidPid()
{
    auto r = transact(make(ReceiverRf, 0x06, 0x00, 0x8B, {0x02}));
    if (!r)
        return std::nullopt;
    const uint16_t vid = ((*r)[8] << 8) | (*r)[9];
    const uint16_t pid = ((*r)[10] << 8) | (*r)[11];
    return std::make_pair(vid, pid);
}

std::optional<std::pair<bool, int>> Protocol::battery()
{
    auto r = transact(make(Mouse, 0x02, 0x00, 0x83), 30, 3, 15, 30);
    if (!r || (*r)[3] != 0x02)
        return std::nullopt;
    return std::make_pair((*r)[6] == 1, std::min<int>((*r)[7], 100));
}

// ── Profile ──────────────────────────────────────────────────────────────

std::optional<int> Protocol::profile()
{
    for (int i = 0; i < 5; ++i) {
        auto r = transact(make(Mouse, 0x01, 0x00, 0x85));
        if (r && (*r)[6] >= 1)
            return (*r)[6];
        sleepMs(30);
    }
    return std::nullopt;
}

bool Protocol::setProfile(int p)
{
    return transact(make(Mouse, 0x01, 0x00, 0x05, {uint8_t(p)})).has_value();
}

// ── Scalars ──────────────────────────────────────────────────────────────

int Protocol::pollingCodeToHz(uint8_t code)
{
    switch (code) {
    case 0x08: return 125;
    case 0x04: return 250;
    case 0x02: return 500;
    case 0x01: return 1000;
    case 0x10: return 1000;
    case 0x20: return 2000;
    case 0x40: return 4000;
    case 0x80: return 8000;
    default: return 0;
    }
}

uint8_t Protocol::pollingHzToCode(int hz)
{
    switch (hz) {
    case 125: return 0x08;
    case 250: return 0x04;
    case 500: return 0x02;
    case 2000: return 0x20;
    case 4000: return 0x40;
    case 8000: return 0x80;
    default: return 0x01;
    }
}

std::optional<int> Protocol::pollingRate(int p)
{
    auto r = transact(make(Mouse, 0x02, 0x01, 0x80, {uint8_t(p)}));
    if (!r)
        return std::nullopt;
    const int hz = pollingCodeToHz((*r)[7]);
    if (!hz)
        return std::nullopt;
    return hz;
}

bool Protocol::setPollingRate(int p, int hz)
{
    return transact(make(Mouse, 0x02, 0x01, 0x00, {uint8_t(p), pollingHzToCode(hz)})).has_value();
}

std::optional<int> Protocol::debounce(int p)
{
    auto r = transact(make(Mouse, 0x02, 0x00, 0x88, {uint8_t(p)}));
    if (!r)
        return std::nullopt;
    return (*r)[7];
}

bool Protocol::setDebounce(int p, int ms)
{
    ms = std::clamp(ms, 0, 15);
    return transact(make(Mouse, 0x02, 0x00, 0x08, {uint8_t(p), uint8_t(ms)})).has_value();
}

std::optional<int> Protocol::sleepSeconds(int p)
{
    auto r = transact(make(Mouse, 0x03, 0x00, 0x87, {uint8_t(p)}));
    if (!r)
        return std::nullopt;
    const int s = ((*r)[7] << 8) | (*r)[8];
    if (s == 0xFFFF || s == 0xFF00)
        return 0;
    return s;
}

bool Protocol::setSleepSeconds(int p, int s)
{
    s = std::clamp(s, 1, 0xFFFE);
    return transact(make(Mouse, 0x03, 0x00, 0x07, {uint8_t(p), uint8_t(s >> 8), uint8_t(s & 0xFF)})).has_value();
}

uint8_t Protocol::encodeLod(double mm)
{
    if (mm < 0.95)
        return uint8_t(std::lround(mm * 10)) | 0x80;
    if (mm > 1.05 && mm < 1.95)
        return uint8_t((int(std::floor(mm)) << 4) | (int(std::lround(mm * 10)) % 10));
    return uint8_t(std::lround(mm));
}

double Protocol::decodeLod(uint8_t v, int sensor)
{
    if (v >= 0x80)
        return (v - 0x80) / 10.0;
    if (sensor == 4 && v != 1 && v != 2)
        return ((v >> 4) * 10 + (v & 0x0F)) / 10.0;
    return v;
}

std::optional<double> Protocol::lod(int p, int sensor)
{
    auto r = transact(make(Mouse, 0x02, 0x01, 0x88, {uint8_t(p)}));
    if (!r)
        return std::nullopt;
    return decodeLod((*r)[7], sensor);
}

bool Protocol::setLod(int p, double mm)
{
    return transact(make(Mouse, 0x02, 0x01, 0x08, {uint8_t(p), encodeLod(mm)})).has_value();
}

std::optional<bool> Protocol::flag(int p, uint8_t setCmd)
{
    auto r = transact(make(Mouse, 0x02, 0x01, uint8_t(setCmd | 0x80), {uint8_t(p)}));
    if (!r)
        return std::nullopt;
    return (*r)[7] == 1;
}

bool Protocol::setFlag(int p, uint8_t setCmd, bool on)
{
    return transact(make(Mouse, 0x02, 0x01, setCmd, {uint8_t(p), uint8_t(on ? 1 : 0)})).has_value();
}

std::optional<int> Protocol::angleTune(int p)
{
    auto r = transact(make(Mouse, 0x02, 0x01, 0x94, {uint8_t(p)}));
    if (!r)
        return std::nullopt;
    const int v = (*r)[7];
    return v <= 30 ? v : v - 256;
}

bool Protocol::setAngleTune(int p, int deg)
{
    deg = std::clamp(deg, -30, 30);
    return transact(make(Mouse, 0x02, 0x01, 0x14, {uint8_t(p), uint8_t(int8_t(deg))})).has_value();
}

// ── DPI ──────────────────────────────────────────────────────────────────

std::optional<std::vector<Stage>> Protocol::stages(int p)
{
    auto r = transact(make(Mouse, 0x0A, 0x01, 0x81, {uint8_t(p), 0x06}), 100);
    if (!r)
        return std::nullopt;
    const int n = std::clamp<int>((*r)[7], 0, kMaxStages);
    if (n == 0)
        return std::nullopt;
    std::vector<Stage> out;
    for (int k = 0; k < n; ++k) {
        const int o = 8 + 4 * k;
        Stage s;
        s.x = ((*r)[o] << 8) | (*r)[o + 1];
        s.y = ((*r)[o + 2] << 8) | (*r)[o + 3];
        if (s.y == 0)
            s.y = s.x;
        out.push_back(s);
    }
    return out;
}

bool Protocol::setStages(int p, const std::vector<Stage> &st)
{
    const int n = std::clamp<int>(int(st.size()), 1, kMaxStages);
    Packet req = make(Mouse, 0x1A, 0x01, 0x01, {uint8_t(p), uint8_t(n)});
    for (int k = 0; k < n; ++k) {
        const int o = 8 + 4 * k;
        req[o] = uint8_t(st[k].x >> 8);
        req[o + 1] = uint8_t(st[k].x);
        req[o + 2] = uint8_t(st[k].y >> 8);
        req[o + 3] = uint8_t(st[k].y);
    }
    return transact(req).has_value();
}

std::optional<int> Protocol::activeStage(int p)
{
    auto r = transact(make(Mouse, 0x02, 0x01, 0x82, {uint8_t(p)}));
    if (!r)
        return std::nullopt;
    return (*r)[7];
}

bool Protocol::setActiveStage(int p, int s)
{
    return transact(make(Mouse, 0x02, 0x01, 0x02, {uint8_t(p), uint8_t(s)})).has_value();
}

std::optional<std::vector<Rgb>> Protocol::stageColors(int p, int count)
{
    auto r = transact(make(Mouse, 0x13, 0x02, 0x81, {uint8_t(p)}), 30, 3, 15, 30);
    if (!r)
        return std::nullopt;
    std::vector<Rgb> out;
    for (int k = 0; k < std::min(count, kMaxStages); ++k)
        out.push_back({(*r)[7 + 3 * k], (*r)[8 + 3 * k], (*r)[9 + 3 * k]});
    return out;
}

bool Protocol::setStageColors(int p, const std::vector<Rgb> &colors)
{
    Packet req = make(Mouse, 0x13, 0x02, 0x01, {uint8_t(p)});
    for (size_t k = 0; k < colors.size() && k < size_t(kMaxStages); ++k) {
        req[7 + 3 * k] = colors[k].r;
        req[8 + 3 * k] = colors[k].g;
        req[9 + 3 * k] = colors[k].b;
    }
    return transact(req, 30, 3, 15, 30).has_value();
}

// ── Buttons ──────────────────────────────────────────────────────────────

std::optional<ButtonAction> Protocol::button(int p, uint8_t btn)
{
    auto r = transact(make(Mouse, 0x0F, 0x03, 0x80, {uint8_t(p), btn, 0x00, 0xFF, 0x0A}));
    if (!r)
        return std::nullopt;
    ButtonAction a;
    a.type = (*r)[9];
    const int plen = std::min<int>((*r)[10], 64 - 11);
    a.params.assign(r->begin() + 11, r->begin() + 11 + plen);
    // Trim by type where the firmware reports a generous PLEN.
    size_t want = a.params.size();
    switch (a.type) {
    case ActionType::Disabled: want = 0; break;
    case ActionType::Mouse: case ActionType::ProfileCycle: case ActionType::PollingCycle:
    case ActionType::LodCycle: case ActionType::HyperCycle: want = 1; break;
    case ActionType::Keyboard: case ActionType::Consumer: case ActionType::MacroHold:
    case ActionType::MacroToggle: want = 2; break;
    case ActionType::MacroTimes: want = 3; break;
    case ActionType::XClick: want = 5; break;
    case ActionType::Dpi: want = (!a.params.empty() && a.params[0] == 0x05) ? 5 : 1; break;
    default: break;
    }
    if (a.params.size() > want)
        a.params.resize(want);
    return a;
}

bool Protocol::setButton(int p, uint8_t btn, const ButtonAction &a)
{
    const uint8_t plen = uint8_t(a.params.size());
    Packet req = make(Mouse, uint8_t(5 + plen), 0x03, 0x00, {uint8_t(p), btn, 0x00, a.type, plen});
    for (size_t i = 0; i < a.params.size() && 11 + i < req.size(); ++i)
        req[11 + i] = a.params[i];
    return transact(req).has_value();
}

// ── Macros ───────────────────────────────────────────────────────────────

std::optional<uint32_t> Protocol::macroSize(int id)
{
    auto r = transact(make(Mouse, 0x06, 0x04, 0x81, {uint8_t(id >> 8), uint8_t(id)}), 10);
    if (!r)
        return std::nullopt;
    return (uint32_t((*r)[8]) << 24) | (uint32_t((*r)[9]) << 16) | (uint32_t((*r)[10]) << 8) | (*r)[11];
}

std::optional<std::vector<uint8_t>> Protocol::readMacro(int id)
{
    auto size = macroSize(id);
    if (!size)
        return std::nullopt;
    std::vector<uint8_t> out;
    if (*size == 0 || *size > 64 * 1024)
        return out;
    for (uint32_t off = 0; off < *size; off += kMacroChunkRead) {
        const uint8_t cl = uint8_t(std::min<uint32_t>(kMacroChunkRead, *size - off));
        auto r = transact(make(Mouse, uint8_t(7 + cl), 0x04, 0x83,
                               {uint8_t(id >> 8), uint8_t(id), uint8_t(off >> 24), uint8_t(off >> 16), uint8_t(off >> 8),
                                uint8_t(off), cl}),
                          30, 3, 15, 30);
        if (!r)
            return std::nullopt;
        out.insert(out.end(), r->begin() + 13, r->begin() + 13 + cl);
    }
    return out;
}

bool Protocol::writeMacro(int id, const std::vector<uint8_t> &data)
{
    const uint32_t size = uint32_t(data.size());
    sleepMs(30);
    if (!transact(make(Mouse, 0x06, 0x04, 0x01,
                       {uint8_t(id >> 8), uint8_t(id), uint8_t(size >> 24), uint8_t(size >> 16), uint8_t(size >> 8), uint8_t(size)}),
                  30))
        return false;
    for (uint32_t off = 0; off < size; off += kMacroChunkWrite) {
        const uint8_t cl = uint8_t(std::min<uint32_t>(kMacroChunkWrite, size - off));
        Packet req = make(Mouse, uint8_t(7 + cl), 0x04, 0x03,
                          {uint8_t(id >> 8), uint8_t(id), uint8_t(off >> 24), uint8_t(off >> 16), uint8_t(off >> 8), uint8_t(off), cl});
        std::memcpy(req.data() + 13, data.data() + off, cl);
        if (!transact(req, 30, 3, 15, 30))
            return false;
    }
    return true;
}

bool Protocol::deleteMacro(int id)
{
    return transact(make(Mouse, 0x02, 0x04, 0x02, {uint8_t(id >> 8), uint8_t(id)}), 30).has_value();
}

// ── Reset ────────────────────────────────────────────────────────────────

bool Protocol::factoryReset(bool wireless)
{
    if (!sendOnly(make(Mouse, 0x02, 0x00, 0x00, {0xC0, 0x01})))
        return false;
    if (wireless) {
        sleepMs(1000);
        return sendOnly(make(ReceiverUsb, 0x02, 0x00, 0x00, {0xC0, 0x00}));
    }
    return true;
}

// ── Events ───────────────────────────────────────────────────────────────

Event Protocol::parseEvent(const uint8_t *d, int len)
{
    Event e;
    if (len < 3 || d[0] != 0x04)
        return e;
    switch (d[1]) {
    case 0x01:
        if (len >= 7) {
            e.kind = Event::StageChanged;
            e.a = d[2];
            e.b = (d[3] << 8) | d[4];
            e.c = (d[5] << 8) | d[6];
        }
        break;
    case 0x02: e.kind = Event::ProfileChanged; e.a = d[2]; break;
    case 0x03:
        if (len >= 4 && d[2] > 0) {
            e.kind = Event::Battery;
            e.a = std::min<int>(d[2], 100);
            e.b = d[3];
        }
        break;
    case 0x06: e.kind = Event::Link; e.a = d[2]; break;
    case 0x07: e.kind = Event::Lod; e.a = d[2]; break;
    case 0x08: e.kind = Event::Polling; e.a = pollingCodeToHz(d[2]); break;
    case 0x0D:
        if (len >= 4) {
            e.kind = Event::Hyper;
            e.a = d[2];
            e.b = d[3];
        }
        break;
    default: break;
    }
    return e;
}

} // namespace flux::lamzu
