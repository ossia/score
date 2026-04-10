#pragma once
#include <ossia/detail/fmt.hpp>

#include <boost/algorithm/string.hpp>

#include <sys/types.h>

#include <dlfcn.h>
#include <elf.h>
#include <grp.h>
#include <stdio.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace linuxcheck
{
enum class Distro
{
  Unknown,
  Debian_Bookworm,
  Debian_Trixie,
  Fedora,
  Arch,
  Nix,
  OpenSUSE,
  Gentoo,
  Alpine,
  Void,
};

// Look for a given package manager by name
inline bool has_pkg_manager(const char* name)
{
  // Check standard binary paths
  std::string usr_bin = std::string("/usr/bin/") + name;
  if(access(usr_bin.c_str(), X_OK) == 0)
    return true;

  std::string bin = std::string("/bin/") + name;
  if(access(bin.c_str(), X_OK) == 0)
    return true;

  // Nix is special and uses its own store paths
  if(std::string_view(name) == "nix-env")
  {
    if(access("/run/current-system/sw/bin/nix-env", X_OK) == 0)
      return true;
    if(access("/nix/var/nix/profiles/default/bin/nix-env", X_OK) == 0)
      return true;
  }

  return false;
}

// Look for a given distro base from the package manager
inline Distro get_distro_from_package_manager()
{
  if(has_pkg_manager("apt-get"))
    return Distro::Debian_Trixie;
  if(has_pkg_manager("dnf"))
    return Distro::Fedora;
  if(has_pkg_manager("pacman"))
    return Distro::Arch;
  if(has_pkg_manager("zypper"))
    return Distro::OpenSUSE;
  if(has_pkg_manager("nix-env"))
    return Distro::Nix;
  if(has_pkg_manager("emerge"))
    return Distro::Gentoo;

  return Distro::Unknown;
}

inline Distro get_current_distro()
{
  static const auto distro = [] {
    std::ifstream os_release("/etc/os-release");
    if(!os_release)
      return get_distro_from_package_manager();

    std::string line, id, id_like, version_id;
    bool is_debian_like = false;

    // 1. Parse /etc/os-release
    while(std::getline(os_release, line))
    {
      line.erase(std::remove(line.begin(), line.end(), '"'), line.end());

      if(line.rfind("ID=", 0) == 0)
      {
        id = line.substr(3);
      }
      else if(line.rfind("ID_LIKE=", 0) == 0)
      {
        id_like = line.substr(8);
      }
      else if(line.rfind("VERSION_ID=", 0) == 0)
      {
        version_id = line.substr(11);
      }
    }

    // 2. Distro guess-work: first check with id
    if(id.starts_with("ubuntu") || id.starts_with("debian") || id.starts_with("pop")
       || id.starts_with("linuxmint"))
    {
      is_debian_like = true;
    }
    else if(id.starts_with("fedora"))
      return Distro::Fedora;
    else if(
        id.starts_with("arch") || id.starts_with("manjaro") || id.starts_with("cachy"))
      return Distro::Arch;
    else if(id.starts_with("nixos"))
      return Distro::Nix;
    else if(id.starts_with("opensuse") || line.starts_with("sles"))
      return Distro::OpenSUSE;
    else if(
        id.starts_with("rhel") || id.starts_with("fedora") || id.starts_with("centos")
        || id.starts_with("rocky") || id.starts_with("alma"))
      return Distro::Fedora;
    else if(id.starts_with("gentoo"))
      return Distro::Gentoo;
    else if(id.starts_with("alpine"))
      return Distro::Alpine;
    else if(id.starts_with("void"))
      return Distro::Void;

    // then with ID_LIKE
    if(!is_debian_like)
    {
      if(id_like.find("debian") != std::string::npos)
        is_debian_like = true;
      else if(id_like.find("ubuntu") != std::string::npos)
        is_debian_like = true;
      else if(id_like.find("suse") != std::string::npos)
        return Distro::OpenSUSE;
      else if(id_like.find("arch") != std::string::npos)
        return Distro::Arch;
      else if(id_like.find("rhel") != std::string::npos)
        return Distro::Fedora;
      else if(id_like.find("fedora") != std::string::npos)
        return Distro::Fedora;
      else if(id_like.find("centos") != std::string::npos)
        return Distro::Fedora;
    }

    // Debian had a change of package with recent versions having e.g.
    // libasound2t64 vs libasound2 for older versions
    if(is_debian_like)
    {
      bool is_new_debian_like = false;
      try
      {
        float version = std::stof(version_id);

        if(id == "ubuntu" || id == "pop")
        {
          is_new_debian_like = version >= 24.04f;
        }
        else if(id == "debian")
        {
          is_new_debian_like = version >= 13.0f;
        }
        else if(id == "linuxmint")
        {
          is_new_debian_like = version >= 22.0f;
        }
      }
      catch(...)
      {
        if(version_id == "trixie" || version_id == "sid")
          is_new_debian_like = true;
      }
      if(is_new_debian_like)
        return Distro::Debian_Trixie;
      else
        return Distro::Debian_Bookworm;
    }

    return Distro::Unknown;
  }();
  return distro;
}

struct PackageInfo
{
  // Debian, Ubuntu, Mint, Pop!_OS...
  std::string_view debian_bookworm;
  std::string_view debian_trixie;
  std::string_view fedora;     // Fedora, RHEL, CentOS, Asahi
  std::string_view arch;       // Arch Linux, CachyOS, Manjaro
  std::string_view nix;        // NixOS
  std::string_view opensuse;   // openSUSE Leap, Tumbleweed, SLES
  std::string_view gentoo;     // Gentoo
  std::string_view alpine;     // Alpine Linux
  std::string_view void_linux; // Void Linux
};

inline const std::unordered_map<std::string, PackageInfo>& get_package_db()
{
  // clang-format off
  static const std::unordered_map<std::string, PackageInfo> db = {
      //                                          bookworm,  trixie,                 fedora,                        arch,                    nix,                        opensuse,                gentoo,                           alpine,                  void
      {"libz.so.1",                              {"zlib1g",  "zlib1g",               "zlib",                        "zlib",                  "zlib",                     "libz1",                 "sys-libs/zlib",                  "zlib",                  "zlib"}}                 ,
      {"libudev.so.1",                         {"libudev1",  "libudev1",             "systemd-libs",                "systemd-libs",          "systemd",                  "libudev1",              "virtual/libudev",                "eudev-libs",            "eudev-libudev"}}        ,
      {"libv4l2.so.0",                         {"libv4l-0",  "libv4l-0",             "libv4l",                      "v4l-utils",             "libv4l",                   "libv4l2-0",             "media-tv/v4l-utils",             "v4l-utils",             "v4l-utils"}}            ,
      {"libasound.so.2",                     {"libasound2",  "libasound2t64",        "alsa-lib",                    "alsa-lib",              "alsa-lib",                 "libasound2",            "media-libs/alsa-lib",            "alsa-lib",              "alsa-lib"}}             ,
      {"libdbus-1.so.3",                    {"libdbus-1-3",  "libdbus-1-3",          "dbus-libs",                   "dbus",                  "dbus",                     "libdbus-1-3",           "sys-apps/dbus",                  "dbus-libs",             "dbus"}}                 ,
      {"libbluetooth.so.3",               {"libbluetooth3",  "libbluetooth3",        "bluez-libs",                  "bluez-libs",            "bluez",                    "libbluetooth3",         "net-wireless/bluez",             "bluez-libs",            "bluez"}}                ,
      {"libwayland-egl.so.1",           {"libwayland-egl1",  "libwayland-egl1",      "libwayland-egl",              "wayland",               "wayland",                  "libwayland-egl1",       "dev-libs/wayland",               "wayland-egl",           "wayland"}}              ,
      {"libwayland-cursor.so.0",     {"libwayland-cursor0",  "libwayland-cursor0",   "libwayland-cursor",           "wayland",               "wayland",                  "libwayland-cursor0",    "dev-libs/wayland",               "wayland-libs-cursor",   "wayland"}}              ,
      {"libwayland-client.so.0",     {"libwayland-client0",  "libwayland-client0",   "libwayland-client",           "wayland",               "wayland",                  "libwayland-client0",    "dev-libs/wayland",               "wayland-libs-client",   "wayland"}}              ,
      {"libGLX.so.0",                           {"libglx0",  "libglx0",              "libglvnd-glx",                "libglvnd",              "libglvnd",                 "libglvnd",              "media-libs/libglvnd",            "libglvnd",              "libglvnd"}}             ,
      {"libOpenGL.so.0",                     {"libopengl0",  "libopengl0",           "libglvnd-opengl",             "libglvnd",              "libglvnd",                 "libglvnd",              "media-libs/libglvnd",            "libglvnd",              "libglvnd"}}             ,
      {"libEGL.so.1",                           {"libegl1",  "libegl1",              "libglvnd-egl",                "libglvnd",              "libglvnd",                 "libglvnd",              "media-libs/libglvnd",            "libglvnd",              "libglvnd"}}             ,
      {"libdrm.so.2",                           {"libdrm2",  "libdrm2",              "libdrm",                      "libdrm",                "libdrm",                   "libdrm2",               "x11-libs/libdrm",                "libdrm",                "libdrm"}}               ,
      {"libgbm.so.1",                           {"libgbm1",  "libgbm1",              "mesa-libgbm",                 "mesa",                  "mesa",                     "libgbm1",               "media-libs/mesa",                "mesa-gbm",              "mesa"}}                 ,
      {"libX11.so.6",                          {"libx11-6",  "libx11-6",             "libX11",                      "libx11",                "xorg.libX11",              "libX11-6",              "x11-libs/libX11",                "libx11",                "libX11"}}               ,
      {"libX11-xcb.so.1",                   {"libx11-xcb1",  "libx11-xcb1",          "libX11-xcb",                  "libx11",                "xorg.libX11",              "libX11-xcb1",           "x11-libs/libX11",                "libx11",                "libX11"}}               ,
      {"libxkbcommon.so.0",               {"libxkbcommon0",  "libxkbcommon0",        "libxkbcommon",                "libxkbcommon",          "libxkbcommon",             "libxkbcommon0",         "x11-libs/libxkbcommon",          "libxkbcommon",          "libxkbcommon"}}         ,
      {"libxkbcommon-x11.so.0",      {"libxkbcommon-x11-0",  "libxkbcommon-x11-0",   "libxkbcommon-x11",            "libxkbcommon-x11",      "libxkbcommon",             "libxkbcommon-x11-0",    "x11-libs/libxkbcommon",          "libxkbcommon",          "libxkbcommon-x11"}}     ,
      {"libXext.so.6",                         {"libxext6",  "libxext6",             "libXext",                     "libxext",               "xorg.libXext",             "libXext6",              "x11-libs/libXext",               "libxext",               "libXext"}}              ,
      {"libXcomposite.so.1",             {"libxcomposite1",  "libxcomposite1",       "libXcomposite",               "libxcomposite",         "xorg.libXcomposite",       "libXcomposite1",        "x11-libs/libXcomposite",         "libxcomposite",         "libXcomposite"}}        ,
      {"libXrandr.so.2",                     {"libxrandr2",  "libxrandr2",           "libXrandr",                   "libxrandr",             "xorg.libXrandr",           "libXrandr2",            "x11-libs/libXrandr",             "libxrandr",             "libXrandr"}}            ,
      {"libsystemd.so.0",                   {"libsystemd0",  "libsystemd0",          "systemd-libs",                "systemd-libs",          "systemd",                  "libsystemd0",           "sys-apps/systemd",               "elogind",               "elogind"}}              ,
      {"libxcb.so.1",                           {"libxcb1",  "libxcb1",              "libxcb",                      "libxcb",                "xorg.libxcb",              "libxcb1",               "x11-libs/libxcb",                "libxcb",                "libxcb"}}               ,
      {"libxcb-glx.so.0",                   {"libxcb-glx0",  "libxcb-glx0",          "libxcb",                      "libxcb",                "xorg.libxcb",              "libxcb-glx0",           "x11-libs/libxcb",                "libxcb",                "libxcb"}}               ,
      {"libxcb-randr.so.0",               {"libxcb-randr0",  "libxcb-randr0",        "libxcb",                      "libxcb",                "xorg.libxcb",              "libxcb-randr0",         "x11-libs/libxcb",                "libxcb",                "libxcb"}}               ,
      {"libxcb-shm.so.0",                   {"libxcb-shm0",  "libxcb-shm0",          "libxcb",                      "libxcb",                "xorg.libxcb",              "libxcb-shm0",           "x11-libs/libxcb",                "libxcb",                "libxcb"}}               ,
      {"libxcb-sync.so.1",                 {"libxcb-sync1",  "libxcb-sync1",         "libxcb",                      "libxcb",                "xorg.libxcb",              "libxcb-sync1",          "x11-libs/libxcb",                "libxcb",                "libxcb"}}               ,
      {"libxcb-xfixes.so.0",             {"libxcb-xfixes0",  "libxcb-xfixes0",       "libxcb",                      "libxcb",                "xorg.libxcb",              "libxcb-xfixes0",        "x11-libs/libxcb",                "libxcb",                "libxcb"}}               ,
      {"libxcb-render.so.0",             {"libxcb-render0",  "libxcb-render0",       "libxcb",                      "libxcb",                "xorg.libxcb",              "libxcb-render0",        "x11-libs/libxcb",                "libxcb",                "libxcb"}}               ,
      {"libxcb-shape.so.0",               {"libxcb-shape0",  "libxcb-shape0",        "libxcb",                      "libxcb",                "xorg.libxcb",              "libxcb-shape0",         "x11-libs/libxcb",                "libxcb",                "libxcb"}}               ,
      {"libxcb-xkb.so.1",                   {"libxcb-xkb1",  "libxcb-xkb1",          "libxcb",                      "libxcb",                "xorg.libxcb",              "libxcb-xkb1",           "x11-libs/libxcb",                "libxcb",                "libxcb"}}               ,
      {"libxcb-cursor.so.0",             {"libxcb-cursor0",  "libxcb-cursor0",       "xcb-util-cursor",             "xcb-util-cursor",       "xorg.xcbutilcursor",       "libxcb-cursor0",        "x11-libs/xcb-util-cursor",       "xcb-util-cursor",       "xcb-util-cursor"}}      ,
      {"libxcb-icccm.so.4",               {"libxcb-icccm4",  "libxcb-icccm4",        "xcb-util-wm",                 "xcb-util-wm",           "xorg.xcbutilwm",           "libxcb-icccm4",         "x11-libs/xcb-util-wm",           "xcb-util-wm",           "xcb-util-wm"}}          ,
      {"libxcb-util.so.1",                 {"libxcb-util1",  "libxcb-util1",         "xcb-util",                    "xcb-util",              "xorg.xcbutil",             "libxcb-util1",          "x11-libs/xcb-util",              "xcb-util",              "xcb-util"}}             ,
      {"libxcb-image.so.0",               {"libxcb-image0",  "libxcb-image0",        "xcb-util-image",              "xcb-util-image",        "xorg.xcbutilimage",        "libxcb-image0",         "x11-libs/xcb-util-image",        "xcb-util-image",        "xcb-util-image"}}       ,
      {"libxcb-keysyms.so.1",           {"libxcb-keysyms1",  "libxcb-keysyms1",      "xcb-util-keysyms",            "xcb-util-keysyms",      "xorg.xcbutilkeysyms",      "libxcb-keysyms1",       "x11-libs/xcb-util-keysyms",      "xcb-util-keysyms",      "xcb-util-keysyms"}}     ,
      {"libxcb-render-util.so.0",   {"libxcb-render-util0",  "libxcb-render-util0",  "xcb-util-renderutil",         "xcb-util-renderutil",   "xorg.xcbutilrenderutil",   "libxcb-render-util0",   "x11-libs/xcb-util-renderutil",   "xcb-util-renderutil",   "xcb-util-renderutil"}}  ,
      {"libavahi-client.so.3",         {"libavahi-client3",  "libavahi-client3",     "avahi-libs",                  "avahi",                 "avahi",                    "libavahi-client3",      "net-dns/avahi",                  "avahi-libs",            "avahi"}}                ,
      {"avahi-daemon",                     {"avahi-daemon",  "avahi-daemon",         "avahi",                       "avahi",                 "avahi",                    "avahi",                 "net-dns/avahi",                  "avahi",                 "avahi"}}                ,
      {"dbus-broker",                       {"dbus-broker",  "dbus-broker",          "dbus-broker",                 "dbus-broker",           "dbus-broker",              "dbus-broker",           "sys-apps/dbus-broker",           "dbus-broker",           "dbus-broker"}}          ,
      {"jackd",                                  {"jackd2",  "jackd2",               "jack-audio-connection-kit",   "jack2",                 "jack2",                    "jack",                  "media-sound/jack2",              "jack",                  "jack"}}                 ,
  };
  // clang-format on
  return db;
}

inline std::string_view suggest_package(std::string_view missing_item, Distro distro)
{
  const auto& db = get_package_db();

  // For OR-separated strings like "libasound.so.2|libjack.so.0", just look up the first one
  std::string primary_item(missing_item);
  auto pipe_pos = primary_item.find('|');
  if(pipe_pos != std::string::npos)
  {
    primary_item = primary_item.substr(0, pipe_pos);
  }

  auto it = db.find(primary_item);
  if(it == db.end())
    return ""; // No suggestion available

  switch(distro)
  {
    case Distro::Debian_Bookworm:
      return it->second.debian_bookworm;
    case Distro::Debian_Trixie:
      return it->second.debian_trixie;
    case Distro::Fedora:
      return it->second.fedora;
    case Distro::Arch:
      return it->second.arch;
    case Distro::Nix:
      return it->second.nix;
    case Distro::OpenSUSE:
      return it->second.opensuse;
    case Distro::Gentoo:
      return it->second.gentoo;
    case Distro::Alpine:
      return it->second.alpine;
    case Distro::Void:
      return it->second.void_linux;
    default:
      return "";
  }
}
class ElfInspector
{
public:
  std::vector<std::string> get_dt_needed(std::string_view filename)
  {
    std::vector<std::string> libraries;

    m_file.open(filename.data(), std::ios::binary);
    if(!m_file)
    {
      std::cerr << "Cannot open file: " << filename << "\n";
      return libraries;
    }

    // Read ELF magic and class
    unsigned char e_ident[EI_NIDENT];
    m_file.read(reinterpret_cast<char*>(e_ident), EI_NIDENT);

    if(e_ident[EI_MAG0] != ELFMAG0 || e_ident[EI_MAG1] != ELFMAG1
       || e_ident[EI_MAG2] != ELFMAG2 || e_ident[EI_MAG3] != ELFMAG3)
    {
      std::cerr << filename << " is not a valid ELF file\n";
      return libraries;
    }

    if(e_ident[EI_CLASS] != ELFCLASS64)
    {
      std::cerr << filename << " is not a valid 64-bit ELF file\n";
      return libraries;
    }

    process_elf64(libraries);

    return libraries;
  }

private:
  template <typename T>
  T read_at(std::streampos pos)
  {
    T value;
    m_file.seekg(pos);
    m_file.read(reinterpret_cast<char*>(&value), sizeof(T));
    if(!m_file)
      throw std::runtime_error("Failed to read from file");
    return value;
  }

  std::string read_string_at(std::streampos pos)
  {
    m_file.seekg(pos);
    std::string result;
    char c;
    while(m_file.get(c) && c != '\0')
      result += c;
    return result;
  }

  void process_elf64(std::vector<std::string>& libraries)
  {
    // Read ELF64 header
    Elf64_Ehdr ehdr = read_at<Elf64_Ehdr>(0);

    // Find dynamic section
    auto phdrs = std::unique_ptr<Elf64_Phdr[]>(new Elf64_Phdr[ehdr.e_phnum]);
    m_file.seekg(ehdr.e_phoff);
    m_file.read(reinterpret_cast<char*>(phdrs.get()), ehdr.e_phnum * sizeof(Elf64_Phdr));

    Elf64_Phdr* dynamic_phdr = nullptr;
    for(int i = 0; i < ehdr.e_phnum; ++i)
    {
      if(phdrs[i].p_type == PT_DYNAMIC)
      {
        dynamic_phdr = &phdrs[i];
        break;
      }
    }

    if(!dynamic_phdr)
      return;

    // Read dynamic section
    std::vector<Elf64_Dyn> dyns(dynamic_phdr->p_filesz / sizeof(Elf64_Dyn));
    m_file.seekg(dynamic_phdr->p_offset);
    m_file.read(reinterpret_cast<char*>(dyns.data()), dynamic_phdr->p_filesz);

    // Find string table
    Elf64_Addr strtab_addr = 0;
    for(const auto& dyn : dyns)
    {
      if(dyn.d_tag == DT_STRTAB)
      {
        strtab_addr = dyn.d_un.d_ptr;
        break;
      }
    }

    // Convert virtual address to file offset
    Elf64_Off strtab_offset = 0;
    for(int i = 0; i < ehdr.e_phnum; ++i)
    {
      if(phdrs[i].p_type == PT_LOAD && strtab_addr >= phdrs[i].p_vaddr
         && strtab_addr < phdrs[i].p_vaddr + phdrs[i].p_memsz)
      {
        strtab_offset = phdrs[i].p_offset + (strtab_addr - phdrs[i].p_vaddr);
        break;
      }
    }

    // Extract DT_NEEDED entries
    for(const auto& dyn : dyns)
    {
      if(dyn.d_tag == DT_NEEDED)
      {
        std::string lib = read_string_at(strtab_offset + dyn.d_un.d_val);
        libraries.push_back(lib);
      }
    }
  }

  std::ifstream m_file;
};

inline bool is_in_group(std::string_view group)
{
  struct group* grp = ::getgrnam(group.data());
  if(!grp)
    return false;

  gid_t target_gid = grp->gr_gid;
  int ngroups = ::getgroups(0, nullptr);
  auto groups = (gid_t*)alloca(ngroups * sizeof(gid_t));

  if(::getgroups(ngroups, groups) == -1)
    return false;

  for(int i = 0; i < ngroups; i++)
  {
    if(groups[i] == target_gid)
      return true;
  }
  return false;
}

inline void check_cpu(std::string& ret)
{
#if defined(__x86_64__)
  if(!__builtin_cpu_supports("avx2"))
  {
    ret += 
        "AVX2 not supported! Use at your own risk. For a build which works on older "
        "CPUs, use Flatpak, the .deb packages or build from source.\n";
  }
#endif
}

inline void check_libraries(std::string_view path_to_binary, std::string& ret)
{
  std::set<std::string_view> suggested_packages_list;
  // First check the libraries the app is directly linking against.
  // Not having them will prevent everything from running.
  for(auto dylib : ElfInspector{}.get_dt_needed(path_to_binary))
  {
    if(auto lib = dlopen(dylib.data(), RTLD_LAZY))
    {
      dlclose(lib);
    }
    else
    {
      ret += fmt::format("{}: MISSING LIBRARY!\n", dylib);

      if(auto distro = get_current_distro(); distro != Distro::Unknown)
      {
        const auto suggestion = suggest_package(dylib, distro);
        if(!suggestion.empty())
        {
          suggested_packages_list.insert(suggestion);
        }
      }
    }
  }

  // Then check the ones that we know we dlopen at run-time
  static constexpr std::string_view libraries[]
      = {"libudev.so.1",
         "libX11.so.6",
         "libXext.so.6",
         "libXcomposite.so.1",
         "libXrandr.so.2",
         "libresolv.so.2",
         "libasound.so.2|libjack.so.0|libjack.so.1|libjack.so.2|libpipewire-0.3.so.0",
         "libdbus-1.so.3",
         "libbluetooth.so.3",
         "libsystemd.so.0",
         "libv4l2.so.0",
         "libavahi-client.so.3"};
  for(auto dylibs : libraries)
  {
    std::vector<std::string> split_on_pipe;
    boost::split(split_on_pipe, dylibs, boost::is_any_of("|"));
    bool missing = true;
    for(std::string dylib : split_on_pipe)
    {
      if(auto lib = dlopen(dylib.data(), RTLD_LAZY))
      {
        dlclose(lib);
        missing = false;
        break;
      }
    }

    if(missing)
    {
      ret += fmt::format("{}: MISSING LIBRARY!\n", dylibs);

      if(auto distro = get_current_distro(); distro != Distro::Unknown)
      {
        const auto suggestion = suggest_package(dylibs, distro);
        if(!suggestion.empty())
        {
          suggested_packages_list.insert(suggestion);
        }
      }
    }
  }

  if(!suggested_packages_list.empty())
  {

    std::string suggested_packages;
    for(auto& str : suggested_packages_list)
    {
      suggested_packages += str;
      suggested_packages += ' ';
    }
    const auto d = get_current_distro();
    if(d == Distro::Unknown)
      return;

    ret += fmt::format("\n\n -> Run the following command:\n  ");
    switch(d)
    {
      case Distro::Debian_Bookworm:
      case Distro::Debian_Trixie:
        ret += fmt::format("sudo apt install {}\n", suggested_packages);
        break;
      case Distro::Fedora:
        ret += fmt::format("sudo yum install {}\n", suggested_packages);
        break;
      case Distro::Arch:
        ret += fmt::format("sudo pacman -S {}\n", suggested_packages);
        break;
      case Distro::Nix:
        ret += fmt::format("nix-shell -p {}\n", suggested_packages);
        break;
      case Distro::OpenSUSE:
        ret += fmt::format("sudo zypper install {}\n", suggested_packages);
        break;
      case Distro::Gentoo:
        ret += fmt::format("sudo emerge -av {}\n", suggested_packages);
        break;
      case Distro::Alpine:
        ret += fmt::format("sudo apk add {}\n", suggested_packages);
        break;
      case Distro::Void:
        ret += fmt::format("sudo xbps-install -S {}\n", suggested_packages);
        break;
      case Distro::Unknown:
        break;
    }
  }
}

inline void check_binaries(std::string_view path_to_binary, std::string& ret)
{
  static constexpr std::string_view binaries[]
      = {"avahi-daemon|/usr/sbin/avahi-daemon", "dbus-broker|dbus-daemon",
         "jackd|jackd2|pipewire"};
  for(auto programs : binaries)
  {
    std::vector<std::string> split_program_on_pipe;
    boost::split(split_program_on_pipe, programs, boost::is_any_of("|"));
    bool missing = true;
    for(std::string p : split_program_on_pipe)
    {
      if(!system(fmt::format("command -v {} > /dev/null 2>&1", p).c_str()))
      {
        missing = false;
        break;
      }
    }

    if(missing)
    {
      ret += fmt::format("{}: MISSING PROGRAM!\n", programs);

      if(auto distro = get_current_distro(); distro != Distro::Unknown)
      {
        const auto suggestion = suggest_package(programs, distro);
        if(!suggestion.empty())
        {
          ret += fmt::format(
              " -> Run the following command:\n  {}\n", programs, suggestion);
        }
      }
    }
  }
}

inline std::string diagnostics(std::string_view path_to_binary)
{
  std::string ret;
  check_cpu(ret);
  check_libraries(path_to_binary, ret);
  check_binaries(path_to_binary, ret);
  return ret;
}
}
