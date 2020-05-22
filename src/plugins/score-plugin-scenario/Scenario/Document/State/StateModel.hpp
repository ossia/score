#pragma once
#include <Process/Instantiations.hpp>
#include <Process/Process.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/Metatypes.hpp>
#include <Scenario/Document/State/ItemModel/ControlItemModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>

#include <score/model/Component.hpp>
#include <score/model/EntityImpl.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/Identifier.hpp>
#include <score/selection/Selectable.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <score/tools/Metadata.hpp>
#include <score/tools/std/Optional.hpp>

#include <nano_signal_slot.hpp>
#include <score_plugin_scenario_export.h>

#include <list>
#include <set>
#include <vector>
#include <verdigris>
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
  ProcessStateWrapper(ProcessStateDataInterface* proc) : m_proc{proc} { }
  ~ProcessStateWrapper() override;

  ProcessStateDataInterface& process() const { return *m_proc; }
};

// Model for the graphical state in a scenario.
class SCORE_PLUGIN_SCENARIO_EXPORT StateModel final : public score::Entity<StateModel>,
                                                      public Nano::Observer
{
  W_OBJECT(StateModel)

  SCORE_SERIALIZE_FRIENDS
public:
  using ProcessVector = std::list<ProcessStateWrapper>;

  score::EntityMap<Process::ProcessModel> stateProcesses;
  Selectable selection;

  StateModel(
      const Id<StateModel>& id,
      const Id<EventModel>& eventId,
      double yPos,
      const score::DocumentContext& ctx,
      QObject* parent);

  ~StateModel() override;

  // Load
  template <typename DeserializerVisitor, enable_if_deserializer<DeserializerVisitor>* = nullptr>
  StateModel(DeserializerVisitor&& vis, const score::DocumentContext& ctx, QObject* parent)
      : Entity{vis, parent}, m_context{ctx}
  {
    vis.writeTo(*this);
    init();
  }

  const score::DocumentContext& context() const noexcept { return m_context; }

  double heightPercentage() const;

  MessageItemModel& messages() const;
  ControlItemModel& controlMessages() const;

  const Id<EventModel>& eventId() const;
  void setEventId(const Id<EventModel>&);

  const OptionalId<IntervalModel>& previousInterval() const;
  const OptionalId<IntervalModel>& nextInterval() const;

  // Note : the added interval shall be in
  // the scenario when this is called.
  void setNextInterval(const OptionalId<IntervalModel>&);
  void setPreviousInterval(const OptionalId<IntervalModel>&);

  ProcessVector& previousProcesses() { return m_previousProcesses; }
  ProcessVector& followingProcesses() { return m_nextProcesses; }
  const ProcessVector& previousProcesses() const { return m_previousProcesses; }
  const ProcessVector& followingProcesses() const { return m_nextProcesses; }

  void setStatus(ExecutionStatus);
  ExecutionStatus status() const { return m_status.get(); }

  void setHeightPercentage(double y);

  bool empty() const { return !messages().rootNode().hasChild(0); }

public:
  void sig_statesUpdated() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, sig_statesUpdated)
  void sig_controlMessagesUpdated()
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, sig_controlMessagesUpdated)

  void heightPercentageChanged() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, heightPercentageChanged)
  void statusChanged(Scenario::ExecutionStatus arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, statusChanged, arg_1)

  void eventChanged(Id<Scenario::EventModel> oldev, Id<Scenario::EventModel> newev)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, eventChanged, oldev, newev)

private:
  void statesUpdated_slt();
  void init(); // TODO check if other model elements need an init method too.

  const score::DocumentContext& m_context;

  ProcessVector m_previousProcesses;
  ProcessVector m_nextProcesses;
  Id<EventModel> m_eventId;

  // OPTIMIZEME if we shift to Id = int, put this std::optional
  OptionalId<IntervalModel> m_previousInterval;
  OptionalId<IntervalModel> m_nextInterval;

  double m_heightPercentage{0.5}; // In the whole scenario

  MessageItemModel* m_messageItemModel{};
  ControlItemModel* m_controlItemModel{};
  ExecutionStatusProperty m_status{};
};
}

DEFAULT_MODEL_METADATA(Scenario::StateModel, "State")
TR_TEXT_METADATA(, Scenario::StateModel, PrettyName_k, "State")
