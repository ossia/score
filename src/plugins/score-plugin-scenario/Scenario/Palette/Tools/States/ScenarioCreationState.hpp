#pragma once
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateSequence.hpp>
#include <Scenario/Commands/Scenario/Creations/CreationMetaCommand.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Palette/ScenarioPaletteBaseStates.hpp>
#include <Scenario/Palette/ScenarioPaletteBaseTransitions.hpp>
#include <Scenario/Palette/Tool.hpp>
#include <Scenario/Palette/Tools/ScenarioRollbackStrategy.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>
#include <Scenario/Tools/elementFindingHelper.hpp>

#include <score/command/Dispatchers/MultiOngoingCommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/selection/SelectionDispatcher.hpp>

#include <QDebug>

namespace Scenario
{

template <int Value>
class StrongQState : public QState
{
public:
  static constexpr auto value() { return Value; }
  StrongQState(QState* parent) : QState{parent}
  {
    this->setObjectName(debug_StateMachineIDs<Value>());
  }
};

template <typename Scenario_T>
class CreationStateBase : public StateBase<Scenario_T>
{
public:
  using StateBase<Scenario_T>::StateBase;

  QVector<Id<StateModel>> createdStates;
  QVector<Id<EventModel>> createdEvents;
  QVector<Id<TimeSyncModel>> createdTimeSyncs;
  QVector<Id<IntervalModel>> createdIntervals;

  void clearCreatedIds()
  {
    createdEvents.clear();
    createdIntervals.clear();
    createdTimeSyncs.clear();
    createdStates.clear();
  }
};

// Here to prevent pollution of the CreationState header with the command
// dispatcher
template <typename Scenario_T, typename ToolPalette_T>
class CreationState : public CreationStateBase<Scenario_T>
{
public:
  CreationState(
      const ToolPalette_T& sm,
      const score::CommandStackFacade& stack,
      const Scenario_T& scenarioPath,
      QState* parent)
      : CreationStateBase<Scenario_T>{scenarioPath, parent}, m_parentSM{sm}, m_dispatcher{stack}
  {
  }

protected:
  void createToState_base(const Id<StateModel>& originalState)
  {
    if (this->hoveredState)
    {
      const bool graphal = isCreatingGraph();
      const bool differentParents
          = Scenario::parentEvent(originalState, m_parentSM.model()).timeSync()
            != Scenario::parentEvent(*this->hoveredState, m_parentSM.model()).timeSync();

      if (graphal && differentParents)
      {
        auto cmd = new Scenario::Command::CreateInterval{
            this->m_scenario, originalState, *this->hoveredState, true};

        m_dispatcher.submit(cmd);

        this->createdIntervals.append(cmd->createdInterval());
      }
      else
      {
        // make sure the hovered corresponding timesync dont have a date prior
        // to original state date
        if (getDate(m_parentSM.model(), originalState)
            < getDate(m_parentSM.model(), *this->hoveredState))
        {
          auto cmd = new Scenario::Command::CreateInterval{
              this->m_scenario, originalState, *this->hoveredState};

          m_dispatcher.submit(cmd);

          this->createdIntervals.append(cmd->createdInterval());
        }
      }
    }
  }

  void createToEvent_base(const Id<StateModel>& originalState)
  {
    if (this->hoveredEvent)
    {
      const bool graphal = isCreatingGraph();
      const bool differentParents
          = Scenario::parentEvent(originalState, m_parentSM.model()).timeSync()
            != m_parentSM.model().event(*this->hoveredEvent).timeSync();
      const bool timeIsInOrder = getDate(m_parentSM.model(), originalState)
                                 < getDate(m_parentSM.model(), *this->hoveredEvent);
      // make sure the hovered corresponding timesync dont have a date prior to
      // original state date
      if ((graphal || timeIsInOrder) && differentParents)
      {
        auto cmd = new Scenario::Command::CreateInterval_State{
            this->m_scenario, originalState, *this->hoveredEvent, this->currentPoint.y, graphal};

        m_dispatcher.submit(cmd);

        this->createdIntervals.append(cmd->createdInterval());
        this->createdStates.append(cmd->createdState());
      } // else do nothing
    }
  }

  void createToTimeSync_base(const Id<StateModel>& originalState)
  {
    if (this->hoveredTimeSync)
    {
      const bool graphal = isCreatingGraph();
      const bool differentParents
          = Scenario::parentEvent(originalState, m_parentSM.model()).timeSync()
            != *this->hoveredTimeSync;
      const bool timeIsInOrder = getDate(m_parentSM.model(), originalState)
                                 < getDate(m_parentSM.model(), *this->hoveredTimeSync);
      // make sure the hovered corresponding timesync dont have a date prior to
      // original state date
      if ((graphal || timeIsInOrder) && differentParents)
      {
        auto cmd = new Scenario::Command::CreateInterval_State_Event{
            this->m_scenario,
            originalState,
            *this->hoveredTimeSync,
            this->currentPoint.y,
            graphal};

        m_dispatcher.submit(cmd);

        this->createdStates.append(cmd->createdState());
        this->createdEvents.append(cmd->createdEvent());
        this->createdIntervals.append(cmd->createdInterval());
      }
    }
  }

