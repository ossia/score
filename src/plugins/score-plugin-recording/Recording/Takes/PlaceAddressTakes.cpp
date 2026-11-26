#include "PlaceAddressTakes.hpp"

#include <State/Message.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Process/Commands/Properties.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/command/Dispatchers/RuntimeDispatcher.hpp>

#include <QMimeData>
#include <QUrl>

#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/InitAutomation.hpp>
#include <Recording/Takes/RecordedValues.hpp>

#include <ossia/network/dataspace/dataspace.hpp>

#include <algorithm>
#include <limits>
#include <map>

namespace Recording
{
QPointF takeNodePosition(const Scenario::IntervalModel& box, std::optional<QPointF> asked)
{
  if(asked)
    return freeNodePosition(box, *asked);
  QPointF anchor{};
  bool first = true;
  for(const auto& p : box.processes)
  {
    const auto pos = p.position();
    anchor = first ? pos : QPointF{std::min(anchor.x(), pos.x()), std::min(anchor.y(), pos.y())};
    first = false;
  }
  return freeNodePosition(box, anchor);
}

QPointF freeNodePosition(const Scenario::IntervalModel& box, QPointF wanted)
{
  // As any process created in the node view: below what is in the way
  return Scenario::freeProcessPositionBelow(box, wanted, takeNodeSize);
}
namespace
{
//! Where the next process goes in the node view: from where the menu opened,
//! clear of the others, or where new processes go
struct Placer
{
  std::optional<QPointF> pos;
  Process::ProcessModel* operator()(
      Scenario::Command::Macro& macro, const Scenario::IntervalModel& box,
      const Process::ProcessData& data)
  {
    return macro.createProcessInNewSlot(box, data, takeNodePosition(box, pos));
  }
};

void placeAutomations(
    Scenario::Command::Macro& macro, const Scenario::IntervalModel& box,
    const AddressTake& take, int components, int64_t start, int64_t end,
    int64_t offset, CurveOptions curves, Placer& place)
{
  for(int c = 0; c < components; c++)
  {
    auto curve = curveOf(take.events, c, start, end, curves);
    if(!curve)
      continue;
    auto* proc = place(
        macro, box,
        Process::ProcessData{Metadata<ConcreteKey_k, Automation::ProcessModel>::get(), {}, {}});
    if(!proc)
      continue;
    auto& autom = *safe_cast<Automation::ProcessModel*>(proc);
    // A vector or a list: an automation for each of its numbers
    State::AddressAccessor addr = components == 1
                                      ? State::AddressAccessor{take.address, {}, take.unit}
                                      : State::AddressAccessor{
                                            take.address, {uint8_t(c)}, take.unit};
    macro.submit(new Automation::InitAutomation{
        autom, std::move(addr), curve->min, curve->max, std::move(curve->segments)});
    if(offset != 0)
      macro.submit(new Process::SetStartOffset{autom, TimeVal{offset}});
  }
}

void placeMessages(
    Scenario::Command::Macro& macro, const Scenario::IntervalModel& box,
    const std::vector<const AddressTake*>& takes, int64_t start, int64_t offset,
    Placer& place)
{
  // The values by date, in the order they came
  std::map<int64_t, State::MessageList> by_date;
  for(const auto* t : takes)
    for(const auto& e : t->events)
      by_date[std::max<int64_t>(0, e.date - start)].push_back(
          State::Message{State::AddressAccessor{t->address}, e.value});
  if(by_date.empty())
    return;

  auto* proc = place(
      macro, box,
      Process::ProcessData{Metadata<ConcreteKey_k, Scenario::ProcessModel>::get(), {}, {}});
  if(!proc)
    return;
  auto& sub = *safe_cast<Scenario::ProcessModel*>(proc);
  if(offset != 0)
    macro.submit(new Process::SetStartOffset{sub, TimeVal{offset}});
  const auto& first_states = sub.startEvent().states();
  if(first_states.empty())
    return;

  Id<Scenario::StateModel> state = first_states.front();
  for(auto& [date, messages] : by_date)
  {
    if(date > 0)
    {
      auto cmd = new Scenario::Command::CreateInterval_State_Event_TimeSync{
          sub, state, TimeVal{date}, 0.5, false};
      macro.submit(cmd);
      state = cmd->createdState();
    }
    macro.submit(new Scenario::Command::AddMessagesToState{sub.state(state), messages});
  }
}
}

namespace
{
//! The span of the takes: [start, end)
std::pair<int64_t, int64_t>
spanOf(const std::vector<AddressTake>& takes, const std::vector<FileTake>& files)
{
  int64_t start = std::numeric_limits<int64_t>::max();
  int64_t end = std::numeric_limits<int64_t>::lowest();
  for(const auto& t : takes)
  {
    if(t.events.empty())
      continue;
    start = std::min(start, t.startDate);
    end = std::max(end, t.endDate);
  }
  for(const auto& f : files)
  {
    start = std::min(start, f.startDate);
    end = std::max(end, f.endDate);
  }
  return {start, end};
}

/**
 * The processes of the takes in `box`: their content starts at the box's
 * date `origin` and spans `span`; `offset` is their start offset, 0 in a box
 * made for them, -origin in a box that was there.
 */
void placeInto(
    Scenario::Command::Macro& macro, const Scenario::IntervalModel& box,
    const std::vector<AddressTake>& takes, AddressPlacement placement,
    CurveOptions curves, const std::vector<FileTake>& files, int64_t origin,
    int64_t span, int64_t offset, std::optional<QPointF> nodePosition)
{
  Placer place{nodePosition};
  std::vector<const AddressTake*> as_messages;
  for(const auto& t : takes)
  {
    if(t.events.empty())
      continue;
    const int components
        = placement == AddressPlacement::Automations ? numericComponents(t.events) : 0;
    if(components > 0)
      placeAutomations(
          macro, box, t, components, origin, origin + span, offset, curves, place);
    else
      as_messages.push_back(&t);
  }
  placeMessages(macro, box, as_messages, origin, offset, place);

  // The files as their drop makes them, each from where it was recorded
  if(!files.empty())
  {
    const auto& handlers = score::AppContext().interfaces<Process::ProcessDropHandlerList>();
    score::Dispatcher_T disp{macro};
    for(const auto& f : files)
    {
      QMimeData mime;
      mime.setUrls({QUrl::fromLocalFile(f.file)});
      for(auto& d : handlers.getDrop(mime, box.context()))
      {
        auto* p = place(macro, box, d.creation);
        if(!p)
          continue;
        if(d.setup)
          d.setup(*p, disp);
        macro.submit(new Process::SetLoop{*p, false});
        macro.submit(
            new Process::SetStartOffset{*p, TimeVal{offset + origin - f.startDate}});
      }
    }
  }
}
}

Scenario::IntervalModel* placeAddressTakes(
    Scenario::Command::Macro& macro, const Scenario::ProcessModel& scenario, double y,
    const std::vector<AddressTake>& takes, AddressPlacement placement,
    CurveOptions curves, const std::vector<FileTake>& files)
{
  const auto [start, end] = spanOf(takes, files);
  if(end <= start)
    return nullptr;
  auto& box = macro.createBox(scenario, TimeVal{start}, TimeVal{end}, y);
  placeInto(macro, box, takes, placement, curves, files, start, end - start, 0, {});
  return &box;
}

bool placeAddressTakesInBox(
    Scenario::Command::Macro& macro, const Scenario::IntervalModel& box,
    const std::vector<AddressTake>& takes, AddressPlacement placement,
    CurveOptions curves, const std::vector<FileTake>& files,
    std::optional<QPointF> nodePosition)
{
  const auto [start, end] = spanOf(takes, files);
  if(end <= start)
    return false;
  const int64_t span = std::max(end - start, box.duration.defaultDuration().impl);
  placeInto(macro, box, takes, placement, curves, files, start, span, -start, nodePosition);
  return true;
}
}
