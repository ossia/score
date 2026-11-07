#include <JitCpp/JitBuildTree.hpp>

#include <score_plugin_jit_build_tree.hpp>

namespace Jit
{
std::span<const char* const> buildTreeIncludeDirs() noexcept
{
  return build_tree::include_dirs;
}

std::span<const char* const> buildTreeDefinitions() noexcept
{
  return build_tree::definitions;
}

std::span<const char* const> buildTreeFlags() noexcept
{
  return build_tree::flags;
}
}