  void createToNothing_base(const Id<StateModel>& originalState)
  {
    if (m_parentSM.editionSettings().tool() != Tool::CreateSequence)
    {
      auto cmd = new Scenario::Command::CreateInterval_State_Event_TimeSync{
          this->m_scenario,
          originalState, // Put there in createInitialState
          this->currentPoint.date,
          this->currentPoint.y,
          isCreatingGraph()};

      m_dispatcher.submit(cmd);

      this->createdStates.append(cmd->createdState());
      this->createdEvents.append(cmd->createdEvent());
      this->createdTimeSyncs.append(cmd->createdTimeSync());
      this->createdIntervals.append(cmd->createdInterval());
    }
    else
    {

      // This
      auto cmd = Scenario::Command::CreateSequence::make(
          this->m_parentSM.context().context,
          this->m_parentSM.model(),
          originalState, // Put there in createInitialState
          this->currentPoint.date,
          this->currentPoint.y);

      m_dispatcher.submitQuiet(cmd);

      this->createdStates.append(cmd->createdState());
      this->createdEvents.append(cmd->createdEvent());
      this->createdTimeSyncs.append(cmd->createdTimeSync());
      this->createdIntervals.append(cmd->createdInterval());
    }
  }

  void makeSnapshot()
  {
    const score::DocumentContext& ctx = this->m_parentSM.context().context;
    if (!ctx.app.settings<Scenario::Settings::Model>().getSnapshotOnCreate())
      return;

    using namespace Command;
    if (m_parentSM.editionSettings().tool() == Tool::CreateSequence)
      return;

    if (this->createdStates.empty())
      return;

    if (!this->createdIntervals.empty())
    {
      const auto& cst = m_parentSM.model().intervals.at(this->createdIntervals.last());
      if (!cst.processes.empty())
      {
        // In case of the presence of a sequence, we
        // only use the sequence's namespace, hence we don't need to make a
        // snapshot at the end..
        return;
      }
    }

    auto& device_explorer = ctx.template plugin<Explorer::DeviceDocumentPlugin>().explorer();

    State::MessageList messages = getSelectionSnapshot(device_explorer);
    if (messages.empty())
      return;

    m_dispatcher.submit(new AddMessagesToState{
        m_parentSM.model().states.at(this->createdStates.last()), messages});
  }

  template <typename DestinationState, typename Function>
  void add_transition(QState* from, DestinationState* to, Function&& fun)
  {
    using transition_type = Transition_T<Scenario_T, DestinationState::value()>;
    auto trans = score::make_transition<transition_type>(from, to, *this);
    trans->setObjectName(QString::number(DestinationState::value()));
    QObject::connect(trans, &transition_type::triggered, this, fun);
  }

  void commit()
  {
    this->makeSnapshot();
    this->m_dispatcher.template commit<Scenario::Command::CreationMetaCommand>();

    // Select all the created elements
    Selection sel;
    if (!this->createdStates.empty())
    {
      auto& s = this->createdStates.back();
      auto sp = m_parentSM.model().states.find(s);
      if (sp == m_parentSM.model().states.end())
      {
        qDebug() << "Error: tried to select state but it did not exist";
      }
      else
      {
        sel.append(*sp);
      }
    }

    if (!this->createdIntervals.empty())
    {
      auto& i = this->createdIntervals.back();
      auto ip = m_parentSM.model().intervals.find(i);
      if (ip == m_parentSM.model().intervals.end())
      {
        qDebug() << "Error: tried to select interval but it did not exist";
      }
      else
      {
        sel.append(*ip);
      }
    }

    score::SelectionDispatcher d{this->m_parentSM.context().context.selectionStack};
    d.select(sel);
    this->clearCreatedIds();
  }

  void rollback()
  {
    m_dispatcher.template rollback<ScenarioRollbackStrategy>();
    this->clearCreatedIds();
  }

  inline bool isCreatingGraph() const noexcept
  {
    return this->m_parentSM.editionSettings().tool() == Scenario::Tool::CreateGraph;
  }

  const ToolPalette_T& m_parentSM;
  MultiOngoingCommandDispatcher m_dispatcher;

  Scenario::Point m_clickedPoint{};
};
}
