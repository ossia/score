#pragma once
#include <Process/Process.hpp>
#include <Process/Dataflow/Port.hpp>
#include <score/widgets/RectItem.hpp>

namespace Media::Effect
{

class DefaultEffectItem final:
    public score::RectItem
{
  public:
    DefaultEffectItem(
        const Process::ProcessModel& effect,
        const score::DocumentContext& doc,
        score::RectItem* root);

    void setupInlet(
        Process::ControlInlet& inlet,
        const score::DocumentContext& doc);

};


}
