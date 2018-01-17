#pragma once
#include <Process/Process.hpp>
#include <Engine/Node/Layer.hpp>

namespace Media::Effect
{

class DefaultEffectItem:
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
