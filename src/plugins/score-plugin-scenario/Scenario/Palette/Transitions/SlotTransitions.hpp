#pragma once

#include <score/statemachine/StateMachineUtils.hpp>

#include <Scenario/Palette/ScenarioPaletteBaseEvents.hpp>
#include <score_plugin_scenario_export.h>
class QEvent;
namespace Scenario
{
class SlotState;
} // namespace Scenario

namespace Scenario
{
class SCORE_PLUGIN_SCENARIO_EXPORT ClickOnSlotHandle_Transition final
    : public score::MatchedTransition<ClickOnSlotHandle_Event>
{
public:
  ClickOnSlotHandle_Transition(Scenario::SlotState& state);

  Scenario::SlotState& state() const;

protected:
  void onTransition(QEvent* ev) override;

private:
  Scenario::SlotState& m_state;
};
class SCORE_PLUGIN_SCENARIO_EXPORT MoveOnSlotHandle_Transition final
    : public score::MatchedTransition<MoveOnSlotHandle_Event>
{
public:
  MoveOnSlotHandle_Transition(Scenario::SlotState& state);

  Scenario::SlotState& state() const;

protected:
  void onTransition(QEvent* ev) override;

private:
  Scenario::SlotState& m_state;
};

class SCORE_PLUGIN_SCENARIO_EXPORT ReleaseOnSlotHandle_Transition final
    : public score::MatchedTransition<ReleaseOnSlotHandle_Event>
{
public:
  ReleaseOnSlotHandle_Transition(Scenario::SlotState& state);

  Scenario::SlotState& state() const;

protected:
  void onTransition(QEvent* ev) override;

private:
  Scenario::SlotState& m_state;
};
}
