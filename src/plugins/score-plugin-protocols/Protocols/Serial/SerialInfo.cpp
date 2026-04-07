// serial_port_info.cpp — Qt-free serial port enumeration (C++23)
// Ported from Qt 6 QSerialPortInfo. Original copyrights:
// Copyright (C) 2011-2012 Denis Shienkov <denis.shienkov@gmail.com>
// Copyright (C) 2011 Sergey Belyashov <Sergey.Belyashov@gmail.com>
// Copyright (C) 2012 Laszlo Papp <lpapp@kde.org>
// Copyright (C) 2016 Denis Shienkov <denis.shienkov@gmail.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "SerialInfo.hpp"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#if defined(__linux__)
#  include <cerrno>
#  include <dlfcn.h>
#  include <fcntl.h>
#  include <linux/serial.h>
#  include <sys/ioctl.h>
#  include <unistd.h>
#endif

#if defined(__APPLE__)
#  include <CoreFoundation/CoreFoundation.h>
#  include <IOKit/IOBSD.h>
#  include <IOKit/IOKitLib.h>
#  include <IOKit/serial/IOSerialKeys.h>
#  include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#  include <IOKit/usb/USB.h>
#endif

#if defined(__FreeBSD__)
#  include <sys/sysctl.h>
#endif

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <cfgmgr32.h>
#  include <devguid.h>
#  include <initguid.h>
#  include <setupapi.h>
#  include <winioctl.h>
#  pragma comment(lib, "setupapi.lib")
#  pragma comment(lib, "cfgmgr32.lib")

// GUID_DEVINTERFACE_COMPORT {86E0D1E0-8089-11D0-9CE4-08003E301F73}
DEFINE_GUID(GUID_DEVINTERFACE_COMPORT_LOCAL,
    0x86e0d1e0, 0x8089, 0x11d0, 0x9c, 0xe4, 0x08, 0x00, 0x3e, 0x30, 0x1f, 0x73);
// GUID_DEVINTERFACE_MODEM {2C7089AA-2E0E-11D1-B114-00C04FC2AAE4}
DEFINE_GUID(GUID_DEVINTERFACE_MODEM_LOCAL,
    0x2c7089aa, 0x2e0e, 0x11d1, 0xb1, 0x14, 0x00, 0xc0, 0x4f, 0xc2, 0xaa, 0xe4);
#endif

#if !defined(_WIN32)
#include <termios.h>
#endif

namespace fs = std::filesystem;

