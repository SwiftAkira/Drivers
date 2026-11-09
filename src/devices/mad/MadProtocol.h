#pragma once

// MADLIONS / FGG "DuckBread" HE keyboards (MAD LIGHT 60 HE, 373b:10f6).
// QMK VIA raw HID on usage page 0xFF60/0x61: 32-byte output + input
// reports, no report ID, no checksum, big-endian integers. Vendor commands
// are 02 96 <sub> (get) / 03 96 <sub> (set). Distances are u16 in 0.01 mm.

#include "core/HidDevice.h"

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace flux::mad {

constexpr uint16_t kVendorId = 0x373B;
constexpr uint16_t kPidMadLight60 = 0x10F6;
constexpr uint16_t kUsagePage = 0xFF60;
constexpr uint16_t kUsage = 0x61;

constexpr int kRows = 5;
constexpr int kCols = 14;
constexpr int kKeys = kRows * kCols;
constexpr int kLayers = 4;

using Report = std::array<uint8_t, 32>;

struct Features {
    uint8_t rgbArea = 0;
    bool wasdSwap = false;
    bool macMode = false;
    bool winLock = false;
    bool nkro = false;
};

struct RtInfo {
    bool on = false;
    uint16_t release = 0; // 0.01 mm
    uint16_t press = 0;   // 0.01 mm
};

struct DeadZones {
    uint16_t top = 0;     // 0.01 mm
    uint16_t bottom = 0;  // 0.01 mm
};

struct TapDance {
    uint16_t tap = 0;
    uint16_t hold = 0;
    uint16_t termMs = 200;
};

struct SocdSlot {
    uint8_t mode = 0;     // 0 none, 1 RS, 2 SOCD last, 3 key1, 4 key2, 5 neutral, 6 OKS
    uint16_t level = 0;   // actuation level, 0.01 mm
    bool globalApc = true;
    bool useRt = false;
    uint8_t k1Row = 0, k1Col = 0, k2Row = 0, k2Col = 0;
    uint8_t layer = 0;
};

struct DksAction {
    uint16_t mask = 0;    // 10-bit segment mask
    uint16_t keycode = 0;
};

struct Lighting {
    uint8_t effect = 0;
    uint8_t speed = 0;
    uint8_t r = 255, g = 255, b = 255;
    uint8_t brightness = 0;
};

struct BoxLight {
    uint8_t mode = 0;
    uint8_t colorful = 0;
    uint8_t brightness = 0;
    uint8_t speed = 0;
    uint8_t r = 0, g = 0, b = 0;
};

struct KeyTravel {
    uint16_t adc = 0;
    uint16_t trip = 0;    // 0.01 mm
    uint8_t status = 0;
};

struct Rgb {
    uint8_t r = 0, g = 0, b = 0;
};

class Protocol {
public:
    explicit Protocol(HidDevice &dev);

    // Transport
    std::optional<Report> request(const Report &req, int timeoutMs = 600);
    bool send(const Report &req);
    // Unsolicited 02 96 14 reports collected while waiting for replies or idle.
    std::deque<Report> takeEvents();
    void pumpEvents(int timeoutMs);
    static Report make(std::initializer_list<uint8_t> bytes);
    std::string lastError() const { return m_lastError; }

    // Identification / globals
    std::optional<std::string> version();
    std::optional<Features> features();
    bool setFeatures(const Features &f);
    std::optional<int> defaultLayer();
    std::optional<bool> bottomOptimize();
    bool setBottomOptimize(bool on);
    std::optional<bool> gameMode();
    bool setGameMode(bool on);
    bool factoryReset();
    bool keymapReset();

    // Travel
    std::optional<uint16_t> actuation(int row, int col);
    bool setActuation(int row, int col, uint16_t raw);
    std::optional<std::vector<uint16_t>> actuationAll();
    bool setActuationAll(const std::vector<uint16_t> &raw);
    std::optional<RtInfo> rt(int row, int col);
    bool setRt(int row, int col, const RtInfo &rt);
    std::optional<std::vector<RtInfo>> rtAll();
    bool setRtAll(const std::vector<RtInfo> &rt);
    std::optional<DeadZones> deadZones();
    bool setDeadZones(const DeadZones &dz);

    // Advanced keys
    std::optional<TapDance> tapDance(int index);
    bool setTapDance(int index, const TapDance &td);
    std::optional<SocdSlot> socd(int index);
    bool setSocd(int index, const SocdSlot &s);
    std::optional<std::array<DksAction, 4>> dks(int index);
    bool setDks(int index, const std::array<DksAction, 4> &a);

    // Keymap
    std::optional<std::vector<uint16_t>> keymapAll(); // kLayers*kKeys
    bool setKey(int layer, int row, int col, uint16_t kc);

    // Macros (VIA dynamic macro buffer)
    std::optional<int> macroCount();
    std::optional<int> macroBufferSize();
    std::optional<std::vector<uint8_t>> macroBuffer(int count, int size);
    bool setMacroBuffer(const std::vector<uint8_t> &buf);

    // Calibration / live travel / switch types
    bool calibrationStart();
    bool calibrationFinish();
    std::optional<std::vector<KeyTravel>> travelAll();
    std::optional<std::vector<uint8_t>> switchTypes();
    bool setSwitchType(int row, int col, uint8_t type);
    bool setSwitchTypeAll(uint8_t type);

    // Lighting
    std::optional<Lighting> lighting();
    bool setLighting(const Lighting &l);
    std::optional<std::vector<Rgb>> keyColors();
    // One key per packet: this firmware applies a multi-key packet that runs
    // past the end of a row onto the next row's first keys, so 8-key packets
    // make the left-hand keys flicker or go dark.
    bool setKeyColors(const std::vector<Rgb> &colors, const std::vector<bool> &present = {}, bool save = true);
    bool setKeyColor(int row, int col, const Rgb &c);
    bool saveKeyColors();
    std::optional<BoxLight> boxLight();
    bool setBoxLight(const BoxLight &b);

private:
    bool matches(const Report &req, const Report &rep) const;
    void drainStale();
    static bool isEvent(const Report &r) { return r[0] == 0x02 && r[1] == 0x96 && r[2] == 0x14; }
    void sleepMs(int ms);

    HidDevice &m_dev;
    std::deque<Report> m_events;
    std::string m_lastError;
};

} // namespace flux::mad
