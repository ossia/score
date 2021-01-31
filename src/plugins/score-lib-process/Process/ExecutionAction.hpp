#pragma once
#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <ossia/audio/audio_tick.hpp>

#include <score_lib_process_export.h>

namespace Execution
{
class SCORE_LIB_PROCESS_EXPORT ExecutionAction : public score::InterfaceBase
{
  SCORE_INTERFACE(ExecutionAction, "1b08ebd8-4a5a-44a9-a448-3e90d7faf18a")
public:
  virtual ~ExecutionAction();
  virtual void startTick(const ossia::audio_tick_state& st);
  virtual void endTick(const ossia::audio_tick_state& st);
};

class SCORE_LIB_PROCESS_EXPORT ExecutionActionList final
    : public score::InterfaceList<ExecutionAction>
{
public:
  ~ExecutionActionList();
};
}
