#pragma once

// LAMZU "Aurora" (new protocol, VID 0x37B0) — INCA 8K and siblings.
// Transport: one unnumbered 64-byte feature report on the vendor
// interface (usage page 0xFFFF, usage 0x00). No checksum.
//   request : 00 00 TARGET LEN CLASS CMD data...   (CMD bit7 = GET)
//   reply   : A1 .. .. LEN CLASS CMD data...
// Asynchronous events arrive as input report 4 on the 0xFFA0 interface.

#include "core/HidDevice.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace flux::lamzu {

constexpr uint16_t kVendorId = 0x37B0;
constexpr uint16_t kPidWired = 0x0009;
constexpr uint16_t kPid1K = 0x000F;
constexpr uint16_t kPid1KAlt = 0x0003;
constexpr uint16_t kPid8K = 0x0010;
constexpr uint16_t kPid8KAlt = 0x0001;
constexpr uint16_t kConfigUsagePage = 0xFFFF;
constexpr uint16_t kConfigUsage = 0x0000;
constexpr uint16_t kNotifyUsagePage = 0xFFA0;
constexpr uint16_t kNotifyUsage = 0x0001;

enum Target : uint8_t { ReceiverUsb = 0, ReceiverRf = 1, Mouse = 2 };

constexpr int kMaxStages = 6;
constexpr int kMaxMacros = 30;
constexpr int kMacroChunkWrite = 51;
constexpr int kMacroChunkRead = 50;

using Packet = std::array<uint8_t, 64>;

struct Stage {
    int x = 400;
    int y = 400;
};

struct Rgb {
    uint8_t r = 0, g = 0, b = 0;
};

struct ButtonAction {
    uint8_t type = 0;               // see ActionType
    std::vector<uint8_t> params;    // raw params (PLEN = size)
};

namespace ActionType {
constexpr uint8_t Disabled = 0x00;
constexpr uint8_t Mouse = 0x01;
constexpr uint8_t XClick = 0x02;
constexpr uint8_t Keyboard = 0x04;
constexpr uint8_t Consumer = 0x05;
constexpr uint8_t Dpi = 0x07;
constexpr uint8_t ProfileCycle = 0x08;
constexpr uint8_t PollingCycle = 0x0D;
constexpr uint8_t LodCycle = 0x0E;
constexpr uint8_t MacroTimes = 0x10;
constexpr uint8_t MacroHold = 0x11;
constexpr uint8_t MacroToggle = 0x12;
constexpr uint8_t HyperCycle = 0x14;
} // namespace ActionType

namespace Button {
constexpr uint8_t Left = 0x01;
constexpr uint8_t Right = 0x02;
constexpr uint8_t Middle = 0x03;
constexpr uint8_t Back = 0x04;
constexpr uint8_t Forward = 0x05;
constexpr uint8_t DpiButton = 0x14;
} // namespace Button

// Event pushed by the receiver/mouse (input report 4).
struct Event {
    enum Kind { None, StageChanged, ProfileChanged, Battery, Link, Lod, Polling, Hyper } kind = None;
    int a = 0, b = 0, c = 0;
};

class Protocol {
public:
    explicit Protocol(HidDevice &config);

    // Low level --------------------------------------------------------
    // Sends `req` and polls the reply until status A1 with matching
    // class/cmd. `firstDelayMs` is the wait before the first GET.
    std::optional<Packet> transact(const Packet &req, int firstDelayMs = 15, int maxResends = 4, int pollsPerSend = 30,
                                   int pollDelayMs = 15);
    bool sendOnly(const Packet &req);
    static Packet make(Target t, uint8_t len, uint8_t cls, uint8_t cmd, std::initializer_list<uint8_t> data = {});

    // Identification ---------------------------------------------------
    std::optional<std::array<int, 4>> firmware(Target t, bool quick = false);
    std::optional<int> sensorModel();
    std::optional<int> eid();
    std::optional<std::pair<uint16_t, uint16_t>> pairedVidPid();
    std::optional<std::pair<bool, int>> battery(); // charging, percent

    // Profile ----------------------------------------------------------
    std::optional<int> profile();
    bool setProfile(int p);

    // Per-profile scalar settings ---------------------------------------
    std::optional<int> pollingRate(int p);   // Hz
    bool setPollingRate(int p, int hz);
    std::optional<int> debounce(int p);
    bool setDebounce(int p, int ms);
    std::optional<int> sleepSeconds(int p);
    bool setSleepSeconds(int p, int s);
    std::optional<double> lod(int p, int sensor);
    bool setLod(int p, double mm);
    std::optional<bool> flag(int p, uint8_t setCmd);          // class 01 booleans
    bool setFlag(int p, uint8_t setCmd, bool on);
    std::optional<int> angleTune(int p);
    bool setAngleTune(int p, int deg);

    // DPI ----------------------------------------------------------------
    std::optional<std::vector<Stage>> stages(int p);
    bool setStages(int p, const std::vector<Stage> &stages);
    std::optional<int> activeStage(int p);
    bool setActiveStage(int p, int s);
    std::optional<std::vector<Rgb>> stageColors(int p, int count);
    bool setStageColors(int p, const std::vector<Rgb> &colors);

    // Buttons ------------------------------------------------------------
    std::optional<ButtonAction> button(int p, uint8_t btn);
    bool setButton(int p, uint8_t btn, const ButtonAction &a);

    // Macros -------------------------------------------------------------
    std::optional<uint32_t> macroSize(int id);
    std::optional<std::vector<uint8_t>> readMacro(int id);
    bool writeMacro(int id, const std::vector<uint8_t> &data);
    bool deleteMacro(int id);

    // Reset --------------------------------------------------------------
    bool factoryReset(bool wireless);

    // Events -------------------------------------------------------------
    static Event parseEvent(const uint8_t *data, int len);

    static int pollingCodeToHz(uint8_t code);
    static uint8_t pollingHzToCode(int hz);
    static uint8_t encodeLod(double mm);
    static double decodeLod(uint8_t v, int sensor);

    std::string lastError() const { return m_lastError; }

private:
    bool setFeature(const Packet &req);
    bool getFeature(Packet &out);
    void sleepMs(int ms);

    HidDevice &m_dev;
    std::string m_lastError;
};

namespace SetCmd {
constexpr uint8_t AngleSnap = 0x04;
constexpr uint8_t MotionSync = 0x09;
constexpr uint8_t Ripple = 0x0A;
constexpr uint8_t Hyper = 0x0B;
constexpr uint8_t DpiXY = 0x0D;
constexpr uint8_t Tracking = 0x13;
} // namespace SetCmd

} // namespace flux::lamzu
