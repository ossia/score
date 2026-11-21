#pragma once
#include <Process/ProcessFactory.hpp>

#include <score/plugins/UuidKey.hpp>

namespace Library
{
//! Snippets are timeline content saved as a .scenario file: the processes,
//! the boxes holding them and the cables between them. The process library
//! lists them under their own branch with this key, which no process has:
//! they are not created like a process but pasted, as their file is dropped.
inline constexpr Process::ProcessModelFactory::ConcreteKey snippetKey() noexcept
{
  return_uuid("7d4f3a2e-5b9c-4e61-a8d0-2c7e9f1b3a64");
}

//! The folder of the user library where snippets are saved.
inline constexpr const char* snippetsFolder() noexcept
{
  return "snippets";
}

inline constexpr const char* snippetSuffix() noexcept
{
  return "scenario";
}
}
