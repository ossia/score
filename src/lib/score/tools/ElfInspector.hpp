#pragma once

// Minimal ELF reader: lists the DT_NEEDED sonames of a 64-bit ELF binary.
// Header-only and dependency-free so that both the linuxcheck diagnostic
// tool and the plug-in hosts (e.g. the LV2 UI loader, which must refuse
// UIs linked against a different Qt major) can share it.
//
// Extracted from linuxcheck/diagnostics.hpp.

#if defined(__linux__)

#include <elf.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace score
{
class ElfInspector
{
public:
  //! nullopt: not a readable / valid / 64-bit ELF file (as opposed to a
  //! valid binary with an empty dependency list).
  std::optional<std::vector<std::string>>
  try_get_dt_needed(std::string_view filename)
  {
    std::vector<std::string> libraries;

    m_file.open(std::string(filename), std::ios::binary);
    if(!m_file)
      return std::nullopt;

    // Read ELF magic and class
    unsigned char e_ident[EI_NIDENT];
    m_file.read(reinterpret_cast<char*>(e_ident), EI_NIDENT);
    if(!m_file)
      return std::nullopt;

    if(e_ident[EI_MAG0] != ELFMAG0 || e_ident[EI_MAG1] != ELFMAG1
       || e_ident[EI_MAG2] != ELFMAG2 || e_ident[EI_MAG3] != ELFMAG3)
      return std::nullopt;

    if(e_ident[EI_CLASS] != ELFCLASS64)
      return std::nullopt;

    try
    {
      process_elf64(libraries);
    }
    catch(...)
    {
      // Truncated / corrupt file
      return std::nullopt;
    }

    return libraries;
  }

  //! linuxcheck-compatible interface: failures print to stderr and yield an
  //! empty list.
  std::vector<std::string> get_dt_needed(std::string_view filename)
  {
    if(auto res = try_get_dt_needed(filename))
      return *std::move(res);

    std::cerr << filename << " is not a readable, valid 64-bit ELF file\n";
    return {};
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

    // Read program headers
    auto phdrs = std::unique_ptr<Elf64_Phdr[]>(new Elf64_Phdr[ehdr.e_phnum]);
    m_file.seekg(ehdr.e_phoff);
    m_file.read(reinterpret_cast<char*>(phdrs.get()), ehdr.e_phnum * sizeof(Elf64_Phdr));
    if(!m_file)
      throw std::runtime_error("Failed to read program headers");

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
      return; // Statically linked or no dynamic section

    // Read dynamic section
    std::vector<Elf64_Dyn> dyns(dynamic_phdr->p_filesz / sizeof(Elf64_Dyn));
    m_file.seekg(dynamic_phdr->p_offset);
    m_file.read(reinterpret_cast<char*>(dyns.data()), dynamic_phdr->p_filesz);
    if(!m_file)
      throw std::runtime_error("Failed to read dynamic section");

    // Find string table address
    Elf64_Addr strtab_addr = 0;
    for(const auto& dyn : dyns)
    {
      if(dyn.d_tag == DT_STRTAB)
      {
        strtab_addr = dyn.d_un.d_ptr;
        break;
      }
    }

    if(!strtab_addr)
      return;

    // Convert virtual address to file offset
    Elf64_Off strtab_offset = 0;
    for(int i = 0; i < ehdr.e_phnum; ++i)
    {
      if(phdrs[i].p_type == PT_LOAD && strtab_addr >= phdrs[i].p_vaddr
         && strtab_addr < phdrs[i].p_vaddr + phdrs[i].p_filesz)
      {
        strtab_offset = strtab_addr - phdrs[i].p_vaddr + phdrs[i].p_offset;
        break;
      }
    }

    if(!strtab_offset)
      return;

    // Extract DT_NEEDED entries
    for(const auto& dyn : dyns)
    {
      if(dyn.d_tag == DT_NEEDED)
      {
        libraries.push_back(read_string_at(strtab_offset + dyn.d_un.d_val));
      }
    }
  }

  std::ifstream m_file;
};
}

#endif
