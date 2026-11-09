#include "HidDevice.h"

#include <hidapi.h>

#include <cstdio>
#include <mutex>

namespace flux {

namespace {
std::string narrow(const wchar_t *w)
{
    if (!w)
        return {};
    std::string out;
    for (; *w; ++w)
        out.push_back(*w < 0x80 ? static_cast<char>(*w) : '?');
    return out;
}
std::mutex g_enumMutex;
} // namespace

void HidDevice::initLibrary()
{
    static std::once_flag once;
    std::call_once(once, [] { hid_init(); });
}

std::vector<HidInfo> HidDevice::enumerate(uint16_t vid, uint16_t pid)
{
    std::lock_guard lock(g_enumMutex);
    std::vector<HidInfo> out;
    hid_device_info *list = hid_enumerate(vid, pid);
    for (auto *d = list; d; d = d->next) {
        HidInfo info;
        info.path = d->path ? d->path : "";
        info.vendorId = d->vendor_id;
        info.productId = d->product_id;
        info.usagePage = d->usage_page;
        info.usage = d->usage;
        info.interfaceNumber = d->interface_number;
        info.product = narrow(d->product_string);
        info.manufacturer = narrow(d->manufacturer_string);
        info.serial = narrow(d->serial_number);
        out.push_back(std::move(info));
    }
    hid_free_enumeration(list);
    return out;
}

HidDevice::~HidDevice()
{
    close();
}

bool HidDevice::open(const std::string &path)
{
    close();
    m_dev = hid_open_path(path.c_str());
    if (m_dev)
        m_path = path;
    return m_dev != nullptr;
}

void HidDevice::close()
{
    if (m_dev) {
        hid_close(m_dev);
        m_dev = nullptr;
    }
    m_path.clear();
}

int HidDevice::write(std::span<const uint8_t> data)
{
    if (!m_dev)
        return -1;
    return hid_write(m_dev, data.data(), data.size());
}

int HidDevice::read(std::span<uint8_t> buf, int timeoutMs)
{
    if (!m_dev)
        return -1;
    return hid_read_timeout(m_dev, buf.data(), buf.size(), timeoutMs);
}

int HidDevice::sendFeature(std::span<const uint8_t> data)
{
    if (!m_dev)
        return -1;
    return hid_send_feature_report(m_dev, data.data(), data.size());
}

int HidDevice::getFeature(std::span<uint8_t> buf)
{
    if (!m_dev)
        return -1;
    return hid_get_feature_report(m_dev, buf.data(), buf.size());
}

void HidDevice::drainInput()
{
    if (!m_dev)
        return;
    uint8_t scratch[128];
    while (hid_read_timeout(m_dev, scratch, sizeof scratch, 0) > 0) {
    }
}

std::string HidDevice::lastError() const
{
    return narrow(hid_error(m_dev));
}

std::string hexdump(std::span<const uint8_t> data, size_t max)
{
    std::string s;
    char b[4];
    for (size_t i = 0; i < data.size() && i < max; ++i) {
        std::snprintf(b, sizeof b, "%02x", data[i]);
        if (i)
            s.push_back(' ');
        s += b;
    }
    if (data.size() > max)
        s += " …";
    return s;
}

} // namespace flux
