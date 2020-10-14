#pragma once
#include <Scenario/Palette/Tools/ScenarioToolState.hpp>
#include <Scenario/Palette/Tools/States/ResizeSlotState.hpp>
#include <Scenario/Palette/Tools/States/ScenarioSelectionState.hpp>
#include <Scenario/Palette/Transitions/IntervalTransitions.hpp>
#include <Scenario/Palette/Transitions/SlotTransitions.hpp>
#include <Scenario/Palette/Transitions/StateTransitions.hpp>
#include <Scenario/Process/Algorithms/StandardDisplacementPolicy.hpp>

#include <score/selection/SelectionDispatcher.hpp>

namespace Scenario
{
class ToolPalette;

// TODO Generic smart tool with variadic...
template <
    typename Scenario_T,
    typename ToolPalette_T,
    typename View_T,
    typename MoveIntervalWrapper_T,
    typename MoveLeftBraceWrapper_T,
    typename MoveRightBraceWrapper_T,
    typename MoveEventWrapper_T,
    typename MoveTimeSyncWrapper_T>
class SmartTool final : public ToolBase<ToolPalette_T>
{
public:
  SmartTool(ToolPalette_T& sm) : ToolBase<ToolPalette_T>{sm}
  {
    auto sub = new QState{&this->localSM()};
    sub->setChildMode(QState::ParallelStates);

    // Selection
    m_state = new SelectionState<ToolPalette_T, View_T>{
        this->m_palette.context().context.selectionStack,
        this->m_palette,
        this->m_palette.presenter().view(),
        sub};

    this->localSM().setInitialState(sub);

    // Other actions; they are in //.
    auto actionsState = new QState(sub);
    {
      auto waitState = new QState(actionsState);
      actionsState->setInitialState(waitState);

      auto mov_i = MoveIntervalWrapper_T::template make<Scenario_T, ToolPalette_T>(
          this->m_palette, waitState, *actionsState);
      auto mov_lb = MoveLeftBraceWrapper_T::template make<Scenario_T, ToolPalette_T>(
          this->m_palette, waitState, *actionsState);
      auto mov_rb = MoveRightBraceWrapper_T::template make<Scenario_T, ToolPalette_T>(
          this->m_palette, waitState, *actionsState);
      auto mov_e = MoveEventWrapper_T::template make<Scenario_T, ToolPalette_T>(
          this->m_palette, waitState, *actionsState);
      auto mov_ts = MoveTimeSyncWrapper_T::template make<Scenario_T, ToolPalette_T>(
          this->m_palette, waitState, *actionsState);

      if constexpr (!std::is_same_v<decltype(mov_i), std::nullptr_t>)
      {
        score::make_transition<ClickOnState_Transition<Scenario_T>>(mov_i, mov_e, *mov_e);
        score::make_transition<ClickOnState_Transition<Scenario_T>>(mov_ts, mov_e, *mov_e);
        score::make_transition<ClickOnState_Transition<Scenario_T>>(mov_lb, mov_e, *mov_e);
        score::make_transition<ClickOnState_Transition<Scenario_T>>(mov_rb, mov_e, *mov_e);

        score::make_transition<ClickOnInterval_Transition<Scenario_T>>(mov_ts, mov_i, *mov_i);
        score::make_transition<ClickOnInterval_Transition<Scenario_T>>(mov_e, mov_i, *mov_i);
        score::make_transition<ClickOnInterval_Transition<Scenario_T>>(mov_lb, mov_i, *mov_i);
        score::make_transition<ClickOnInterval_Transition<Scenario_T>>(mov_rb, mov_i, *mov_i);
      }
      /// Slot resize
      auto resizeSlot = new ResizeSlotState<Scenario_T, ToolPalette_T>{
          this->m_palette.context().context.commandStack, this->m_palette, actionsState};

      score::make_transition<ClickOnSlotHandle_Transition>(waitState, resizeSlot, *resizeSlot);

      resizeSlot->addTransition(resizeSlot, finishedState(), waitState);
    }

    this->localSM().start();
  }

