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
#include <stdexcept>
#include <string>
#include <vector>

namespace linuxcheck
{
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

inline void check_groups(std::string& ret)
{
  if(!is_in_group("audio"))
    ret += "Unix user is not in audio group! Some audio features will not work.\n";
  if(!is_in_group("realtime"))
    ret += "User not in realtime group! "
        "If doing audio or precise control, consider setting up your OS for real-time processing.\n";
  if(!is_in_group("uucp") && !is_in_group("dialout"))
    ret += "User not in uucp (Arch, Suse, Nix) / dialout (Debian, Ubuntu Fedora) group! "
        "Some features such as serial port access or some DMX chips won't work.\n";
  if(!is_in_group("input"))
    ret += "User not in input group! Raw evdev device access and some gamepads will not work.\n";
  if(!is_in_group("video"))
    ret += "User not in video group! Some camera features may not work.\n";
  if(!is_in_group("bluetooth"))
    ret += "User not in bluetooth group! Bluetooth support will not work.\n";
#if !defined(__x86_64__)
  if(!is_in_group("gpio"))
    ret += "User not in gpio group! Raw GPIO access will not work.\n";
  if(!is_in_group("i2c"))
    ret += "User not in i2c group! Raw hardware access (I2C, etc.) will not work.\n";
#endif
}

inline void check_libraries(std::string_view path_to_binary, std::string& ret)
{
  static constexpr std::string_view libraries[]
      = {"libudev.so.1",
         "libX11.so.6",
         "libresolv.so.2",
         "libasound.so.2|libjack.so.0|libjack.so.1|libjack.so.2|libpipewire-0.3.so.0",
         "libdbus-1.so.3",
         "libbluetooth.so.3",
         "libv4l2.so.0",
         "libavahi-client.so.3"};

  for(auto dylib : ElfInspector{}.get_dt_needed(path_to_binary))
  {
    if(auto lib = dlopen(dylib.data(), RTLD_LAZY))
    {
      dlclose(lib);
    }
    else
    {
      ret += fmt::format("{}: MISSING LIBRARY!\n", dylib);
    }
  }

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
    }
  }
}

inline std::string diagnostics(std::string_view path_to_binary)
{
  std::string ret;
  check_cpu(ret);
  check_groups(ret);
  check_libraries(path_to_binary, ret);
  check_binaries(path_to_binary, ret);
  return ret;
}
}
