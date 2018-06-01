#include <Scenario/Commands/CommandAPI.hpp>
#include <score_plugin_scenario_commands_files.hpp>

namespace Scenario { namespace Command {

Macro::Macro(
    score::AggregateCommand* cmd
    , const score::DocumentContext& ctx):
  m{std::unique_ptr<score::AggregateCommand>(cmd), ctx.commandStack}
{

}

Macro::~Macro()
{
  m.rollback();
}

StateModel&Macro::createState(
    const ProcessModel& scenar
    , const Id<EventModel>& ev
    , double y)
{
  auto cmd = new Scenario::Command::CreateState{scenar, ev, y};
  m.submitCommand(cmd);
  return scenar.states.at(cmd->createdState());
}

std::tuple<TimeSyncModel&, EventModel&, StateModel&>
Macro::createDot(const ProcessModel& scenar, Point pt)
{
  auto cmd = new CreateTimeSync_Event_State{scenar, pt.date, pt.y};
  m.submitCommand(cmd);
  return std::tie(
        scenar.timeSyncs.at(cmd->createdTimeSync())
        , scenar.events.at(cmd->createdEvent())
        , scenar.states.at(cmd->createdState()));
}

IntervalModel&
Macro::createBox(const ProcessModel& scenar
                 , TimeVal start, TimeVal end, double y)
{
  auto c_st = new CreateTimeSync_Event_State{scenar, start, y};
  m.submitCommand(c_st);

  auto c_itv = new CreateInterval_State_Event_TimeSync{
      scenar, c_st->createdState(), end, y};
  m.submitCommand(c_itv);

  return scenar.intervals.at(c_itv->createdInterval());
}

IntervalModel&
Macro::createIntervalAfter(const ProcessModel& scenar, const Id<StateModel>& state, Point pt)
{
  auto cmd = new CreateInterval_State_Event_TimeSync{
      scenar, state, pt.date, pt.y};
  m.submitCommand(cmd);
  return scenar.intervals.at(cmd->createdInterval());
}

IntervalModel& Macro::createInterval(
    const ProcessModel& scenar
    , const Id<StateModel>& start
    , const Id<StateModel>& end)
{
  auto cmd = new CreateInterval{scenar, start, end};
  m.submitCommand(cmd);
  return scenar.intervals.at(cmd->createdInterval());
}

Process::ProcessModel*
Macro::createProcess(const IntervalModel& interval, const UuidKey<Process::ProcessModel>& key, const QString& data)
{
  auto process_cmd = new AddOnlyProcessToInterval{
      interval, key, data};
  m.submitCommand(process_cmd);
  auto it = interval.processes.find(process_cmd->processId());
  if(it != interval.processes.end())
    return &(*it);
  return nullptr;
}

Process::ProcessModel*
Macro::createProcessInSlot(const IntervalModel& interval, const UuidKey<Process::ProcessModel>& key, const QString& data)
{
  auto process_cmd = new AddProcessToInterval{
      interval, key, data};
  m.submitCommand(process_cmd);
  auto it = interval.processes.find(process_cmd->processId());
  if(it != interval.processes.end())
    return &(*it);
  return nullptr;
}

Process::ProcessModel* Macro::loadProcessInSlot(
    const IntervalModel& interval
    , const QJsonObject& procdata)
{
  auto process_cmd = new LoadProcessInInterval{interval, procdata};
  m.submitCommand(process_cmd);
  auto it = interval.processes.find(process_cmd->processId());
  if(it != interval.processes.end())
    return &(*it);
  return nullptr;

}

void Macro::createSlot(const IntervalModel& interval)
{
  auto slot_cmd = new AddSlotToRack{interval};
  m.submitCommand(slot_cmd);
}

void Macro::addLayer(
    const IntervalModel& interval
    , int slot_index
    , const Process::ProcessModel& proc)
{
  addLayer(SlotPath{interval, slot_index}, proc);
}

void Macro::addLayerToLastSlot(
    const IntervalModel& interval
    , const Process::ProcessModel& proc)
{
  addLayer(SlotPath{interval, int(interval.smallView().size() - 1)}, proc);
}

void Macro::addLayerInNewSlot(
    const IntervalModel& interval
    , const Process::ProcessModel& proc)
{
  auto layer_cmd = new AddLayerInNewSlot{interval, proc.id()};
  m.submitCommand(layer_cmd);
}

void Macro::addLayer(
    const SlotPath& slotpath
    , const Process::ProcessModel& proc)
{
  auto layer_cmd = new AddLayerModelToSlot{slotpath, proc};
  m.submitCommand(layer_cmd);
}

void Macro::showRack(const IntervalModel& interval)
{
  auto show_cmd = new ShowRack{interval};
  m.submitCommand(show_cmd);
}

void Macro::resizeSlot(
    const IntervalModel& interval
    , const SlotPath& slotPath
    , double newSize)
{
  auto cmd = new ResizeSlotVertically{interval, slotPath, newSize};
  m.submitCommand(cmd);
}

void Macro::resizeSlot(
    const IntervalModel& interval
    , SlotPath&& slotPath
    , double newSize)
{
  auto cmd = new ResizeSlotVertically{interval, std::move(slotPath), newSize};
  m.submitCommand(cmd);
}

Scenario::IntervalModel& Macro::duplicate(
    const Scenario::ProcessModel& scenario
    , const IntervalModel& itv)
{
  auto cmd = new DuplicateInterval{scenario, itv};
  m.submitCommand(cmd);
  return scenario.intervals.at(cmd->createdId());
}

Process::ProcessModel& Macro::duplicateProcess(
    const IntervalModel& itv
    , const Process::ProcessModel& process)
{
  auto cmd = new DuplicateOnlyProcessToInterval{itv, process};
  m.submitCommand(cmd);
  return itv.processes.at(cmd->processId());
}

void Macro::pasteElements(
    const ProcessModel& scenario
    , const QJsonObject& objs
    , Point pos)
{
  auto paste = new ScenarioPasteElements(scenario, objs, pos);
  m.submitCommand(paste);
}

void Macro::mergeTimeSyncs(
    const ProcessModel& scenario
    , const Id<TimeSyncModel>& a
    , const Id<TimeSyncModel>& b)
{
  auto cmd = new Command::MergeTimeSyncs(scenario, a, b);
  m.submitCommand(cmd);
}

void Macro::removeProcess(
    const IntervalModel& interval
    , const Id<Process::ProcessModel>& proc)
{
  auto rm_cmd = new RemoveProcessFromInterval(interval, proc);
  m.submitCommand(rm_cmd);
}

void Macro::removeElements(
    const Scenario::ProcessModel& scenario
    , const Selection& sel)
{
  auto rm_cmd = new RemoveSelection(scenario, sel);
  m.submitCommand(rm_cmd);
}

void Macro::addMessages(const StateModel& state, State::MessageList msgs)
{
  auto cmd2 = new AddMessagesToState{state, std::move(msgs)};
  m.submitCommand(cmd2);
}

void Macro::clearInterval(const IntervalModel& itv)
{
  auto cmd = new ClearInterval{itv};
  m.submitCommand(cmd);
}

void Macro::insertInInterval(
    QJsonObject&& json
    , const IntervalModel& itv
    , ExpandMode mode)
{
  auto cmd = new InsertContentInInterval{std::move(json), itv, mode};
  m.submitCommand(cmd);
}

void Macro::submit(score::Command* cmd)
{
  m.submitCommand(cmd);
}

void Macro::commit()
{
  m.commit();
}

} }
