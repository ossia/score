#include "FormatRegistry.hpp"

#include <mutex>
#include <utility>

namespace Gfx
{

namespace
{
struct State
{
  std::mutex mutex;
  std::vector<FormatRegistry::Entry> entries;
};

State& instance()
{
  // Function-local Meyers singleton — registrations at static-init
  // time across translation units don't depend on dynamic-init order.
  static State s;
  return s;
}
} // namespace

void FormatRegistry::register_format(Entry e)
{
  if(e.format_id.empty())
    return;
  auto& s = instance();
  std::lock_guard lock{s.mutex};
  for(auto& existing : s.entries)
  {
    if(existing.format_id == e.format_id)
    {
      existing = std::move(e);
      return;
    }
  }
  s.entries.push_back(std::move(e));
}

std::vector<FormatRegistry::Entry> FormatRegistry::all()
{
  auto& s = instance();
  std::lock_guard lock{s.mutex};
  return s.entries;
}

const FormatRegistry::Entry*
FormatRegistry::find(std::string_view format_id) noexcept
{
  if(format_id.empty())
    return nullptr;
  auto& s = instance();
  std::lock_guard lock{s.mutex};
  for(const auto& e : s.entries)
    if(e.format_id == format_id)
      return &e;
  return nullptr;
}

} // namespace Gfx
