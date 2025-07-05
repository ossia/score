#pragma once
#include <Process/Preset.hpp>

#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/tools/Unused.hpp>

#include <score_lib_process_export.h>

namespace Process
{
class ProcessModel;
class SCORE_LIB_PROCESS_EXPORT LoadPresetCommandFactory : public score::InterfaceBase
{
  SCORE_INTERFACE(LoadPresetCommandFactory, "4a1a228c-16af-4647-917e-09cdf63fe167")
public:
  virtual ~LoadPresetCommandFactory();
  virtual bool matches(
      const Process::ProcessModel& obj, unused_t newval,
      const score::DocumentContext& ctx) const noexcept
      = 0;
  virtual score::Command* make(
      const Process::ProcessModel& obj, Process::Preset newval,
      const score::DocumentContext& ctx) const
      = 0;
};

class SCORE_LIB_PROCESS_EXPORT LoadPresetCommandFactoryList final
    : public score::MatchingFactory<LoadPresetCommandFactory>
{
public:
  ~LoadPresetCommandFactoryList();
};
}
