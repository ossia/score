#pragma once
#include <score_plugin_jit_export.h>

#include <span>

// What the JIT compiles with when it compiles against score's build tree
// instead of an SDK. The values are generated at configure time, per
// configuration under a multi-configuration generator, into a header only
// JitBuildTree.cpp includes: an installed SDK, whose include/score holds this
// header, has no such generated file.
namespace Jit
{
//! The include directories of score's libraries and plug-ins.
//! Entries can be empty.
SCORE_PLUGIN_JIT_EXPORT std::span<const char* const> buildTreeIncludeDirs() noexcept;

//! The compile definitions of the JIT plug-in, without the -D.
//! Entries can be empty.
SCORE_PLUGIN_JIT_EXPORT std::span<const char* const> buildTreeDefinitions() noexcept;

//! The -D and -U flags of the configuration's compiler flags.
//! Entries can be empty.
SCORE_PLUGIN_JIT_EXPORT std::span<const char* const> buildTreeFlags() noexcept;
}
