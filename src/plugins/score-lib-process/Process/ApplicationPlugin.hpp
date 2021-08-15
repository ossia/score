#pragma once
#include <Process/Preset.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>

namespace Process
{

class SCORE_LIB_PROCESS_EXPORT ApplicationPlugin
    : public score::ApplicationPlugin
{
public:
  explicit ApplicationPlugin(const score::ApplicationContext& ctx);

  ~ApplicationPlugin();

  void addPreset(Process::Preset&& p);

  std::vector<Process::Preset> presets;
};

}