namespace serial {
namespace {

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

static uint16_t parse_hex_id(const std::string& s, bool& ok)
{
    if (s.empty()) { ok = false; return 0; }
    uint16_t val = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val, 16);
    ok = (ec == std::errc{}) && (ptr == s.data() + s.size());
    return val;
}

// ---------------------------------------------------------------------------
// POSIX path helpers
// ---------------------------------------------------------------------------

#if defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__)

static std::string port_name_to_system_location(const std::string& name)
{
    if (name.starts_with('/') || name.starts_with("./") || name.starts_with("../"))
        return name;
    return "/dev/" + name;
}

static std::string port_name_from_system_location(const std::string& path)
{
    if (path.starts_with("/dev/"))
        return path.substr(5);
    return path;
}

#endif // POSIX

// ============================================================================
//  Linux
// ============================================================================
#if defined(__linux__)

// --- udev dynamic loading ---------------------------------------------------

struct udev;
struct udev_enumerate;
struct udev_device;
struct udev_list_entry;

#define UDEV_FUNC(ret, name, ...) \
    using name##_t = ret(*)(__VA_ARGS__); \
    static name##_t p_##name = nullptr;

UDEV_FUNC(udev*, udev_new)
UDEV_FUNC(udev_enumerate*, udev_enumerate_new, udev*)
UDEV_FUNC(int, udev_enumerate_add_match_subsystem, udev_enumerate*, const char*)
UDEV_FUNC(int, udev_enumerate_scan_devices, udev_enumerate*)
UDEV_FUNC(udev_list_entry*, udev_enumerate_get_list_entry, udev_enumerate*)
UDEV_FUNC(udev_list_entry*, udev_list_entry_get_next, udev_list_entry*)
UDEV_FUNC(udev_device*, udev_device_new_from_syspath, udev*, const char*)
UDEV_FUNC(const char*, udev_list_entry_get_name, udev_list_entry*)
UDEV_FUNC(const char*, udev_device_get_devnode, udev_device*)
UDEV_FUNC(const char*, udev_device_get_sysname, udev_device*)
UDEV_FUNC(const char*, udev_device_get_driver, udev_device*)
UDEV_FUNC(udev_device*, udev_device_get_parent, udev_device*)
UDEV_FUNC(const char*, udev_device_get_property_value, udev_device*, const char*)
UDEV_FUNC(void, udev_device_unref, udev_device*)
UDEV_FUNC(void, udev_enumerate_unref, udev_enumerate*)
UDEV_FUNC(void, udev_unref, udev*)

#undef UDEV_FUNC

static bool load_udev_impl()
{
    void* lib = dlopen("libudev.so.1", RTLD_LAZY);
    if (!lib) lib = dlopen("libudev.so.0", RTLD_LAZY);
    if (!lib) return false;

    #define LOAD(name) \
        p_##name = reinterpret_cast<name##_t>(dlsym(lib, #name)); \
        if (!p_##name) return false;
    LOAD(udev_new)
    LOAD(udev_enumerate_new)
    LOAD(udev_enumerate_add_match_subsystem)
    LOAD(udev_enumerate_scan_devices)
    LOAD(udev_enumerate_get_list_entry)
    LOAD(udev_list_entry_get_next)
    LOAD(udev_device_new_from_syspath)
    LOAD(udev_list_entry_get_name)
    LOAD(udev_device_get_devnode)
    LOAD(udev_device_get_sysname)
    LOAD(udev_device_get_driver)
    LOAD(udev_device_get_parent)
    LOAD(udev_device_get_property_value)
    LOAD(udev_device_unref)
    LOAD(udev_enumerate_unref)
    LOAD(udev_unref)
    #undef LOAD

    return true;
}

static bool load_udev()
{
    static std::once_flag flag;
    static bool loaded = false;
    std::call_once(flag, [] { loaded = load_udev_impl(); });
    return loaded;
}

// RAII wrappers
struct udev_deleter {
    void operator()(udev* p) const { if (p) p_udev_unref(p); }
    void operator()(udev_enumerate* p) const { if (p) p_udev_enumerate_unref(p); }
    void operator()(udev_device* p) const { if (p) p_udev_device_unref(p); }
};

// --- Linux helpers ----------------------------------------------------------

static std::string safe_str(const char* s) { return s ? s : ""; }

static bool is_rfcomm(std::string_view name)
{
    if (!name.starts_with("rfcomm")) return false;
    auto rest = name.substr(6);
    if (rest.empty()) return false;
    int val = 0;
    auto [ptr, ec] = std::from_chars(rest.data(), rest.data() + rest.size(), val);
    return ec == std::errc{} && ptr == rest.data() + rest.size() && val >= 0 && val <= 255;
}

static bool is_virtual_null_modem(std::string_view name) { return name.starts_with("tnt"); }
static bool is_gadget(std::string_view name) { return name.starts_with("ttyGS"); }

// --- sysfs helpers ----------------------------------------------------------

static std::string read_file_trimmed(const fs::path& path)
{
    std::ifstream f(path);
    if (!f) return {};
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    while (!content.empty() && (content.back() == '\n' || content.back() == '\r' || content.back() == ' '))
        content.pop_back();
    return content;
}

// --- serial8250 phantom port detection --------------------------------------

// Detect whether a tty is backed by the serial8250 platform driver.
// Qt only checked the direct parent's driver name (which is "port", not
// "serial8250"), so phantom ports were never filtered.  We instead check
// whether the sysfs symlink target contains "serial8250" anywhere in its
// path, which is reliable across kernel versions.
static bool is_serial8250_device(std::string_view port_name)
{
    std::error_code ec;
    fs::path link = fs::read_symlink(fs::path("/sys/class/tty") / port_name, ec);
    if (ec) return false;
    return link.string().find("serial8250") != std::string::npos;
}

// Check whether a serial8250 port is real hardware or a phantom.
// Strategy 1 (preferred): read /sys/class/tty/<name>/type — value 0
//   means PORT_UNKNOWN (phantom).  Needs no device permissions.
// Strategy 2 (fallback):  open the device and ioctl(TIOCGSERIAL).
//   Requires read-write access, so it fails for non-root users.
static bool is_real_serial8250(std::string_view port_name, const std::string& dev_path)
{
    // Try sysfs first — no permissions needed
    std::string type_str = read_file_trimmed(
        fs::path("/sys/class/tty") / port_name / "type");
    if (!type_str.empty()) {
        int port_type = 0;
        auto [ptr, ec] = std::from_chars(
            type_str.data(), type_str.data() + type_str.size(), port_type);
        if (ec == std::errc{})
            return port_type != 0; // 0 = PORT_UNKNOWN = phantom
    }

    // Fallback: open device and check via ioctl
    // Retry on EINTR (matching Qt's qt_safe_open behavior)
    auto safe_open = [](const char* path, int flags) {
        int fd;
        do { fd = ::open(path, flags); } while (fd == -1 && errno == EINTR);
        return fd;
    };
    int fd = safe_open(dev_path.c_str(), O_RDWR | O_NONBLOCK | O_NOCTTY);
    if (fd < 0) {
        // Try read-only — some kernels allow TIOCGSERIAL on O_RDONLY
        fd = safe_open(dev_path.c_str(), O_RDONLY | O_NONBLOCK | O_NOCTTY);
    }
    if (fd < 0) return false;
    struct serial_struct serinfo{};
    int rv = ::ioctl(fd, TIOCGSERIAL, &serinfo);
    ::close(fd);
    return rv != -1 && serinfo.type != PORT_UNKNOWN;
}

static std::string uevent_property(const fs::path& dir, std::string_view pattern)
{
    std::ifstream f(dir / "uevent");
    if (!f) return {};
    std::string line;
    while (std::getline(f, line)) {
        if (line.starts_with(pattern))
            return line.substr(pattern.size());
    }
    return {};
}

static std::string sysfs_device_name(const fs::path& dir)
{
    return uevent_property(dir, "DEVNAME=");
}

static std::string sysfs_device_driver(const fs::path& dir)
{
    return uevent_property(dir / "device", "DRIVER=");
}

// --- Linux enumeration strategies -------------------------------------------

// Strategy 1: libudev
static bool available_ports_udev(std::vector<port_info>& out)
{
    if (!load_udev()) return false;

    if (const char* env = std::getenv("QT_SERIALPORT_SKIP_UDEV_LOOKUP"); env && *env)
        return false;

    std::unique_ptr<udev, udev_deleter> ctx(p_udev_new());
    if (!ctx) return false;

    std::unique_ptr<udev_enumerate, udev_deleter> en(p_udev_enumerate_new(ctx.get()));
    if (!en) return false;

    p_udev_enumerate_add_match_subsystem(en.get(), "tty");
    p_udev_enumerate_scan_devices(en.get());

    udev_list_entry* devices = p_udev_enumerate_get_list_entry(en.get());
    if (!devices) return false;

    bool found_any = false;
    for (udev_list_entry* entry = devices; entry; entry = p_udev_list_entry_get_next(entry)) {
        found_any = true;

        std::unique_ptr<udev_device, udev_deleter> dev(
            p_udev_device_new_from_syspath(ctx.get(), p_udev_list_entry_get_name(entry)));
        if (!dev) continue;

        port_info info;
        info.system_location = safe_str(p_udev_device_get_devnode(dev.get()));
        info.port_name = safe_str(p_udev_device_get_sysname(dev.get()));

        // Filter phantom serial8250 ports before doing any expensive work
        if (is_serial8250_device(info.port_name) &&
            !is_real_serial8250(info.port_name, info.system_location))
            continue;

        udev_device* parent = p_udev_device_get_parent(dev.get());
        if (parent) {

            auto prop = [&](const char* name) {
                return safe_str(p_udev_device_get_property_value(dev.get(), name));
            };

            std::string desc = prop("ID_MODEL");
            std::replace(desc.begin(), desc.end(), '_', ' ');
            info.description = std::move(desc);

            std::string mfr = prop("ID_VENDOR");
            std::replace(mfr.begin(), mfr.end(), '_', ' ');
            info.manufacturer = std::move(mfr);

            info.serial_number = prop("ID_SERIAL_SHORT");

            bool ok = false;
            uint16_t vid = parse_hex_id(prop("ID_VENDOR_ID"), ok);
            if (ok) info.vendor_id = vid;

            ok = false;
            uint16_t pid = parse_hex_id(prop("ID_MODEL_ID"), ok);
            if (ok) info.product_id = pid;
        } else {
            if (!is_rfcomm(info.port_name) &&
                !is_virtual_null_modem(info.port_name) &&
                !is_gadget(info.port_name))
                continue;
        }

        out.push_back(std::move(info));
    }

    return found_any;
}

// Strategy 2: sysfs
static bool available_ports_sysfs(std::vector<port_info>& out)
{
    const fs::path sysfs("/sys/class/tty");
    std::error_code ec;
    if (!fs::exists(sysfs, ec)) return false;

    bool found = false;
    for (const auto& entry : fs::directory_iterator(sysfs, ec)) {
        if (!fs::is_symlink(entry.path(), ec)) continue;

        fs::path target = fs::read_symlink(entry.path(), ec);
        if (ec) continue;
        fs::path target_abs = fs::canonical(sysfs / target, ec);
        if (ec) continue;

        std::string name = sysfs_device_name(target_abs);
        if (name.empty()) continue;

        std::string driver = sysfs_device_driver(target_abs);
        if (driver.empty()) {
            if (!is_rfcomm(name) && !is_virtual_null_modem(name) && !is_gadget(name))
                continue;
        }

        port_info info;
        info.port_name = name;
        info.system_location = port_name_to_system_location(name);

        if (is_serial8250_device(name) && !is_real_serial8250(name, info.system_location))
            continue;

        // Walk up directory tree looking for USB metadata
        fs::path dir = target_abs;
        do {
            if (info.description.empty())
                info.description = read_file_trimmed(dir / "product");
            if (info.manufacturer.empty())
                info.manufacturer = read_file_trimmed(dir / "manufacturer");
            if (info.serial_number.empty())
                info.serial_number = read_file_trimmed(dir / "serial");

            if (!info.vendor_id) {
                std::string s = read_file_trimmed(dir / "idVendor");
                if (s.empty()) s = read_file_trimmed(dir / "vendor");
                bool ok = false;
                uint16_t v = parse_hex_id(s, ok);
                if (ok) info.vendor_id = v;
            }
            if (!info.product_id) {
                std::string s = read_file_trimmed(dir / "idProduct");
                if (s.empty()) s = read_file_trimmed(dir / "device");
                bool ok = false;
                uint16_t v = parse_hex_id(s, ok);
                if (ok) info.product_id = v;
            }

            if (!info.description.empty() || !info.manufacturer.empty() ||
                !info.serial_number.empty() || info.vendor_id || info.product_id)
                break;

            dir = dir.parent_path();
        } while (dir != "/" && dir != dir.parent_path());

        out.push_back(std::move(info));
        found = true;
    }

    return found;
}

// Strategy 3: filter known device file patterns in /dev
static bool available_ports_dev_filter(std::vector<port_info>& out)
{
    static constexpr std::string_view patterns[] = {
        "ttyS", "ttyO", "ttyUSB", "ttyACM", "ttyGS", "ttyMI",
        "ttymxc", "ttyAMA", "ttyTHS", "ttyHS",
        "ttyAS",     // Amlogic Meson UART
        "ttySAC",    // Samsung S3C UART
        "ttyPS",     // Xilinx UART
        "ttyRPMSG",  // Remote processor messaging
        "ttyMSM",    // Qualcomm MSM UART
        "ttySTM",    // STMicroelectronics UART
        "ttySIF",    // SiFive UART
        "ttyLP",     // Parallel port emulation
        "ttyXRUSB",  // Exar USB serial
        "rfcomm", "ircomm", "tnt"
    };

    const fs::path dev("/dev");
    std::error_code ec;
    if (!fs::exists(dev, ec)) return false;

    for (const auto& entry : fs::directory_iterator(dev, ec)) {
        if (fs::is_symlink(entry.path(), ec)) continue;
        auto name = entry.path().filename().string();
        bool matched = false;
        for (auto pat : patterns) {
            if (name.starts_with(pat)) { matched = true; break; }
        }
        if (!matched) continue;

        port_info info;
        info.system_location = entry.path().string();
        info.port_name = name;
        out.push_back(std::move(info));
    }
    return true;
}

#endif // __linux__

// ============================================================================
//  macOS
// ============================================================================
#if defined(__APPLE__)

static std::string cf_to_string(CFTypeRef ref)
{
    if (!ref || CFGetTypeID(ref) != CFStringGetTypeID()) return {};
    CFStringRef str = static_cast<CFStringRef>(ref);
    // Try stack buffer first, then heap-allocate for long strings
    char buf[256];
    if (CFStringGetCString(str, buf, sizeof(buf), kCFStringEncodingUTF8))
        return buf;
    CFIndex len = CFStringGetLength(str);
    CFIndex max_size = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1;
    std::string result(max_size, '\0');
    if (CFStringGetCString(str, result.data(), max_size, kCFStringEncodingUTF8)) {
        result.resize(std::strlen(result.c_str()));
        return result;
    }
    return {};
}

static std::string search_string(io_registry_entry_t entry, CFStringRef key)
{
    CFTypeRef val = IORegistryEntrySearchCFProperty(
        entry, kIOServicePlane, key, kCFAllocatorDefault, 0);
    if (!val) return {};
    std::string result = cf_to_string(val);
    CFRelease(val);
    return result;
}

static std::optional<uint16_t> search_uint16(io_registry_entry_t entry, CFStringRef key)
{
    CFTypeRef val = IORegistryEntrySearchCFProperty(
        entry, kIOServicePlane, key, kCFAllocatorDefault, 0);
    if (!val) return std::nullopt;
    if (CFGetTypeID(val) != CFNumberGetTypeID()) { CFRelease(val); return std::nullopt; }
    int16_t num = 0;
    bool ok = CFNumberGetValue(static_cast<CFNumberRef>(val), kCFNumberSInt16Type, &num);
    CFRelease(val);
    if (!ok) return std::nullopt;
    return static_cast<uint16_t>(num);
}

static io_registry_entry_t parent_entry(io_registry_entry_t current)
{
    io_registry_entry_t parent = 0;
    IORegistryEntryGetParentEntry(current, kIOServicePlane, &parent);
    IOObjectRelease(current);
    return parent;
}

#endif // __APPLE__

// ============================================================================
//  FreeBSD
// ============================================================================
#if defined(__FreeBSD__)

static std::string fbsd_property(const std::string& source, std::string_view key)
{
    auto pos = source.find(key);
    if (pos == std::string::npos) return {};
    pos += key.size();
    auto end = source.find(' ', pos);
    if (end == std::string::npos) end = source.size();
    return source.substr(pos, end - pos);
}

static std::vector<int> mib_from_name(const std::string& name)
{
    size_t mibsize = 0;
    if (sysctlnametomib(name.c_str(), nullptr, &mibsize) < 0 || mibsize == 0)
        return {};
    std::vector<int> mib(mibsize);
    if (sysctlnametomib(name.c_str(), mib.data(), &mibsize) < 0)
        return {};
    return mib;
}

static std::vector<int> next_oid(const std::vector<int>& prev)
{
    std::vector<int> mib = {0, 2};
    mib.insert(mib.end(), prev.begin(), prev.end());

    size_t len = 0;
    if (sysctl(mib.data(), static_cast<u_int>(mib.size()), nullptr, &len, nullptr, 0) < 0)
        return {};
    size_t oid_len = len / sizeof(int);
    std::vector<int> oid(oid_len, 0);
    if (sysctl(mib.data(), static_cast<u_int>(mib.size()), oid.data(), &len, nullptr, 0) < 0)
        return {};
    if (!prev.empty() && !oid.empty() && prev[0] != oid[0])
        return {};
    return oid;
}

struct sysctl_node {
    std::string name;
    std::string value;
};

static sysctl_node node_for_oid(const std::vector<int>& oid)
{
    // query node name
    std::vector<int> mib = {0, 1};
    mib.insert(mib.end(), oid.begin(), oid.end());

    size_t len = 0;
    if (sysctl(mib.data(), static_cast<u_int>(mib.size()), nullptr, &len, nullptr, 0) < 0)
        return {};
    std::string name(len, '\0');
    if (sysctl(mib.data(), static_cast<u_int>(mib.size()), name.data(), &len, nullptr, 0) < 0)
        return {};
    name.resize(std::strlen(name.c_str()));

    // query value
    len = 0;
    if (sysctl(oid.data(), static_cast<u_int>(oid.size()), nullptr, &len, nullptr, 0) < 0)
        return {};
    std::string value(len, '\0');
    if (sysctl(oid.data(), static_cast<u_int>(oid.size()), value.data(), &len, nullptr, 0) < 0)
        return {};
    value.resize(std::strlen(value.c_str()));

    // query format — only want string type ('A')
    mib[1] = 4;
    len = 0;
    if (sysctl(mib.data(), static_cast<u_int>(mib.size()), nullptr, &len, nullptr, 0) < 0)
        return {};
    std::vector<char> fmt_buf(len, 0);
    if (sysctl(mib.data(), static_cast<u_int>(mib.size()), fmt_buf.data(), &len, nullptr, 0) < 0)
        return {};
    if (fmt_buf.size() < 5 || fmt_buf[4] != 'A')
        return {};

    return {std::move(name), std::move(value)};
}

#endif // __FreeBSD__

// ============================================================================
//  Windows helpers
// ============================================================================
#if defined(_WIN32)

static std::string port_name_to_system_location(const std::string& name)
{
    if (name.starts_with("COM"))
        return "\\\\.\\" + name;
    return name;
}

[[maybe_unused]]
static std::string port_name_from_system_location(const std::string& path)
{
    if (path.starts_with("\\\\.\\") || path.starts_with("//./"))
        return path.substr(4);
    return path;
}

static std::string from_wide(const wchar_t* ws)
{
    if (!ws || !*ws) return {};
    int sz = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
    std::string s(sz - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws, -1, s.data(), sz, nullptr, nullptr);
    return s;
}

static std::string device_registry_property(HDEVINFO info_set, PSP_DEVINFO_DATA data, DWORD prop)
{
    DWORD data_type = 0;
    std::vector<wchar_t> buf(MAX_PATH + 1, 0);
    DWORD required = MAX_PATH;
    for (;;) {
        if (SetupDiGetDeviceRegistryPropertyW(info_set, data, prop, &data_type,
                reinterpret_cast<PBYTE>(buf.data()), required, &required))
            break;
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
            (data_type != REG_SZ && data_type != REG_EXPAND_SZ))
            return {};
        buf.resize(required / sizeof(wchar_t) + 2, 0);
    }
    return from_wide(buf.data());
}

static std::string device_port_name(HDEVINFO info_set, PSP_DEVINFO_DATA data)
{
    HKEY key = SetupDiOpenDevRegKey(info_set, data, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
    if (key == INVALID_HANDLE_VALUE) return {};

    static const wchar_t* tokens[] = { L"PortName", L"PortNumber" };
    std::string result;

    for (auto token : tokens) {
        DWORD data_type = 0;
        std::vector<wchar_t> buf(MAX_PATH + 1, 0);
        DWORD required = MAX_PATH;
        for (;;) {
            LONG ret = RegQueryValueExW(key, token, nullptr, &data_type,
                                        reinterpret_cast<PBYTE>(buf.data()), &required);
            if (ret == ERROR_MORE_DATA) {
                buf.resize(required / sizeof(wchar_t) + 2, 0);
                continue;
            }
            if (ret == ERROR_SUCCESS) {
                if (data_type == REG_SZ)
                    result = from_wide(buf.data());
                else if (data_type == REG_DWORD)
                    result = "COM" + std::to_string(*reinterpret_cast<DWORD*>(buf.data()));
            }
            break;
        }
        if (!result.empty()) break;
    }
    RegCloseKey(key);
    return result;
}

static std::string device_instance_identifier(DEVINST inst)
{
    std::vector<wchar_t> buf(MAX_DEVICE_ID_LEN + 1, 0);
    if (CM_Get_Device_IDW(inst, buf.data(), MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS)
        return {};
    std::string s = from_wide(buf.data());
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

static DEVINST parent_instance(DEVINST child)
{
    ULONG status = 0, problem = 0;
    if (CM_Get_DevNode_Status(&status, &problem, child, 0) != CR_SUCCESS)
        return 0;
    DEVINST parent = 0;
    if (CM_Get_Parent(&parent, child, 0) != CR_SUCCESS)
        return 0;
    return parent;
}

static uint16_t parse_id(const std::string& inst, std::string_view prefix, bool& ok)
{
    auto pos = inst.find(prefix);
    if (pos == std::string::npos) { ok = false; return 0; }
    pos += prefix.size();
    uint16_t val = 0;
    auto end = inst.data() + std::min(pos + 4, inst.size());
    auto [ptr, ec] = std::from_chars(inst.data() + pos, end, val, 16);
    ok = (ec == std::errc{});
    return val;
}

static uint16_t device_vid(const std::string& inst, bool& ok)
{
    uint16_t v = parse_id(inst, "VID_", ok);
    if (!ok) v = parse_id(inst, "VEN_", ok);
    return v;
}

static uint16_t device_pid(const std::string& inst, bool& ok)
{
    uint16_t v = parse_id(inst, "PID_", ok);
    if (!ok) v = parse_id(inst, "DEV_", ok);
    return v;
}

static std::string parse_serial_number(const std::string& inst)
{
    auto last_backslash = inst.rfind('\\');
    if (last_backslash == std::string::npos) return {};

    if (inst.starts_with("USB\\")) {
        auto underscore = inst.find('_', last_backslash);
        size_t end_pos = inst.size();
        if (underscore != std::string::npos && underscore != inst.size() - 3)
            end_pos = inst.size();
        else if (underscore != std::string::npos)
            end_pos = underscore;
        auto ampersand = inst.find('&', last_backslash);
        if (ampersand != std::string::npos && ampersand < end_pos)
            return {};
        return inst.substr(last_backslash + 1, end_pos - last_backslash - 1);
    } else if (inst.starts_with("FTDIBUS\\")) {
        auto plus = inst.rfind('+');
        if (plus == std::string::npos) return {};
        auto backslash = inst.find('\\', plus);
        if (backslash == std::string::npos) return {};
        return inst.substr(plus + 1, backslash - plus - 1);
    }
    return {};
}

static std::string device_serial(std::string inst, DEVINST dev_inst)
{
    for (;;) {
        std::string sn = parse_serial_number(inst);
        if (!sn.empty()) return sn;
        dev_inst = parent_instance(dev_inst);
        if (!dev_inst) break;
        inst = device_instance_identifier(dev_inst);
        if (inst.empty()) break;
    }
    return {};
}

static std::vector<std::string> ports_from_registry()
{
    HKEY key;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM",
                      0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return {};

    std::vector<std::string> result;
    DWORD index = 0;
    std::vector<wchar_t> name_buf(16384, 0);
    std::vector<wchar_t> value_buf(MAX_PATH + 1, 0);

    for (;;) {
        DWORD name_len = static_cast<DWORD>(name_buf.size());
        DWORD value_bytes = MAX_PATH * sizeof(wchar_t);
        LONG ret = RegEnumValueW(key, index, name_buf.data(), &name_len,
                                 nullptr, nullptr, reinterpret_cast<PBYTE>(value_buf.data()),
                                 &value_bytes);
        if (ret == ERROR_MORE_DATA) {
            value_buf.resize(value_bytes / sizeof(wchar_t) + 2, 0);
        } else if (ret == ERROR_SUCCESS) {
            result.push_back(from_wide(value_buf.data()));
            ++index;
        } else {
            break;
        }
    }
    RegCloseKey(key);
    return result;
}

#endif // _WIN32

} // anonymous namespace

// ============================================================================
//  Public API
// ============================================================================

std::vector<port_info> available_ports()
{
    std::vector<port_info> result;

    // ---- Linux ----
#if defined(__linux__)
    if (!available_ports_udev(result)) {
        if (!available_ports_sysfs(result)) {
            available_ports_dev_filter(result);
        }
    }

    // ---- macOS ----
#elif defined(__APPLE__)
    CFMutableDictionaryRef match = IOServiceMatching(kIOSerialBSDServiceValue);
    if (!match) return result;

    CFDictionaryAddValue(match, CFSTR(kIOSerialBSDTypeKey), CFSTR(kIOSerialBSDAllTypes));

    io_iterator_t iter = 0;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, match, &iter) != KERN_SUCCESS)
        return result;

    for (;;) {
        io_registry_entry_t service = IOIteratorNext(iter);
        if (!service) break;

        port_info info;
        std::string callout_device, dialin_device;

        for (;;) {
            if (callout_device.empty())
                callout_device = search_string(service, CFSTR(kIOCalloutDeviceKey));
            if (dialin_device.empty())
                dialin_device = search_string(service, CFSTR(kIODialinDeviceKey));
            if (info.description.empty()) {
                info.description = search_string(service, CFSTR(kIOPropertyProductNameKey));
                if (info.description.empty())
                    info.description = search_string(service, CFSTR(kUSBProductString));
                if (info.description.empty())
                    info.description = search_string(service, CFSTR("BTName"));
            }
            if (info.manufacturer.empty())
                info.manufacturer = search_string(service, CFSTR(kUSBVendorString));
            if (info.serial_number.empty())
                info.serial_number = search_string(service, CFSTR(kUSBSerialNumberString));
            if (!info.vendor_id)
                info.vendor_id = search_uint16(service, CFSTR(kUSBVendorID));
            if (!info.product_id)
                info.product_id = search_uint16(service, CFSTR(kUSBProductID));

            bool complete = !callout_device.empty() && !dialin_device.empty() &&
                            !info.manufacturer.empty() && !info.description.empty() &&
                            !info.serial_number.empty() && info.vendor_id && info.product_id;
            if (complete) {
                IOObjectRelease(service);
                break;
            }
            service = parent_entry(service);
            if (!service) break;
        }

        // Emit both callout and dialin entries (matching Qt behavior)
        port_info callout_info = info;
        callout_info.system_location = callout_device;
        callout_info.port_name = port_name_from_system_location(callout_device);
        result.push_back(std::move(callout_info));

        port_info dialin_info = info;
        dialin_info.system_location = dialin_device;
        dialin_info.port_name = port_name_from_system_location(dialin_device);
        result.push_back(std::move(dialin_info));
    }

    IOObjectRelease(iter);

    // ---- FreeBSD ----
#elif defined(__FreeBSD__)
    auto mib = mib_from_name("dev");
    if (mib.empty()) return result;

    std::vector<sysctl_node> nodes;
    {
        auto oid = mib;
        for (;;) {
            auto next = next_oid(oid);
            if (next.empty()) break;
            auto node = node_for_oid(next);
            if (!node.name.empty() &&
                (node.name.ends_with("%desc") || node.name.ends_with("%pnpinfo")))
                nodes.push_back(std::move(node));
            oid = next;
        }
    }
    if (nodes.empty()) return result;

    const fs::path dev("/dev");
    std::error_code ec;
    if (!fs::exists(dev, ec)) return result;

    std::vector<port_info> cua_list, tty_list;

    for (const auto& entry : fs::directory_iterator(dev, ec)) {
        auto name = entry.path().filename().string();
        if (!name.starts_with("cua") && !name.starts_with("tty"))
            continue;
        if (name.ends_with(".init") || name.ends_with(".lock"))
            continue;

        port_info info;
        info.port_name = name;
        info.system_location = entry.path().string();

        for (const auto& node : nodes) {
            auto pnp_pos = node.name.find("%pnpinfo");
            if (pnp_pos == std::string::npos || node.value.empty())
                continue;

            std::string ttyname = fbsd_property(node.value, "ttyname=");
            if (ttyname.empty()) continue;

            std::string count_str = fbsd_property(node.value, "ttyports=");
            if (count_str.empty()) continue;

            int count = 0;
            std::from_chars(count_str.data(), count_str.data() + count_str.size(), count);
            if (count <= 0) continue;

            bool matched = false;
            if (count > 1) {
                for (int i = 0; i < count; ++i) {
                    std::string suffix = ttyname + "." + std::to_string(i);
                    if (name.ends_with(suffix)) { matched = true; break; }
                }
            } else {
                matched = name.ends_with(ttyname);
            }
            if (!matched) continue;

            info.serial_number = fbsd_property(node.value, "sernum=");
            std::erase(info.serial_number, '"');

            bool ok = false;
            uint16_t vid = parse_hex_id(fbsd_property(node.value, "vendor="), ok);
            if (ok) info.vendor_id = vid;
            ok = false;
            uint16_t pid = parse_hex_id(fbsd_property(node.value, "product="), ok);
            if (ok) info.product_id = pid;

            std::string base = node.name.substr(0, pnp_pos);
            std::string desc_key = base + "%desc";
            for (const auto& dn : nodes) {
                if (dn.name == desc_key && !dn.value.empty()) {
                    auto class_pos = dn.value.find(", class ");
                    info.description = (class_pos != std::string::npos)
                        ? dn.value.substr(0, class_pos) : dn.value;
                    info.manufacturer = info.description;
                    break;
                }
            }
            break;
        }

        if (name.starts_with("cua"))
            cua_list.push_back(std::move(info));
        else
            tty_list.push_back(std::move(info));
    }

    // Match cua/tty pairs
    for (const auto& cua : cua_list) {
        std::string cua_token = fbsd_property(cua.port_name, "cua");
        for (const auto& tty : tty_list) {
            std::string tty_token = fbsd_property(tty.port_name, "tty");
            if (cua_token == tty_token) {
                result.push_back(cua);
                result.push_back(tty);
            }
        }
    }

    // ---- Windows ----
#elif defined(_WIN32)
    struct SetupToken { GUID guid; DWORD flags; };
    const SetupToken tokens[] = {
        { GUID_DEVCLASS_PORTS,             DIGCF_PRESENT },
        { GUID_DEVCLASS_MODEM,             DIGCF_PRESENT },
        { GUID_DEVINTERFACE_COMPORT_LOCAL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE },
        { GUID_DEVINTERFACE_MODEM_LOCAL,   DIGCF_PRESENT | DIGCF_DEVICEINTERFACE },
    };

    auto already_listed = [&](const std::string& name) {
        return std::any_of(result.begin(), result.end(),
                           [&](const port_info& p) { return p.port_name == name; });
    };

    for (const auto& tok : tokens) {
        HDEVINFO info_set = SetupDiGetClassDevsW(&tok.guid, nullptr, nullptr, tok.flags);
        if (info_set == INVALID_HANDLE_VALUE) continue;

        SP_DEVINFO_DATA info_data{};
        info_data.cbSize = sizeof(info_data);

        for (DWORD i = 0; SetupDiEnumDeviceInfo(info_set, i, &info_data); ++i) {
            std::string name = device_port_name(info_set, &info_data);
            if (name.empty() || name.find("LPT") != std::string::npos)
                continue;
            if (already_listed(name))
                continue;

            port_info info;
            info.port_name = name;
            info.system_location = port_name_to_system_location(name);
            info.description = device_registry_property(info_set, &info_data, SPDRP_DEVICEDESC);
            info.manufacturer = device_registry_property(info_set, &info_data, SPDRP_MFG);

            std::string inst_id = device_instance_identifier(info_data.DevInst);
            info.serial_number = device_serial(inst_id, info_data.DevInst);

            bool ok = false;
            uint16_t vid = device_vid(inst_id, ok);
            if (ok) info.vendor_id = vid;
            ok = false;
            uint16_t pid = device_pid(inst_id, ok);
            if (ok) info.product_id = pid;

            result.push_back(std::move(info));
        }

        SetupDiDestroyDeviceInfoList(info_set);
    }

    // Fallback: registry
    for (const auto& name : ports_from_registry()) {
        if (!already_listed(name)) {
            port_info info;
            info.port_name = name;
            info.system_location = port_name_to_system_location(name);
            result.push_back(std::move(info));
        }
    }

#endif // platform

    return result;
}

std::vector<int32_t> standard_baud_rates()
{
  return {9600,   19200,  38400,  57600,  115200,  230400,
          250000, 460800, 500000, 921600, 1000000, 2000000};
}
} // namespace serial