  void on_pressed(QPointF scene, Scenario::Point sp)
  {
    using namespace std;

    this->mapTopItem(
        this->itemUnderMouse(scene),
        [&](const Id<StateModel>& id) // State
        {
          const auto& elt = this->m_palette.presenter().state(id);

          m_state->dispatcher.select(filterSelections(
              &elt.model(),
              this->m_palette.model().selectedChildren(),
              m_state->multiSelection()));

          this->localSM().postEvent(new ClickOnState_Event{id, sp});
          m_nothingPressed = false;
        },
        [&](const Id<EventModel>& id) // Event
        {
          const auto& elt = this->m_palette.presenter().event(id);

          m_state->dispatcher.select(filterSelections(
              &elt.model(),
              this->m_palette.model().selectedChildren(),
              m_state->multiSelection()));

          this->localSM().postEvent(new ClickOnEvent_Event{id, sp});
          m_nothingPressed = false;
        },
        [&](const Id<TimeSyncModel>& id) // TimeSync
        {
          const auto& elt = this->m_palette.presenter().timeSync(id);

          m_state->dispatcher.select(filterSelections(
              &elt.model(),
              this->m_palette.model().selectedChildren(),
              m_state->multiSelection()));
          this->localSM().postEvent(new ClickOnTimeSync_Event{id, sp});
          m_nothingPressed = false;
        },
        [&](const Id<IntervalModel>& id) // Interval
        {
          const auto& model = this->m_palette.model().interval(id);

          if (!model.selection.get())
          {
            m_state->dispatcher.select(filterSelections(
                &model, this->m_palette.model().selectedChildren(), m_state->multiSelection()));
          }
          this->localSM().postEvent(new ClickOnInterval_Event{id, sp});
          m_nothingPressed = false;
        },
        [&](const Id<IntervalModel>& id) // LeftBrace
        {
          const auto& elt = this->m_palette.presenter().interval(id);

          if (!elt.isSelected())
          {
            m_state->dispatcher.select(filterSelections(
                &elt.model(),
                this->m_palette.model().selectedChildren(),
                m_state->multiSelection()));
          }

          this->localSM().postEvent((new ClickOnLeftBrace_Event{id, sp}));
          m_nothingPressed = false;
        },
        [&](const Id<IntervalModel>& id) // RightBrace
        {
          const auto& elt = this->m_palette.presenter().interval(id);

          if (!elt.isSelected())
          {
            m_state->dispatcher.select(filterSelections(
                &elt.model(),
                this->m_palette.model().selectedChildren(),
                m_state->multiSelection()));
          }

          this->localSM().postEvent((new ClickOnRightBrace_Event{id, sp}));
          m_nothingPressed = false;
        },
        [&](const SlotPath& slot) // Slot handle
        {
          this->localSM().postEvent(new ClickOnSlotHandle_Event{slot});
          m_nothingPressed = false;
        },
        [&]() {
          this->localSM().postEvent(new score::Press_Event);
          m_nothingPressed = true;
        });

    m_moved = false;
  }

  void on_moved(QPointF scene, Scenario::Point sp)
  {
    if (m_nothingPressed)
    {
      this->localSM().postEvent(new score::Move_Event);
    }
    else
    {
      m_moved = true;
      this->mapTopItem(
          this->itemUnderMouse(scene),
          [&](const Id<StateModel>& id) {
            this->localSM().postEvent(new MoveOnState_Event{id, sp});
          }, // state
          [&](const Id<EventModel>& id) {
            this->localSM().postEvent(new MoveOnEvent_Event{id, sp});
          }, // event
          [&](const Id<TimeSyncModel>& id) {
            this->localSM().postEvent(new MoveOnTimeSync_Event{id, sp});
          }, // timesync
          [&](const Id<IntervalModel>& id) {
            this->localSM().postEvent(new MoveOnInterval_Event{id, sp});
          }, // interval
          [&](const Id<IntervalModel>& id) {
            this->localSM().postEvent(new MoveOnLeftBrace_Event{id, sp});
          }, // LeftBrace
          [&](const Id<IntervalModel>& id) {
            this->localSM().postEvent(new MoveOnRightBrace_Event{id, sp});
          }, // RightBrace
          [&](const SlotPath& slot) {
            this->localSM().postEvent(new MoveOnSlotHandle_Event{slot});
          }, // Slot handle
          [&]() { this->localSM().postEvent(new MoveOnNothing_Event{sp}); });
    }
  }

  void on_released(QPointF scene, Scenario::Point sp)
  {
    if (m_nothingPressed)
    {
      this->localSM().postEvent(new score::Release_Event); // select
      m_nothingPressed = false;

      return;
    }
    if (m_moved) // then don't change selection
    {
      this->localSM().postEvent(new ReleaseOnNothing_Event{sp});
      m_nothingPressed = false;

      return;
    }

    this->mapTopItem(
        this->itemUnderMouse(scene),
        [&](const Id<StateModel>& id) // State
        {
          this->localSM().postEvent(new ReleaseOnState_Event{id, sp});
        },
        [&](const Id<EventModel>& id) // Event
        {
          this->localSM().postEvent(new ReleaseOnEvent_Event{id, sp});
        },
        [&](const Id<TimeSyncModel>& id) // TimeSync
        {
          this->localSM().postEvent(new ReleaseOnTimeSync_Event{id, sp});
        },
        [&](const Id<IntervalModel>& id) // Interval
        {
          this->localSM().postEvent(new ReleaseOnInterval_Event{id, sp});
        },
        [&](const Id<IntervalModel>& id) // LeftBrace
        {
          this->localSM().postEvent(new ReleaseOnLeftBrace_Event{id, sp});
        },
        [&](const Id<IntervalModel>& id) // RightBrace
        {
          this->localSM().postEvent(new ReleaseOnRightBrace_Event{id, sp});
        },
        [&](const SlotPath& slot) // Slot handle
        { this->localSM().postEvent(new ReleaseOnSlotHandle_Event{slot}); },
        [&]() {
          this->localSM().postEvent(new ReleaseOnNothing_Event{sp}); // end of move
        });
  }

  void on_cancel() override { GraphicsSceneTool<Point>::on_cancel(); }

  auto& selectionState() const { return *m_state; }

private:
  SelectionState<ToolPalette_T, View_T>* m_state{};

  bool m_nothingPressed{true};
  bool m_moved{false};
};
}
