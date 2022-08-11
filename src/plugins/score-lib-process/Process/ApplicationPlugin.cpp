#include <Process/ApplicationPlugin.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Process::ApplicationPlugin)

namespace Process
{

ApplicationPlugin::ApplicationPlugin(const score::ApplicationContext& ctx)
    : score::ApplicationPlugin{ctx}
{
  presets.reserve(500);
}

ApplicationPlugin::~ApplicationPlugin() { }

void ApplicationPlugin::addPreset(Preset&& p)
{
  auto it = std::lower_bound(
      presets.begin(), presets.end(), p,
      [](const auto& lhs, const auto& rhs) { return lhs.key < rhs.key; });

  presets.insert(it, std::move(p));
}

}
