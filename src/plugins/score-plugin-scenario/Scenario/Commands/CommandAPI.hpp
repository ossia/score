#pragma once
#include <Dataflow/Commands/CableHelpers.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/AggregateCommand.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/document/DocumentContext.hpp>

#include <memory>

namespace Curve
{
struct CurveDomain;
}
namespace Scenario
{
class ScenarioDocumentModel;
namespace Command
{
class SCORE_PLUGIN_SCENARIO_EXPORT Macro
{
  RedoMacroCommandDispatcher<score::AggregateCommand> m;

public:
  Macro(score::AggregateCommand* cmd, const score::DocumentContext& ctx);
  ~Macro();

  StateModel&
  createState(const Scenario::ProcessModel& scenar, const Id<EventModel>& ev, double y);

  std::tuple<TimeSyncModel&, EventModel&, StateModel&>
  createDot(const Scenario::ProcessModel& scenar, Scenario::Point pt);

  IntervalModel&
  createBox(const Scenario::ProcessModel& scenar, TimeVal start, TimeVal end, double y);

  IntervalModel& createIntervalAfter(
      const Scenario::ProcessModel& scenar,
      const Id<Scenario::StateModel>& state,
      Scenario::Point pt);

  IntervalModel& createInterval(
      const Scenario::ProcessModel& scenar,
      const Id<Scenario::StateModel>& start,
      const Id<Scenario::StateModel>& end);

  Process::ProcessModel* createProcess(
      const Scenario::IntervalModel& interval,
      const UuidKey<Process::ProcessModel>& key,
      const QString& data,
      const QPointF& pos);

  template <typename T>
  T& createProcess(
      const Scenario::IntervalModel& interval,
      const QString& data,
      const QPointF& pos)
  {
    return *safe_cast<T*>(
        this->createProcess(interval, Metadata<ConcreteKey_k, T>::get(), data, pos));
  }

  Process::ProcessModel* createProcessInSlot(
      const Scenario::IntervalModel& interval,
      const UuidKey<Process::ProcessModel>& key,
      const QString& data,
      const QPointF& pos);

  template <typename T>
  T& createProcessInSlot(
      const Scenario::IntervalModel& interval,
      const QString& data,
      const QPointF& pos)
  {
    return *safe_cast<T*>(
        this->createProcessInSlot(interval, Metadata<ConcreteKey_k, T>::get(), data, pos));
  }

  Process::ProcessModel* createProcessInNewSlot(
      const Scenario::IntervalModel& interval,
      const UuidKey<Process::ProcessModel>& key,
      const QString& data);

  Process::ProcessModel* createProcessInNewSlot(
      const Scenario::IntervalModel& interval,
      const UuidKey<Process::ProcessModel>& key,
      const QString& data,
      const QPointF& pos);

  template <typename T>
  T& createProcessInNewSlot(const Scenario::IntervalModel& interval, const QString& data)
  {
    return *safe_cast<T*>(
        this->createProcessInNewSlot(interval, Metadata<ConcreteKey_k, T>::get(), data));
  }

  Process::ProcessModel*
  loadProcessInSlot(const Scenario::IntervalModel& interval, const rapidjson::Value& procdata);

  void clearInterval(const Scenario::IntervalModel&);

  void insertInInterval(
      rapidjson::Value&& sourceInterval,
      const IntervalModel& targetInterval,
      ExpandMode mode);

  void resizeInterval(const IntervalModel& itv, const TimeVal& dur);

  void setIntervalMin(const IntervalModel& itv, const TimeVal& dur, bool noMin);

  void setIntervalMax(const IntervalModel& itv, const TimeVal& dur, bool infinite);

  void createSlot(const Scenario::IntervalModel& interval);

  void addLayer(
      const Scenario::IntervalModel& interval,
      int slot_index,
      const Process::ProcessModel& proc);

  void
  addLayerToLastSlot(const Scenario::IntervalModel& interval, const Process::ProcessModel& proc);

  void
  addLayerInNewSlot(const Scenario::IntervalModel& interval, const Process::ProcessModel& proc);

  void addLayer(const Scenario::SlotPath& slotpath, const Process::ProcessModel& proc);

  void showRack(const Scenario::IntervalModel& interval);

  void
  resizeSlot(const Scenario::IntervalModel& interval, const SlotPath& slotPath, double newSize);

  void resizeSlot(const Scenario::IntervalModel& interval, SlotPath&& slotPath, double newSize);

  IntervalModel&
  duplicate(const Scenario::ProcessModel& scenario, const Scenario::IntervalModel& itv);

  Process::ProcessModel&
  duplicateProcess(const Scenario::IntervalModel& itv, const Process::ProcessModel& process);

  void pasteElements(
      const Scenario::ProcessModel& scenario,
      const rapidjson::Value& objs,
      Scenario::Point pos);

  void pasteElementsAfter(
      const ProcessModel& scenario,
      const TimeSyncModel& sync,
      const rapidjson::Value& objs,
      double scale);

  void mergeTimeSyncs(
      const Scenario::ProcessModel& scenario,
      const Id<TimeSyncModel>& a,
      const Id<TimeSyncModel>& b);

  void moveProcess(
      const Scenario::IntervalModel& old_interval,
      const Scenario::IntervalModel& new_interval,
      const Id<Process::ProcessModel>& proc);

  void
  moveSlot(const IntervalModel& old_interval, const IntervalModel& new_interval, int slot_idx);

  void
  removeProcess(const Scenario::IntervalModel& interval, const Id<Process::ProcessModel>& proc);

  Process::Cable& createCable(const Scenario::ScenarioDocumentModel& dp, const Process::Port& source, const Process::Port& sink);

  void removeCable(const Scenario::ScenarioDocumentModel& dp, Process::Cable& theCable);

  void loadCables(const ObjectPath& parent, const Dataflow::SerializedCables& c);
  void removeElements(const Scenario::ProcessModel& scenario, const Selection& sel);

  void addMessages(const Scenario::StateModel& state, State::MessageList msgs);

  std::vector<Process::ProcessModel*>
  automate(const Scenario::IntervalModel& scenar, const QString& addr);

  Process::ProcessModel& automate(
      const IntervalModel& interval,
      const std::vector<SlotPath>& slotList,
      Id<Process::ProcessModel> curveId,
      State::AddressAccessor address,
      const Curve::CurveDomain& dom,
      bool tween);

  template <typename Property, typename T, typename U>
  void setProperty(const T& object, U&& value)
  {
    auto cmd = new score::PropertyCommand_T<Property>{object, std::forward<U>(value)};
    m.submit(cmd);
  }

  void submit(score::Command* cmd);
  void commit();
};
}
}
