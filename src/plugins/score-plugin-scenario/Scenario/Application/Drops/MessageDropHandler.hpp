#pragma once
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>

namespace Scenario
{
namespace Command
{
class Macro;
}

//! Creates the state for a cue dropped at pos: on the event of the nearby
//! state, at the end of an interval after it, or standalone.
SCORE_PLUGIN_SCENARIO_EXPORT const StateModel& createCueState(
    Scenario::Command::Macro& m, const ScenarioPresenter& pres, QPointF pos,
    MagneticStates& magnetic);

/**
 * @brief The MessageDropHandler class
 * Will create a state in the scenario at the
 * point a MessageList is dropped.
 */
class MessageDropHandler final : public GhostIntervalDropHandler
{
  SCORE_CONCRETE("75156fa7-e083-4c9c-a88c-3a05c54f330f")

public:
  MessageDropHandler();

private:
  bool drop(const ScenarioPresenter&, QPointF drop, const QMimeData& mime) override;
};
}
