// fluxctl — command-line access to the same protocols Flux uses.
//   fluxctl list
//   fluxctl mouse info | dpi | battery | set-dpi <stage> <dpi> | polling <hz> | lod <mm> | profile <n>
//   fluxctl kb info | travel | actuation | rt
#include "core/HidDevice.h"
#include "devices/lamzu/LamzuProtocol.h"
#include "devices/mad/LightEngine.h"
#include "devices/mad/MadProtocol.h"

#include <csignal>
#include <thread>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace flux;

static std::string findPath(uint16_t vid, const std::vector<uint16_t> &pids, uint16_t page, uint16_t usage)
{
    for (const auto &h : HidDevice::enumerate(vid, 0)) {
        bool pidOk = false;
        for (auto p : pids)
            pidOk |= h.productId == p;
        if (pidOk && h.usagePage == page && h.usage == usage)
            return h.path;
    }
    return {};
}

static int usage()
{
    std::puts("usage:\n"
              "  fluxctl list\n"
              "  fluxctl mouse info|battery|dpi\n"
              "  fluxctl mouse set-dpi <stage 1-5> <dpi>\n"
              "  fluxctl mouse polling <hz> | lod <mm> | profile <1-3>\n"
              "  fluxctl kb info|actuation|rt|travel");
    return 2;
}

static int cmdMouse(int argc, char **argv)
{
    using namespace lamzu;
    const auto path = findPath(kVendorId, {kPid8K, kPidWired, kPid1K, kPid8KAlt, kPid1KAlt}, kConfigUsagePage, kConfigUsage);
    if (path.empty()) {
        std::fprintf(stderr, "no LAMZU receiver/mouse found (or no permission)\n");
        return 1;
    }
    HidDevice dev;
    if (!dev.open(path)) {
        std::fprintf(stderr, "open %s failed: %s\n", path.c_str(), dev.lastError().c_str());
        return 1;
    }
    Protocol p(dev);
    const std::string sub = argc > 0 ? argv[0] : "info";
    auto fw = [](std::optional<std::array<int, 4>> v) {
        return v ? std::to_string((*v)[0]) + "." + std::to_string((*v)[1]) + "." + std::to_string((*v)[2]) + "." + std::to_string((*v)[3])
                 : std::string("unreachable");
    };
    const int prof = p.profile().value_or(1);
    if (sub == "info") {
        std::printf("node        %s\n", path.c_str());
        std::printf("receiver fw %s\n", fw(p.firmware(ReceiverUsb)).c_str());
        std::printf("mouse fw    %s\n", fw(p.firmware(Mouse)).c_str());
        std::printf("sensor      %d\n", p.sensorModel().value_or(-1));
        std::printf("profile     %d\n", prof);
        std::printf("polling     %d Hz\n", p.pollingRate(prof).value_or(0));
        std::printf("lod         %.1f mm\n", p.lod(prof, 2).value_or(0));
        std::printf("debounce    %d ms\n", p.debounce(prof).value_or(-1));
        std::printf("sleep       %d s\n", p.sleepSeconds(prof).value_or(-1));
        std::printf("motion sync %d  angle snap %d  ripple %d  hyper %d  xy %d\n",
                    p.flag(prof, SetCmd::MotionSync).value_or(false), p.flag(prof, SetCmd::AngleSnap).value_or(false),
                    p.flag(prof, SetCmd::Ripple).value_or(false), p.flag(prof, SetCmd::Hyper).value_or(false),
                    p.flag(prof, SetCmd::DpiXY).value_or(false));
        if (auto b = p.battery())
            std::printf("battery     %d%%%s\n", b->second, b->first ? " (charging)" : "");
        return 0;
    }
    if (sub == "battery") {
        auto b = p.battery();
        if (!b)
            return 1;
        std::printf("%d%s\n", b->second, b->first ? " charging" : "");
        return 0;
    }
    if (sub == "dpi") {
        auto st = p.stages(prof);
        if (!st)
            return 1;
        const int act = p.activeStage(prof).value_or(0);
        for (size_t i = 0; i < st->size(); ++i)
            std::printf("%c stage %zu: %d x %d\n", int(i + 1) == act ? '*' : ' ', i + 1, (*st)[i].x, (*st)[i].y);
        return 0;
    }
    if (sub == "set-dpi" && argc >= 3) {
        const int stage = std::atoi(argv[1]), dpi = std::atoi(argv[2]);
        auto st = p.stages(prof);
        if (!st || stage < 1 || stage > int(st->size()))
            return 1;
        (*st)[stage - 1] = {dpi, dpi};
        return p.setStages(prof, *st) ? 0 : 1;
    }
    if (sub == "polling" && argc >= 2)
        return p.setPollingRate(prof, std::atoi(argv[1])) ? 0 : 1;
    if (sub == "lod" && argc >= 2)
        return p.setLod(prof, std::atof(argv[1])) ? 0 : 1;
    if (sub == "profile" && argc >= 2)
        return p.setProfile(std::atoi(argv[1])) ? 0 : 1;
    return usage();
}

