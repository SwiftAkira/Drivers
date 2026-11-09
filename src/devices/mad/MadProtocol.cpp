#include "MadProtocol.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

namespace flux::mad {

namespace {
constexpr uint8_t V = 0x96; // vendor "custom id"
inline uint16_t be16(const Report &r, int o) { return uint16_t((r[o] << 8) | r[o + 1]); }
inline void put16(Report &r, int o, uint16_t v)
{
    r[o] = uint8_t(v >> 8);
    r[o + 1] = uint8_t(v);
}
using Clock = std::chrono::steady_clock;
} // namespace

Protocol::Protocol(HidDevice &dev)
    : m_dev(dev)
{
}

void Protocol::sleepMs(int ms)
{
    if (ms > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

Report Protocol::make(std::initializer_list<uint8_t> bytes)
{
    Report r{};
    size_t i = 0;
    for (uint8_t b : bytes) {
        if (i >= r.size())
            break;
        r[i++] = b;
    }
    return r;
}

bool Protocol::send(const Report &req)
{
    uint8_t buf[33];
    buf[0] = 0x00;
    std::memcpy(buf + 1, req.data(), 32);
    if (m_dev.write({buf, sizeof buf}) < 0) {
        m_lastError = "write failed: " + m_dev.lastError();
        return false;
    }
    return true;
}

bool Protocol::matches(const Report &req, const Report &rep) const
{
    if (rep[0] != req[0])
        return false;
    // Every reply echoes the request header; for buffered / indexed commands
    // the offset or slot is echoed too. Matching it stops a stale reply (for
    // example from another process polling the same device, or a late answer
    // to a timed-out request) from shifting data onto the wrong keys.
    auto same = [&](std::initializer_list<int> idx) {
        for (int i : idx)
            if (rep[i] != req[i])
                return false;
        return true;
    };
    switch (req[0]) {
    case 0x02:
    case 0x03:
        if (!same({1, 2}))
            return false;
        if (req[1] == 0x96) {
            switch (req[2]) {
            case 0x03: case 0x07: return same({3, 4});              // row, col
            case 0x0D: case 0x0E: case 0x1B: case 0x1C: case 0x1F:
                return same({5, 6, 7});                            // offset + size
            case 0x0F: return same({5, 6});                        // dks slot
            case 0x10: return same({3, 4, 5}) && (req[5] != 0x04 && req[5] != 0x06 ? true : same({6, 7}));
            case 0x20: return same({5, 6});                        // rs slot
            default: return true;
            }
        }
        return true;
    case 0x07:
    case 0x08:
        if (!same({1}))
            return false;
        if (req[1] == 0x42 || req[1] == 0x45)
            return same({2, 3});                                   // row, col
        return true;
    case 0x04:
    case 0x05:
        return same({1, 2, 3});
    case 0x0E:
    case 0x0F:
    case 0x12:
    case 0x13:
        return same({1, 2});
    case 0xFE:
        return same({1, 2, 3});
    default:
        return true;
    }
}

void Protocol::drainStale()
{
    // Throw away replies nobody is waiting for, keeping async events.
    Report rep{};
    for (int i = 0; i < 64 && m_dev.read({rep.data(), rep.size()}, 0) > 0; ++i) {
        if (isEvent(rep))
            m_events.push_back(rep);
    }
}

std::optional<Report> Protocol::request(const Report &req, int timeoutMs)
{
    for (int attempt = 0; attempt < 2; ++attempt) {
        drainStale();
        if (!send(req))
            return std::nullopt;
        const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
        while (Clock::now() < deadline) {
            Report rep{};
            const int left = int(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count());
            const int n = m_dev.read({rep.data(), rep.size()}, std::max(1, left));
            if (n < 0) {
                m_lastError = "read failed: " + m_dev.lastError();
                return std::nullopt;
            }
            if (n == 0)
                continue;
            if (rep[0] == 0x00)
                continue;
            if (isEvent(rep) && !(req[0] == 0x02 && req[2] == 0x14)) {
                m_events.push_back(rep);
                continue;
            }
            if (rep[0] == 0xFF) {
                m_lastError = "command not supported by firmware";
                return std::nullopt;
            }
            if (matches(req, rep))
                return rep;
        }
    }
    m_lastError = "keyboard didn't answer";
    return std::nullopt;
}

std::deque<Report> Protocol::takeEvents()
{
    std::deque<Report> out;
    out.swap(m_events);
    return out;
}

void Protocol::pumpEvents(int timeoutMs)
{
    Report rep{};
    const int n = m_dev.read({rep.data(), rep.size()}, timeoutMs);
    if (n > 0 && isEvent(rep))
        m_events.push_back(rep);
}

// ── Globals ──────────────────────────────────────────────────────────────

std::optional<std::string> Protocol::version()
{
    auto r = request(make({0x02, V, 0x00}));
    if (!r)
        return std::nullopt;
    char buf[16];
    std::snprintf(buf, sizeof buf, "%x.%02x", (*r)[5], (*r)[6]);
    return std::string(buf);
}

std::optional<Features> Protocol::features()
{
    auto r = request(make({0x02, V, 0x11}));
    if (!r)
        return std::nullopt;
    Features f;
    f.rgbArea = (*r)[3];
    f.wasdSwap = (*r)[4];
    f.macMode = (*r)[5];
    f.winLock = (*r)[6];
    f.nkro = (*r)[7];
    return f;
}

bool Protocol::setFeatures(const Features &f)
{
    return request(make({0x03, V, 0x11, f.rgbArea, uint8_t(f.wasdSwap), uint8_t(f.macMode), uint8_t(f.winLock), uint8_t(f.nkro)}))
        .has_value();
}

std::optional<int> Protocol::defaultLayer()
{
    auto r = request(make({0x02, V, 0x13}));
    if (!r)
        return std::nullopt;
    switch ((*r)[3]) {
    case 1: case 2: return 0;
    case 4: case 8: return 2;
    default: return std::nullopt;
    }
}

std::optional<bool> Protocol::bottomOptimize()
{
    auto r = request(make({0x02, V, 0x1D}));
    if (!r)
        return std::nullopt;
    return (*r)[3] != 0;
}

bool Protocol::setBottomOptimize(bool on)
{
    return request(make({0x03, V, 0x1D, uint8_t(on)})).has_value();
}

std::optional<bool> Protocol::gameMode()
{
    auto r = request(make({0x02, V, 0x1E}));
    if (!r)
        return std::nullopt;
    return (*r)[3] != 0;
}

bool Protocol::setGameMode(bool on)
{
    return request(make({0x03, V, 0x1E, uint8_t(on)})).has_value();
}

bool Protocol::factoryReset()
{
    return request(make({0x03, V, 0x15}), 2000).has_value();
}

bool Protocol::keymapReset()
{
    return request(make({0x06}), 2000).has_value();
}

// ── Travel ───────────────────────────────────────────────────────────────

std::optional<uint16_t> Protocol::actuation(int row, int col)
{
    auto r = request(make({0x02, V, 0x03, uint8_t(row), uint8_t(col)}));
    if (!r)
        return std::nullopt;
    return be16(*r, 5);
}

bool Protocol::setActuation(int row, int col, uint16_t raw)
{
    return request(make({0x03, V, 0x03, uint8_t(row), uint8_t(col), uint8_t(raw >> 8), uint8_t(raw)})).has_value();
}

std::optional<std::vector<uint16_t>> Protocol::actuationAll()
{
    std::vector<uint16_t> out(kKeys);
    constexpr int per = 12;
    for (int off = 0; off < kKeys; off += per) {
        auto r = request(make({0x02, V, 0x0D, 0x00, 0x00, uint8_t(off >> 8), uint8_t(off), per}));
        if (!r)
            return std::nullopt;
        for (int k = 0; k < per && off + k < kKeys; ++k)
            out[off + k] = be16(*r, 8 + 2 * k);
    }
    return out;
}

bool Protocol::setActuationAll(const std::vector<uint16_t> &raw)
{
    constexpr int per = 11;
    const int chunks = (kKeys + per - 1) / per;
    for (int c = 0; c < chunks; ++c) {
        const int off = c * per;
        const uint8_t op = chunks == 1 ? 2 : (c == 0 ? 1 : (c == chunks - 1 ? 2 : 0));
        Report req = make({0x03, V, 0x0D, 0x00, 0x00, uint8_t(off >> 8), uint8_t(off), per, op});
        for (int k = 0; k < per && off + k < kKeys; ++k)
            put16(req, 9 + 2 * k, off + k < int(raw.size()) ? raw[off + k] : 0);
        if (!request(req, 1500))
            return false;
        if (c == 0 || c == chunks - 1)
            sleepMs(200);
    }
    return true;
}

std::optional<RtInfo> Protocol::rt(int row, int col)
{
    auto r = request(make({0x02, V, 0x07, uint8_t(row), uint8_t(col)}));
    if (!r)
        return std::nullopt;
    return RtInfo{(*r)[5] != 0, be16(*r, 6), be16(*r, 8)};
}

bool Protocol::setRt(int row, int col, const RtInfo &rt)
{
    return request(make({0x03, V, 0x07, uint8_t(row), uint8_t(col), uint8_t(rt.on), uint8_t(rt.release >> 8), uint8_t(rt.release),
                         uint8_t(rt.press >> 8), uint8_t(rt.press)}))
        .has_value();
}

std::optional<std::vector<RtInfo>> Protocol::rtAll()
{
    std::vector<RtInfo> out(kKeys);
    constexpr int per = 4;
    for (int off = 0; off < kKeys; off += per) {
        auto r = request(make({0x02, V, 0x0E, 0x00, 0x00, uint8_t(off >> 8), uint8_t(off), per}));
        if (!r)
            return std::nullopt;
        for (int k = 0; k < per && off + k < kKeys; ++k) {
            const int o = 8 + 5 * k;
            out[off + k] = RtInfo{(*r)[o] != 0, be16(*r, o + 1), be16(*r, o + 3)};
        }
    }
    return out;
}

bool Protocol::setRtAll(const std::vector<RtInfo> &rt)
{
    constexpr int per = 4;
    const int chunks = (kKeys + per - 1) / per;
    for (int c = 0; c < chunks; ++c) {
        const int off = c * per;
        const uint8_t op = chunks == 1 ? 2 : (c == 0 ? 1 : (c == chunks - 1 ? 2 : 0));
        Report req = make({0x03, V, 0x0E, 0x00, 0x00, uint8_t(off >> 8), uint8_t(off), per, op});
        for (int k = 0; k < per && off + k < kKeys; ++k) {
            const RtInfo &i = off + k < int(rt.size()) ? rt[off + k] : RtInfo{};
            const int o = 9 + 5 * k;
            req[o] = uint8_t(i.on);
            put16(req, o + 1, i.release);
            put16(req, o + 3, i.press);
        }
        if (!request(req, 1500))
            return false;
        if (c == 0 || c == chunks - 1)
            sleepMs(200);
    }
    return true;
}

std::optional<DeadZones> Protocol::deadZones()
{
    auto r = request(make({0x02, V, 0x0B}));
    if (!r)
        return std::nullopt;
    return DeadZones{be16(*r, 3), be16(*r, 5)};
}

bool Protocol::setDeadZones(const DeadZones &dz)
{
    return request(make({0x03, V, 0x0B, uint8_t(dz.top >> 8), uint8_t(dz.top), uint8_t(dz.bottom >> 8), uint8_t(dz.bottom)})).has_value();
}

// ── Advanced keys ────────────────────────────────────────────────────────

std::optional<TapDance> Protocol::tapDance(int index)
{
    auto r = request(make({0xFE, 0x0D, 0x01, uint8_t(index)}));
    if (!r)
        return std::nullopt;
    return TapDance{be16(*r, 4), be16(*r, 6), be16(*r, 8)};
}

bool Protocol::setTapDance(int index, const TapDance &td)
{
    return request(make({0xFE, 0x0D, 0x02, uint8_t(index), uint8_t(td.tap >> 8), uint8_t(td.tap), uint8_t(td.hold >> 8),
                         uint8_t(td.hold), uint8_t(td.termMs >> 8), uint8_t(td.termMs)}))
        .has_value();
}

std::optional<SocdSlot> Protocol::socd(int index)
{
    auto r = request(make({0x02, V, 0x20, 0x00, 0x00, 0x01, uint8_t(index), 0x00, uint8_t(index)}));
    if (!r)
        return std::nullopt;
    SocdSlot s;
    s.mode = (*r)[7];
    s.level = be16(*r, 9);
    s.globalApc = (*r)[11];
    s.useRt = (*r)[12];
    s.k1Row = (*r)[13];
    s.k1Col = (*r)[14];
    s.k2Row = (*r)[15];
    s.k2Col = (*r)[16];
    s.layer = (*r)[17];
    return s;
}

bool Protocol::setSocd(int index, const SocdSlot &s)
{
    Report req = make({0x03, V, 0x20, 0x00, 0x00, 0x03, uint8_t(index), s.mode, uint8_t(index % 26)});
    put16(req, 9, s.level);
    req[11] = uint8_t(s.globalApc);
    req[12] = uint8_t(s.useRt);
    req[13] = s.k1Row;
    req[14] = s.k1Col;
    req[15] = s.k2Row;
    req[16] = s.k2Col;
    req[17] = s.layer;
    return request(req).has_value();
}

std::optional<std::array<DksAction, 4>> Protocol::dks(int index)
{
    auto r = request(make({0x02, V, 0x0F, 0x00, 0x00, 0x01, uint8_t(index)}));
    if (!r)
        return std::nullopt;
    std::array<DksAction, 4> a{};
    for (int k = 0; k < 4; ++k)
        a[k] = DksAction{uint16_t(be16(*r, 7 + 4 * k) & 0x03FF), be16(*r, 9 + 4 * k)};
    return a;
}

bool Protocol::setDks(int index, const std::array<DksAction, 4> &a)
{
    Report req = make({0x03, V, 0x0F, 0x00, 0x00, 0x03, uint8_t(index)});
    for (int k = 0; k < 4; ++k) {
        put16(req, 7 + 4 * k, a[k].mask & 0x03FF);
        put16(req, 9 + 4 * k, a[k].keycode);
    }
    return request(req).has_value();
}

// ── Keymap ───────────────────────────────────────────────────────────────

std::optional<std::vector<uint16_t>> Protocol::keymapAll()
{
    constexpr int total = kLayers * kKeys * 2;
    constexpr int per = 28;
    std::vector<uint16_t> out(kLayers * kKeys);
    for (int off = 0; off < total; off += per) {
        const int len = std::min(per, total - off);
        auto r = request(make({0x12, uint8_t(off >> 8), uint8_t(off), uint8_t(len)}));
        if (!r)
            return std::nullopt;
        for (int k = 0; k < len / 2; ++k)
            out[off / 2 + k] = be16(*r, 4 + 2 * k);
    }
    return out;
}

bool Protocol::setKey(int layer, int row, int col, uint16_t kc)
{
    return request(make({0x05, uint8_t(layer), uint8_t(row), uint8_t(col), uint8_t(kc >> 8), uint8_t(kc)})).has_value();
}

// ── Macros ───────────────────────────────────────────────────────────────

std::optional<int> Protocol::macroCount()
{
    auto r = request(make({0x0C}));
    if (!r)
        return std::nullopt;
    return (*r)[1];
}

std::optional<int> Protocol::macroBufferSize()
{
    auto r = request(make({0x0D}));
    if (!r)
        return std::nullopt;
    return be16(*r, 1);
}

std::optional<std::vector<uint8_t>> Protocol::macroBuffer(int count, int size)
{
    std::vector<uint8_t> out;
    int nul = 0;
    constexpr int per = 28;
    for (int off = 0; off < size; off += per) {
        const int len = std::min(per, size - off);
        auto r = request(make({0x0E, uint8_t(off >> 8), uint8_t(off), uint8_t(len)}));
        if (!r)
            return std::nullopt;
        for (int k = 0; k < len; ++k) {
            out.push_back((*r)[4 + k]);
            if ((*r)[4 + k] == 0)
                ++nul;
        }
        if (nul >= count)
            break;
    }
    return out;
}

bool Protocol::setMacroBuffer(const std::vector<uint8_t> &buf)
{
    constexpr int per = 28;
    for (size_t off = 0; off < buf.size(); off += per) {
        const int len = int(std::min<size_t>(per, buf.size() - off));
        Report req = make({0x0F, uint8_t(off >> 8), uint8_t(off), uint8_t(len)});
        std::memcpy(req.data() + 4, buf.data() + off, len);
        if (!request(req))
            return false;
    }
    return true;
}

// ── Calibration / live travel / switch types ─────────────────────────────

bool Protocol::calibrationStart()
{
    const bool ok = request(make({0x03, V, 0x18})).has_value();
    sleepMs(200);
    return ok;
}

bool Protocol::calibrationFinish()
{
    return request(make({0x03, V, 0x19}), 2000).has_value();
}

std::optional<std::vector<KeyTravel>> Protocol::travelAll()
{
    std::vector<KeyTravel> out(kKeys);
    constexpr int per = 4;
    for (int off = 0; off < kKeys; off += per) {
        auto r = request(make({0x02, V, 0x1C, 0x00, 0x00, uint8_t(off >> 8), uint8_t(off), per}), 250);
        if (!r)
            return std::nullopt;
        for (int k = 0; k < per && off + k < kKeys; ++k) {
            const int o = 8 + 5 * k;
            out[off + k] = KeyTravel{be16(*r, o), be16(*r, o + 2), (*r)[o + 4]};
        }
    }
    return out;
}

std::optional<std::vector<uint8_t>> Protocol::switchTypes()
{
    std::vector<uint8_t> out(kKeys);
    constexpr int per = 23;
    for (int off = 0; off < kKeys; off += per) {
        auto r = request(make({0x02, V, 0x10, 0x00, 0x00, 0x04, uint8_t(off >> 8), uint8_t(off), per}));
        if (!r)
            return std::nullopt;
        for (int k = 0; k < per && off + k < kKeys; ++k)
            out[off + k] = (*r)[9 + k];
    }
    return out;
}

bool Protocol::setSwitchType(int row, int col, uint8_t type)
{
    return request(make({0x02, V, 0x10, 0x00, 0x00, 0x01})) &&
           request(make({0x03, V, 0x10, uint8_t(row), uint8_t(col), 0x05, type})) &&
           request(make({0x03, V, 0x10, 0x00, 0x00, 0x02}), 1500);
}

bool Protocol::setSwitchTypeAll(uint8_t type)
{
    return request(make({0x02, V, 0x10, 0x00, 0x00, 0x01})) &&
           request(make({0x03, V, 0x10, 0x00, 0x00, 0x07, type})) &&
           request(make({0x03, V, 0x10, 0x00, 0x00, 0x02}), 1500);
}

// ── Lighting ─────────────────────────────────────────────────────────────

std::optional<Lighting> Protocol::lighting()
{
    auto r = request(make({0x08, 0x41}));
    if (!r)
        return std::nullopt;
    Lighting l;
    l.effect = (*r)[2];
    l.speed = (*r)[4];
    l.r = (*r)[5];
    l.g = (*r)[6];
    l.b = (*r)[7];
    l.brightness = (*r)[8];
    return l;
}

bool Protocol::setLighting(const Lighting &l)
{
    return request(make({0x07, 0x41, l.effect, 0x00, l.speed, l.r, l.g, l.b, l.brightness})).has_value();
}

std::optional<std::vector<Rgb>> Protocol::keyColors()
{
    std::vector<Rgb> out(kKeys);
    for (int row = 0; row < kRows; ++row) {
        for (int col : {0, 8}) {
            auto r = request(make({0x08, 0x45, uint8_t(row), uint8_t(col), 8}));
            if (!r)
                return std::nullopt;
            for (int k = 0; k < 8 && col + k < kCols; ++k)
                out[row * kCols + col + k] = Rgb{(*r)[5 + 3 * k], (*r)[6 + 3 * k], (*r)[7 + 3 * k]};
        }
    }
    return out;
}

bool Protocol::setKeyColors(const std::vector<Rgb> &colors, const std::vector<bool> &present, bool save)
{
    for (int i = 0; i < kKeys && i < int(colors.size()); ++i) {
        if (!present.empty() && (i >= int(present.size()) || !present[i]))
            continue;
        if (!setKeyColor(i / kCols, i % kCols, colors[i]))
            return false;
    }
    return save ? saveKeyColors() : true;
}

bool Protocol::setKeyColor(int row, int col, const Rgb &c)
{
    return request(make({0x07, 0x42, uint8_t(row), uint8_t(col), 1, c.r, c.g, c.b})).has_value();
}

bool Protocol::saveKeyColors()
{
    const bool ok = request(make({0x03, V, 0x12})).has_value();
    sleepMs(100);
    return ok;
}

std::optional<BoxLight> Protocol::boxLight()
{
    auto r = request(make({0x02, V, 0x21}));
    if (!r)
        return std::nullopt;
    return BoxLight{(*r)[3], (*r)[4], (*r)[5], (*r)[6], (*r)[7], (*r)[8], (*r)[9]};
}

bool Protocol::setBoxLight(const BoxLight &b)
{
    return request(make({0x03, V, 0x21, b.mode, b.colorful, b.brightness, b.speed, b.r, b.g, b.b})).has_value();
}

} // namespace flux::mad
