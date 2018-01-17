#pragma once
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
namespace Midi
{

class DropMidiInSenario final : public Scenario::DropHandler
{
  SCORE_CONCRETE("8F162598-9E4E-4865-A861-81DF01D2CDF0")

  bool drop(
      const Scenario::TemporalScenarioPresenter&,
      QPointF pos,
      const QMimeData* mime) override;
};


}