static int cmdKeyboard(int argc, char **argv)
{
    using namespace mad;
    const auto path = findPath(kVendorId, {kPidMadLight60}, kUsagePage, kUsage);
    if (path.empty()) {
        std::fprintf(stderr, "no MAD LIGHT 60 HE found (or no permission)\n");
        return 1;
    }
    HidDevice dev;
    if (!dev.open(path)) {
        std::fprintf(stderr, "open %s failed: %s\n", path.c_str(), dev.lastError().c_str());
        return 1;
    }
    dev.drainInput();
    Protocol p(dev);
    const std::string sub = argc > 0 ? argv[0] : "info";
    if (sub == "info") {
        std::printf("node      %s\n", path.c_str());
        auto v = p.version();
        std::printf("version   %s\n", v ? v->c_str() : ("error: " + p.lastError()).c_str());
        if (auto f = p.features())
            std::printf("features  rgbArea=%d wasd=%d mac=%d winlock=%d nkro=%d\n", f->rgbArea, f->wasdSwap, f->macMode, f->winLock, f->nkro);
        if (auto d = p.deadZones())
            std::printf("deadzones top %.2f mm, bottom %.2f mm\n", d->top / 100.0, d->bottom / 100.0);
        if (auto l = p.lighting())
            std::printf("lighting  effect=%d speed=%d bright=%d rgb=%02x%02x%02x\n", l->effect, l->speed, l->brightness, l->r, l->g, l->b);
        std::printf("macros    %d slots, %d bytes\n", p.macroCount().value_or(-1), p.macroBufferSize().value_or(-1));
        std::printf("layer     %d  bottomOpt %d  gameMode %d\n", p.defaultLayer().value_or(-1), p.bottomOptimize().value_or(false),
                    p.gameMode().value_or(false));
        return 0;
    }
    auto grid = [](auto get) {
        for (int r = 0; r < kRows; ++r) {
            for (int c = 0; c < kCols; ++c)
                std::printf("%6s", get(r * kCols + c).c_str());
            std::puts("");
        }
    };
    if (sub == "actuation") {
        auto a = p.actuationAll();
        if (!a)
            return 1;
        grid([&](int i) { char b[16]; std::snprintf(b, sizeof b, "%.2f", (*a)[i] / 100.0); return std::string(b); });
        return 0;
    }
    if (sub == "rt") {
        auto rt = p.rtAll();
        if (!rt)
            return 1;
        grid([&](int i) { char b[16]; std::snprintf(b, sizeof b, "%s%.2f", (*rt)[i].on ? "*" : " ", (*rt)[i].press / 100.0); return std::string(b); });
        return 0;
    }
    if (sub == "keymap") {
        auto km = p.keymapAll();
        if (!km)
            return 1;
        for (int l = 0; l < kLayers; ++l) {
            std::printf("layer %d\n", l);
            grid([&](int i) { char b[16]; std::snprintf(b, sizeof b, "%04x", (*km)[l * kKeys + i]); return std::string(b); });
        }
        return 0;
    }
    if (sub == "raw" && argc >= 2) {
        // fluxctl kb raw 02 96 1c 00 00 00 14 04  -> prints every report seen for 150 ms
        Report req{};
        for (int i = 1; i < argc && i - 1 < 32; ++i)
            req[i - 1] = uint8_t(std::strtoul(argv[i], nullptr, 16));
        dev.drainInput();
        p.send(req);
        uint8_t buf[64];
        for (int n; (n = dev.read({buf, sizeof buf}, 150)) > 0;)
            std::printf("<- %s\n", hexdump({buf, size_t(n)}).c_str());
        return 0;
    }
    if (sub == "fxtest") {
        // A/B test for LED flicker: Plasma with different quiet gaps after each
        // LED write. Watch the left side of the board and note which phases flicker.
        static volatile std::sig_atomic_t stop = 0;
        std::signal(SIGINT, [](int) { stop = 1; });
        auto savedLight = p.lighting();
        auto savedColors = p.keyColors();
        if (!savedLight || !savedColors) {
            std::fprintf(stderr, "couldn't read current lighting: %s\n", p.lastError().c_str());
            return 1;
        }
        const std::vector<int> empty = {2 * 14 + 12, 3 * 14 + 0, 3 * 14 + 2, 4 * 14 + 3, 4 * 14 + 4, 4 * 14 + 5, 4 * 14 + 7, 4 * 14 + 8, 4 * 14 + 13};
        std::vector<flux::fx::KeyPos> pos(kKeys);
        for (int i = 0; i < kKeys; ++i)
            pos[i] = {std::find(empty.begin(), empty.end(), i) == empty.end(), float(i % kCols) + 0.5f, float(i / kCols) + 0.5f};
        flux::fx::Engine eng;
        eng.setLayout(pos, 15.f, 5.f);
        flux::fx::Params pr;
        pr.effect = "plasma";
        pr.intensity = 0.8f;
        pr.palette = {flux::fx::Color{1.f, 0.216f, 0.373f}, flux::fx::Color{0.369f, 0.361f, 0.902f}, flux::fx::Color{0.392f, 0.824f, 1.f}};
        eng.setParams(pr);
        Lighting perKey = *savedLight;
        perKey.effect = 1;
        perKey.brightness = 120;
        p.setLighting(perKey);
        struct Phase { int gap; const char *label; };
        const Phase phases[] = {{0, "A: no gap (old behaviour)"}, {2, "B: 2 ms gap"}, {5, "C: 5 ms gap (new default)"}, {8, "D: 8 ms gap"}};
        const auto start = std::chrono::steady_clock::now();
        for (const auto &ph : phases) {
            if (stop) break;
            std::printf("\n>>> Phase %s  (8 seconds)\n", ph.label);
            std::fflush(stdout);
            const auto phaseEnd = std::chrono::steady_clock::now() + std::chrono::seconds(8);
            while (!stop && std::chrono::steady_clock::now() < phaseEnd) {
                const auto f0 = std::chrono::steady_clock::now();
                const auto frame = eng.render(std::chrono::duration<double>(f0 - start).count(), {});
                for (int row = 0; row < kRows; ++row)
                    for (int col : {0, 8}) {
                        Report req = Protocol::make({0x07, 0x42, uint8_t(row), uint8_t(col), 8});
                        for (int k = 0; k < 8 && col + k < kCols; ++k) {
                            const auto &c = frame[row * kCols + col + k];
                            req[5 + 3 * k] = uint8_t(std::min(1.f, c.r) * 255);
                            req[6 + 3 * k] = uint8_t(std::min(1.f, c.g) * 255);
                            req[7 + 3 * k] = uint8_t(std::min(1.f, c.b) * 255);
                        }
                        p.request(req, 120);
                        if (ph.gap)
                            std::this_thread::sleep_for(std::chrono::milliseconds(ph.gap));
                    }
                const auto spent = std::chrono::steady_clock::now() - f0;
                if (spent < std::chrono::milliseconds(33))
                    std::this_thread::sleep_for(std::chrono::milliseconds(33) - spent);
            }
        }
        std::printf("\nRestoring your lighting...\n");
        for (int row = 0; row < kRows; ++row)
            for (int col : {0, 8}) {
                Report req = Protocol::make({0x07, 0x42, uint8_t(row), uint8_t(col), 8});
                for (int k = 0; k < 8 && col + k < kCols; ++k) {
                    const auto &c = (*savedColors)[row * kCols + col + k];
                    req[5 + 3 * k] = c.r; req[6 + 3 * k] = c.g; req[7 + 3 * k] = c.b;
                }
                p.request(req, 200);
                std::this_thread::sleep_for(std::chrono::milliseconds(6));
            }
        p.setLighting(*savedLight);
        std::printf("Done. Which phases (A/B/C/D) flickered on the left side?\n");
        return 0;
    }
    if (sub == "timing") {
        // Round-trip latency of a plain read vs an LED write that touches no keys.
        auto bench = [&](const char *name, Report req) {
            std::vector<double> ms;
            for (int i = 0; i < 300; ++i) {
                const auto t0 = std::chrono::steady_clock::now();
                if (!p.request(req, 200))
                    continue;
                ms.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
            }
            std::sort(ms.begin(), ms.end());
            if (ms.empty()) { std::printf("%-22s no replies\n", name); return; }
            std::printf("%-22s n=%zu  min %.2f  p50 %.2f  p90 %.2f  max %.2f ms\n", name, ms.size(), ms.front(), ms[ms.size() / 2],
                        ms[ms.size() * 9 / 10], ms.back());
        };
        bench("read version", Protocol::make({0x02, 0x96, 0x00}));
        bench("led write, 0 keys", Protocol::make({0x07, 0x42, 0x00, 0x00, 0x00}));
        bench("read lighting", Protocol::make({0x08, 0x41}));
        return 0;
    }
    if (sub == "adc") {
        auto t = p.travelAll();
        if (!t)
            return 1;
        grid([&](int i) { char b[16]; std::snprintf(b, sizeof b, "%u/%u", (*t)[i].adc, (*t)[i].status); return std::string(b); });
        return 0;
    }
    if (sub == "travel") {
        auto t = p.travelAll();
        if (!t)
            return 1;
        grid([&](int i) { char b[16]; std::snprintf(b, sizeof b, "%.2f", (*t)[i].trip / 100.0); return std::string(b); });
        return 0;
    }
    return usage();
}

int main(int argc, char **argv)
{
    HidDevice::initLibrary();
    if (argc < 2)
        return usage();
    const std::string cmd = argv[1];
    if (cmd == "list") {
        for (const auto &h : HidDevice::enumerate()) {
            if (h.vendorId != lamzu::kVendorId && h.vendorId != mad::kVendorId)
                continue;
            std::printf("%s  %04x:%04x  page %04x usage %04x  if%d  %s\n", h.path.c_str(), h.vendorId, h.productId, h.usagePage, h.usage,
                        h.interfaceNumber, h.product.c_str());
        }
        return 0;
    }
    if (cmd == "mouse")
        return cmdMouse(argc - 2, argv + 2);
    if (cmd == "kb" || cmd == "keyboard")
        return cmdKeyboard(argc - 2, argv + 2);
    return usage();
}
