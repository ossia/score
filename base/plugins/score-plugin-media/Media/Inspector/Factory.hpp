#pragma once
#include <Media/Inspector/AudioInspector.hpp>
#include <Media/Sound/SoundModel.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

namespace Media
{
namespace Sound
{
class InspectorFactory final
    : public Process::
          InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
  SCORE_CONCRETE("10360b4c-4d73-4274-8339-408644408cab")
};
}
namespace Input
{
class InspectorFactory final
    : public Process::
          InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
  SCORE_CONCRETE("1dd43b23-1e1d-4f06-9032-e5bd379d59e4")
};
}
}
