#pragma once
#include <Process/StateProcess.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <iscore/model/Entity.hpp>
#include <iscore/selection/Selectable.hpp>
#include <iscore/tools/EntityMap.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/Metadata.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <nano_signal_slot.hpp>

#include <set>
#include <vector>

#include <iscore/component/Component.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore_plugin_scenario_export.h>
class DataStream;
class JSONObject;
namespace Process
{
class ProcessModel;
}
class ProcessStateDataInterface;

namespace iscore
{
class CommandStackFacade;
}

namespace Scenario
{
class EventModel;
class ConstraintModel;
class MessageItemModel;
class ProcessStateWrapper final : public QObject
{
private:
  ProcessStateDataInterface* m_proc;

public:
  ProcessStateWrapper(ProcessStateDataInterface* proc) : m_proc{proc}
  {
  }

  ProcessStateDataInterface& process() const
  {
    return *m_proc;
  }
};

// Model for the graphical state in a scenario.
class ISCORE_PLUGIN_SCENARIO_EXPORT StateModel final
    : public iscore::Entity<StateModel>,
      public Nano::Observer
{
  Q_OBJECT

  ISCORE_SERIALIZE_FRIENDS(Scenario::StateModel, DataStream)
  ISCORE_SERIALIZE_FRIENDS(Scenario::StateModel, JSONObject)
public:
  using ProcessVector = std::list<ProcessStateWrapper>;

  EntityMap<Process::StateProcess> stateProcesses;
  Selectable selection;

  StateModel(
      const Id<StateModel>& id,
      const Id<EventModel>& eventId,
      double yPos,
      const iscore::CommandStackFacade& stack,
      QObject* parent);

  // Copy
  StateModel(
      const StateModel& source,
      const Id<StateModel>&,
      const iscore::CommandStackFacade& stack,
      QObject* parent);

  // Load
  template <
      typename DeserializerVisitor,
      enable_if_deserializer<DeserializerVisitor>* = nullptr>
  StateModel(
      DeserializerVisitor&& vis,
      const iscore::CommandStackFacade& stack,
      QObject* parent)
      : Entity{vis, parent}, m_stack{stack}
  {
    vis.writeTo(*this);
    init();
  }

  double heightPercentage() const;

  MessageItemModel& messages() const;

  const Id<EventModel>& eventId() const;
  void setEventId(const Id<EventModel>&);

  const OptionalId<ConstraintModel>& previousConstraint() const;
  const OptionalId<ConstraintModel>& nextConstraint() const;

  // Note : the added constraint shall be in
  // the scenario when this is called.
  void setNextConstraint(const OptionalId<ConstraintModel>&);
  void setPreviousConstraint(const OptionalId<ConstraintModel>&);

  ProcessVector& previousProcesses()
  {
    return m_previousProcesses;
  }
  ProcessVector& followingProcesses()
  {
    return m_nextProcesses;
  }
  const ProcessVector& previousProcesses() const
  {
    return m_previousProcesses;
  }
  const ProcessVector& followingProcesses() const
  {
    return m_nextProcesses;
  }

  void setStatus(ExecutionStatus);
  ExecutionStatus status() const
  {
    return m_status.get();
  }

  void setHeightPercentage(double y);

signals:
  void sig_statesUpdated();
  void heightPercentageChanged();
  void statusChanged(Scenario::ExecutionStatus);

private:
  void statesUpdated_slt();
  void init(); // TODO check if other model elements need an init method too.
  const iscore::CommandStackFacade& m_stack;

  ProcessVector m_previousProcesses;
  ProcessVector m_nextProcesses;
  Id<EventModel> m_eventId;

  // OPTIMIZEME if we shift to Id = int, put this Optional
  OptionalId<ConstraintModel> m_previousConstraint;
  OptionalId<ConstraintModel> m_nextConstraint;

  double m_heightPercentage{0.5}; // In the whole scenario

  ptr<MessageItemModel> m_messageItemModel;
  ExecutionStatusProperty m_status{};
};
}

DEFAULT_MODEL_METADATA(Scenario::StateModel, "State")

TR_TEXT_METADATA(, Scenario::StateModel, PrettyName_k, "State")
