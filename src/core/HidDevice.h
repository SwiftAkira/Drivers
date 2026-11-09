#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

struct hid_device_;

namespace flux {

struct HidInfo {
    std::string path;
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    uint16_t usagePage = 0;
    uint16_t usage = 0;
    int interfaceNumber = -1;
    std::string product;
    std::string manufacturer;
    std::string serial;
};

// Thin RAII wrapper around a hidapi handle. Not thread-safe: each instance is
// meant to be owned and used by exactly one IoWorker thread.
class HidDevice {
public:
    HidDevice() = default;
    ~HidDevice();
    HidDevice(const HidDevice &) = delete;
    HidDevice &operator=(const HidDevice &) = delete;

    static void initLibrary();
    static std::vector<HidInfo> enumerate(uint16_t vid = 0, uint16_t pid = 0);

    bool open(const std::string &path);
    void close();
    bool isOpen() const { return m_dev != nullptr; }
    const std::string &path() const { return m_path; }

    // Output report. data[0] must be the report ID (0 when the device has none).
    int write(std::span<const uint8_t> data);
    // Input report. Returns bytes read, 0 on timeout, -1 on error.
    int read(std::span<uint8_t> buf, int timeoutMs);
    // Feature reports. buf[0] is the report ID in both directions.
    int sendFeature(std::span<const uint8_t> data);
    int getFeature(std::span<uint8_t> buf);

    // Drop any input reports that are already queued.
    void drainInput();

    std::string lastError() const;

private:
    hid_device_ *m_dev = nullptr;
    std::string m_path;
};

std::string hexdump(std::span<const uint8_t> data, size_t max = 64);

} // namespace flux
