#include "PlaceAddressTakes.hpp"

#include <State/Message.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/InitAutomation.hpp>
#include <Recording/Takes/RecordedValues.hpp>

#include <ossia/network/dataspace/dataspace.hpp>

#include <algorithm>
#include <limits>
#include <map>

namespace Recording
{
namespace
{
void placeAutomations(
    Scenario::Command::Macro& macro, const Scenario::IntervalModel& box,
    const AddressTake& take, int components, int64_t start, int64_t end,
    CurveOptions curves)
{
  for(int c = 0; c < components; c++)
  {
    auto curve = curveOf(take.events, c, start, end, curves);
    if(!curve)
      continue;
    auto* proc = macro.createProcessInNewSlot(
        box, Metadata<ConcreteKey_k, Automation::ProcessModel>::get(), {});
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
  }
}

void placeMessages(
    Scenario::Command::Macro& macro, const Scenario::IntervalModel& box,
    const std::vector<const AddressTake*>& takes, int64_t start)
{
  // The values by date, in the order they came
  std::map<int64_t, State::MessageList> by_date;
  for(const auto* t : takes)
    for(const auto& e : t->events)
      by_date[std::max<int64_t>(0, e.date - start)].push_back(
          State::Message{State::AddressAccessor{t->address}, e.value});
  if(by_date.empty())
    return;

  auto* proc = macro.createProcessInNewSlot(
      box, Metadata<ConcreteKey_k, Scenario::ProcessModel>::get(), {});
  if(!proc)
    return;
  auto& sub = *safe_cast<Scenario::ProcessModel*>(proc);
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

Scenario::IntervalModel* placeAddressTakes(
    Scenario::Command::Macro& macro, const Scenario::ProcessModel& scenario, double y,
    const std::vector<AddressTake>& takes, AddressPlacement placement,
    CurveOptions curves)
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
  if(end <= start)
    return nullptr;

  auto& box = macro.createBox(scenario, TimeVal{start}, TimeVal{end}, y);

  std::vector<const AddressTake*> as_messages;
  for(const auto& t : takes)
  {
    if(t.events.empty())
      continue;
    const int components
        = placement == AddressPlacement::Automations ? numericComponents(t.events) : 0;
    if(components > 0)
      placeAutomations(macro, box, t, components, start, end, curves);
    else
      as_messages.push_back(&t);
  }
  placeMessages(macro, box, as_messages, start);
  return &box;
}
}
