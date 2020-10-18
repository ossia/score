#include <Dataflow/Commands/EditConnection.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Scenario/Commands/Cohesion/CreateCurves.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/ResizeInterval.hpp>

#include <score_plugin_scenario_commands_files.hpp>

namespace Scenario
{
namespace Command
{

Macro::Macro(score::AggregateCommand* cmd, const score::DocumentContext& ctx)
    : m{std::unique_ptr<score::AggregateCommand>(cmd), ctx.commandStack}
{
}

Macro::~Macro()
{
  m.rollback();
}

StateModel& Macro::createState(const ProcessModel& scenar, const Id<EventModel>& ev, double y)
{
  auto cmd = new Scenario::Command::CreateState{scenar, ev, y};
  m.submit(cmd);
  return scenar.states.at(cmd->createdState());
}

std::tuple<TimeSyncModel&, EventModel&, StateModel&>
Macro::createDot(const ProcessModel& scenar, Point pt)
{
  auto cmd = new CreateTimeSync_Event_State{scenar, pt.date, pt.y};
  m.submit(cmd);
  return std::tie(
      scenar.timeSyncs.at(cmd->createdTimeSync()),
      scenar.events.at(cmd->createdEvent()),
      scenar.states.at(cmd->createdState()));
}

IntervalModel& Macro::createBox(const ProcessModel& scenar, TimeVal start, TimeVal end, double y)
{
  auto c_st = new CreateTimeSync_Event_State{scenar, start, y};
  m.submit(c_st);

  auto c_itv
      = new CreateInterval_State_Event_TimeSync{scenar, c_st->createdState(), end, y, false};
  m.submit(c_itv);

  return scenar.intervals.at(c_itv->createdInterval());
}

IntervalModel&
Macro::createIntervalAfter(const ProcessModel& scenar, const Id<StateModel>& state, Point pt)
{
  auto cmd = new CreateInterval_State_Event_TimeSync{scenar, state, pt.date, pt.y, false};
  m.submit(cmd);
  return scenar.intervals.at(cmd->createdInterval());
}

IntervalModel& Macro::createInterval(
    const ProcessModel& scenar,
    const Id<StateModel>& start,
    const Id<StateModel>& end)
{
  auto cmd = new CreateInterval{scenar, start, end};
  m.submit(cmd);
  return scenar.intervals.at(cmd->createdInterval());
}

Process::ProcessModel* Macro::createProcess(
    const IntervalModel& interval,
    const UuidKey<Process::ProcessModel>& key,
    const QString& data,
    const QPointF& pos)
{
  auto process_cmd = new AddOnlyProcessToInterval{interval, key, data, pos};
  m.submit(process_cmd);
  auto it = interval.processes.find(process_cmd->processId());
  if (it != interval.processes.end())
    return &(*it);
  return nullptr;
}

Process::ProcessModel* Macro::createProcessInSlot(
    const IntervalModel& interval,
    const UuidKey<Process::ProcessModel>& key,
    const QString& data,
    const QPointF& pos)
{
  auto process_cmd = new AddProcessToInterval{interval, key, data, pos};
  m.submit(process_cmd);
  auto it = interval.processes.find(process_cmd->processId());
  if (it != interval.processes.end())
    return &(*it);
  return nullptr;
}

Process::ProcessModel* Macro::createProcessInNewSlot(
    const IntervalModel& interval,
    const UuidKey<Process::ProcessModel>& key,
    const QString& data)
{
  return createProcessInNewSlot(interval, key, data, newProcessPosition(interval));
}

Process::ProcessModel* Macro::createProcessInNewSlot(
    const IntervalModel& interval,
    const UuidKey<Process::ProcessModel>& key,
    const QString& data,
    const QPointF& pos)
{
  if (auto proc = createProcess(interval, key, data, pos))
  {
    if(!(proc->flags() & Process::ProcessFlags::TimeIndependent))
    {
      addLayerInNewSlot(interval, *proc);
    }
    return proc;
  }
  return nullptr;
}

Process::ProcessModel*
Macro::loadProcessInSlot(const IntervalModel& interval, const rapidjson::Value& procdata)
{
  auto process_cmd = new LoadProcessInInterval{interval, procdata};
  m.submit(process_cmd);
  auto it = interval.processes.find(process_cmd->processId());
  if (it != interval.processes.end())
    return &(*it);
  return nullptr;
}

void Macro::createSlot(const IntervalModel& interval)
{
  m.submit(new AddSlotToRack{interval});
}

void Macro::addLayer(
    const IntervalModel& interval,
    int slot_index,
    const Process::ProcessModel& proc)
{
  addLayer(SlotPath{interval, slot_index}, proc);
}

void Macro::addLayerToLastSlot(const IntervalModel& interval, const Process::ProcessModel& proc)
{
  addLayer(SlotPath{interval, int(interval.smallView().size() - 1)}, proc);
}

void Macro::addLayerInNewSlot(const IntervalModel& interval, const Process::ProcessModel& proc)
{
  m.submit(new AddLayerInNewSlot{interval, proc.id()});
}

void Macro::addLayer(const SlotPath& slotpath, const Process::ProcessModel& proc)
{
  m.submit(new AddLayerModelToSlot{slotpath, proc});
}

void Macro::showRack(const IntervalModel& interval)
{
  m.submit(new ShowRack{interval});
}

void Macro::resizeSlot(const IntervalModel& interval, const SlotPath& slotPath, double newSize)
{
  auto cmd = new ResizeSlotVertically{interval, slotPath, newSize};
  m.submit(cmd);
}

void Macro::resizeSlot(const IntervalModel& interval, SlotPath&& slotPath, double newSize)
{
  auto cmd = new ResizeSlotVertically{interval, std::move(slotPath), newSize};
  m.submit(cmd);
}

Scenario::IntervalModel&
Macro::duplicate(const Scenario::ProcessModel& scenario, const IntervalModel& itv)
{
  auto cmd = new DuplicateInterval{scenario, itv};
  m.submit(cmd);
  return scenario.intervals.at(cmd->createdId());
}

Process::ProcessModel&
Macro::duplicateProcess(const IntervalModel& itv, const Process::ProcessModel& process)
{
  auto cmd = new DuplicateOnlyProcessToInterval{itv, process};
  m.submit(cmd);
  return itv.processes.at(cmd->processId());
}

void Macro::pasteElements(const ProcessModel& scenario, const rapidjson::Value& objs, Point pos)
{
  auto cmd = new ScenarioPasteElements(scenario, objs, pos);
  m.submit(cmd);
}

void Macro::pasteElementsAfter(
    const ProcessModel& scenario,
    const TimeSyncModel& sync,
    const rapidjson::Value& objs,
    double scale)
{
  auto cmd = new ScenarioPasteElementsAfter(scenario, sync, objs, scale);
  m.submit(cmd);
}

void Macro::mergeTimeSyncs(
    const ProcessModel& scenario,
    const Id<TimeSyncModel>& a,
    const Id<TimeSyncModel>& b)
{
  auto cmd = new Command::MergeTimeSyncs(scenario, a, b);
  m.submit(cmd);
}

void Macro::moveProcess(
    const IntervalModel& old_interval,
    const IntervalModel& new_interval,
    const Id<Process::ProcessModel>& proc)
{
  auto cmd = new Command::MoveProcess(old_interval, new_interval, proc);
  m.submit(cmd);
}

void Macro::moveSlot(
    const IntervalModel& old_interval,
    const IntervalModel& new_interval,
    int slot_idx)
{
  const auto old_slot = old_interval.smallView()[slot_idx];
  const auto old_procs = old_slot.processes;

  // OPTIMIZEME by putting it in a single command.
  // Hard to do but the current approach wastes a lot of memory.
  // To the future reader: don't forget to do a getStrongIdRange.

  Scenario::Slot new_slot;
  new_slot.height = old_slot.height;
  for (auto& proc : old_procs)
  {
    auto cmd = new Command::MoveProcess(old_interval, new_interval, proc, false);
    m.submit(cmd);
    const auto& old_id = cmd->oldProcessId();
    const auto& new_id = cmd->newProcessId();

    if (old_id == old_slot.frontProcess)
    {
      new_slot.frontProcess = new_id;
    }

    new_slot.processes.push_back(new_id);
  }

  auto cmd = new Command::AddSlotToRack{new_interval, std::move(new_slot)};
  m.submit(cmd);
}

void Macro::removeProcess(const IntervalModel& interval, const Id<Process::ProcessModel>& proc)
{
  m.submit(new RemoveProcessFromInterval{interval, proc});
}

Process::Cable& Macro::createCable(const ScenarioDocumentModel& dp, const Process::Port& source, const Process::Port& sink)
{
  auto id = getStrongId(dp.cables);
  m.submit(new Dataflow::CreateCable(dp, id, Process::CableType::ImmediateGlutton, source, sink));
  return dp.cables.at(id);
}

void Macro::removeCable(const ScenarioDocumentModel& dp, Process::Cable& theCable)
{
  m.submit(new Dataflow::RemoveCable(dp, theCable));
}

void Macro::loadCables(const ObjectPath& parent, const Dataflow::SerializedCables& c)
{
  m.submit(new Dataflow::LoadCables{parent, c});
}

void Macro::removeElements(const Scenario::ProcessModel& scenario, const Selection& sel)
{
  m.submit(new RemoveSelection{scenario, sel});
}

void Macro::addMessages(const StateModel& state, State::MessageList msgs)
{
  m.submit(new AddMessagesToState{state, std::move(msgs)});
}

std::vector<Process::ProcessModel*> Macro::automate(const IntervalModel& cst, const QString& str)
{
  // Find the address in the device explorer
  if (auto addr = State::parseAddressAccessor(str))
  {
    auto& ctx = m.stack().context();
    auto fa
        = Explorer::makeFullAddressAccessorSettings(*addr, ctx, ossia::value{}, ossia::value{});

    return CreateCurvesFromAddress(cst, std::move(fa), *this);
  }

  return {};
}

Process::ProcessModel& Macro::automate(
    const IntervalModel& interval,
    const std::vector<SlotPath>& slotList,
    Id<Process::ProcessModel> curveId,
    State::AddressAccessor address,
    const Curve::CurveDomain& dom,
    bool tween)
{
  auto c = new CreateAutomationFromStates{interval, slotList, curveId, address, dom, tween};
  m.submit(c);
  return interval.processes.at(c->processId());
}

void Macro::clearInterval(const IntervalModel& itv)
{
  m.submit(new ClearInterval{itv});
}

void Macro::insertInInterval(rapidjson::Value&& json, const IntervalModel& itv, ExpandMode mode)
{
  m.submit(new InsertContentInInterval{std::move(json), itv, mode});
}

void Macro::resizeInterval(const IntervalModel& itv, const TimeVal& dur)
{
  auto& resizers = m.stack().context().app.interfaces<IntervalResizerList>();
  auto cmd = resizers.make(itv, dur);
  if (cmd)
    m.submit(cmd);
}

void Macro::setIntervalMin(const IntervalModel& itv, const TimeVal& dur, bool noMin)
{
  m.submit(new SetMinDuration{itv, dur, noMin});
}

void Macro::setIntervalMax(const IntervalModel& itv, const TimeVal& dur, bool inf)
{
  m.submit(new SetMaxDuration{itv, dur, inf});
}

void Macro::submit(score::Command* cmd)
{
  m.submit(cmd);
}

void Macro::commit()
{
  m.commit();
}
}
}
