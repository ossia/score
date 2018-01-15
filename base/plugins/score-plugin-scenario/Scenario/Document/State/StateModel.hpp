#pragma once
#include <Process/Process.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <score/model/Entity.hpp>
#include <score/selection/Selectable.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/tools/Metadata.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/std/Optional.hpp>
#include <nano_signal_slot.hpp>

#include <set>
#include <vector>
#include <list>

#include <score/model/Component.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <score/tools/Todo.hpp>
#include <score_plugin_scenario_export.h>
class DataStream;
class JSONObject;
namespace Process
{
class ProcessModel;
}
class ProcessStateDataInterface;

namespace score
{
class CommandStackFacade;
}

namespace Scenario
{
class EventModel;
class IntervalModel;
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
class SCORE_PLUGIN_SCENARIO_EXPORT StateModel final
    : public score::Entity<StateModel>,
      public Nano::Observer
{
  Q_OBJECT

  SCORE_SERIALIZE_FRIENDS
public:
  using ProcessVector = std::list<ProcessStateWrapper>;

  score::EntityMap<Process::ProcessModel> stateProcesses;
  Selectable selection;

  StateModel(
      const Id<StateModel>& id,
      const Id<EventModel>& eventId,
      double yPos,
      const score::CommandStackFacade& stack,
      QObject* parent);

  // Load
  template <
      typename DeserializerVisitor,
      enable_if_deserializer<DeserializerVisitor>* = nullptr>
  StateModel(
      DeserializerVisitor&& vis,
      const score::CommandStackFacade& stack,
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

  const OptionalId<IntervalModel>& previousInterval() const;
  const OptionalId<IntervalModel>& nextInterval() const;

  // Note : the added interval shall be in
  // the scenario when this is called.
  void setNextInterval(const OptionalId<IntervalModel>&);
  void setPreviousInterval(const OptionalId<IntervalModel>&);

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

  bool empty() const
  {
    return !messages().rootNode().hasChild(0);
  }

Q_SIGNALS:
  void sig_statesUpdated();
  void heightPercentageChanged();
  void statusChanged(Scenario::ExecutionStatus);

private:
  void statesUpdated_slt();
  void init(); // TODO check if other model elements need an init method too.
  const score::CommandStackFacade& m_stack;

  ProcessVector m_previousProcesses;
  ProcessVector m_nextProcesses;
  Id<EventModel> m_eventId;

  // OPTIMIZEME if we shift to Id = int, put this Optional
  OptionalId<IntervalModel> m_previousInterval;
  OptionalId<IntervalModel> m_nextInterval;

  double m_heightPercentage{0.5}; // In the whole scenario

  ptr<MessageItemModel> m_messageItemModel;
  ExecutionStatusProperty m_status{};
};
}

DEFAULT_MODEL_METADATA(Scenario::StateModel, "State")

TR_TEXT_METADATA(, Scenario::StateModel, PrettyName_k, "State")
