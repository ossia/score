// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CreateCurves.hpp"

#include <Automation/AutomationModel.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Process/State/MessageNode.hpp>
#include <Scenario/Commands/Cohesion/CreateCurveFromStates.hpp>
#include <Scenario/Commands/Cohesion/InterpolateMacro.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <State/ValueConversion.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/IdentifiedObjectAbstract.hpp>
#include <score/selection/Selectable.hpp>
#include <score/selection/SelectionStack.hpp>

#include <ossia/editor/state/destination_qualifiers.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QList>
namespace Scenario
{
static std::vector<Device::FullAddressSettings>
getSelectedAddresses(const score::DocumentContext& doc)
{
  // First get the addresses
  auto& device_explorer = Explorer::deviceExplorerFromContext(doc);

  std::vector<Device::FullAddressSettings> addresses;
  for (const auto& index : device_explorer.selectedIndexes())
  {
    const auto& node = device_explorer.nodeFromModelIndex(index);
    if (node.is<Device::AddressSettings>())
    {
      const Device::AddressSettings& addr = node.get<Device::AddressSettings>();
      if (ossia::is_numeric(addr.value) || ossia::is_array(addr.value))
      {
        Device::FullAddressSettings as;
        static_cast<Device::AddressSettingsCommon&>(as) = addr;
        as.address = Device::address(node).address;
        addresses.push_back(std::move(as));
      }
    }
  }
  return addresses;
}

void CreateCurves(
    const std::vector<const Scenario::IntervalModel*>& selected_intervals,
    const score::CommandStackFacade& stack)
{
  if (selected_intervals.empty())
    return;

  // For each interval, interpolate between the states in its start event and
  // end event.
  auto& doc = score::IDocument::documentContext(*selected_intervals.front());

  auto addresses = getSelectedAddresses(doc);
  if (addresses.empty())
    return;

  CreateCurvesFromAddresses(selected_intervals, addresses, stack);
}

// First the case for a single address accessor

struct CategorizedAddress
{
  explicit CategorizedAddress(const Device::FullAddressAccessorSettings& addr)
  {
    if (addr.value.v)
      ossia::apply_nonnull([&](const auto& v) { (*this)(v, addr); }, addr.value.v);
  }

  void operator()(ossia::impulse, const Device::FullAddressAccessorSettings& addr) { count += 1; }
  void operator()(bool, const Device::FullAddressAccessorSettings& addr) { count += 1; }
  void operator()(int, const Device::FullAddressAccessorSettings& addr) { count += 1; }
  void operator()(float, const Device::FullAddressAccessorSettings& addr) { count += 1; }
  void operator()(char, const Device::FullAddressAccessorSettings& addr) { count += 1; }
  void operator()(const std::string&, const Device::FullAddressAccessorSettings& addr)
  {
    count += 1;
  }
  void operator()(const ossia::vec2f&, const Device::FullAddressAccessorSettings& addr)
  {
    if (addr.address.qualifiers.get().accessors.empty())
      count += 2;
    else
      count += 1;
  }
  void operator()(const ossia::vec3f&, const Device::FullAddressAccessorSettings& addr)
  {
    if (addr.address.qualifiers.get().accessors.empty())
      count += 3;
    else
      count += 1;
  }
  void operator()(const ossia::vec4f&, const Device::FullAddressAccessorSettings& addr)
  {
    if (addr.address.qualifiers.get().accessors.empty())
      count += 4;
    else
      count += 1;
  }
  void
  operator()(const std::vector<ossia::value>& v, const Device::FullAddressAccessorSettings& addr)
  {
    if (addr.address.qualifiers.get().accessors.empty())
      count += v.size();
    else
      count += 1; // TODO not really precise... we should go check what the
                  // number of automations should really be created
  }

  int count = 0;
  int automCount() { return count; }
};

struct AddressAccessorCurveCreator
{
  const Device::FullAddressAccessorSettings& as;
  const Scenario::StateModel& ss;
  const Scenario::StateModel& es;
  const Scenario::IntervalModel& interval;
  const std::vector<Id<Process::ProcessModel>>& process_ids;
  Scenario::Command::Macro& macro;
  const std::vector<SlotPath>& slotsToUse;
  std::vector<Process::ProcessModel*>& created;

  void operator()(ossia::impulse) { }

  void operator()(bool b) { }

  void operator()(int v)
  {
    const State::AddressAccessor& addr = as.address;
    // Then we set-up all the necessary values
    // min / max
    Curve::CurveDomain dom{as.domain.get(), as.value};
    bool tween = false;

    // start value / end value
    Process::MessageNode* s_node
        = Device::try_getNodeFromString(ss.messages().rootNode(), stringList(addr.address));
    if (s_node)
    {
      if (auto val = s_node->value())
      {
        dom.start = State::convert::value<int>(*val);
        dom.min = std::min(dom.start, dom.min);
        dom.max = std::max(dom.start, dom.max);
      }
    }
    else
    {
      tween = true;
    }

    Process::MessageNode* e_node
        = Device::try_getNodeFromString(es.messages().rootNode(), stringList(addr.address));
    if (e_node)
    {
      if (auto val = e_node->value())
      {
        dom.end = State::convert::value<int>(*val);
        dom.min = std::min(dom.end, dom.min);
        dom.max = std::max(dom.end, dom.max);
      }
    }

    // Send the command.
    auto& p = macro.automate(interval, slotsToUse, process_ids[created.size()], addr, dom, tween);
    created.push_back(&p);
  }

  void operator()(float v)
  {
    const State::AddressAccessor& addr = as.address;
    // Then we set-up all the necessary values
    // min / max
    Curve::CurveDomain dom{as.domain.get(), as.value};
    bool tween = false;

    // start value / end value
    Process::MessageNode* s_node
        = Device::try_getNodeFromString(ss.messages().rootNode(), stringList(addr.address));
    if (s_node)
    {
      if (auto val = s_node->value())
      {
        dom.start = State::convert::value<double>(*val);
        dom.min = std::min(dom.start, dom.min);
        dom.max = std::max(dom.start, dom.max);
      }
    }
    else
    {
      tween = true;
    }

    Process::MessageNode* e_node
        = Device::try_getNodeFromString(es.messages().rootNode(), stringList(addr.address));
    if (e_node)
    {
      if (auto val = e_node->value())
      {
        dom.end = State::convert::value<double>(*val);
        dom.min = std::min(dom.end, dom.min);
        dom.max = std::max(dom.end, dom.max);
      }
    }

    // Send the command.
    auto& p = macro.automate(interval, slotsToUse, process_ids[created.size()], addr, dom, tween);
    created.push_back(&p);
  }

  void operator()(char v) { }

  void operator()(const std::string& v) { }

  template <std::size_t N>
  void operator()(const std::array<float, N>& v)
  {
    State::AddressAccessor addr = as.address;
    // Then we set-up all the necessary values
    // min / max
    Curve::CurveDomain dom{as.domain.get(), as.value};
    bool tween = false;

    auto& acc = addr.qualifiers.get().accessors;
    switch (acc.size())
    {
      case 0:
      {
        acc.resize(1);
        for (std::size_t c = 0; c < N; c++)
        {
          acc[0] = (int)c;
          // Send the command.
          auto& p = macro.automate(
              interval, slotsToUse, process_ids[created.size()], addr, dom, tween);
          created.push_back(&p);
        }
        break;
      }
      case 1:
      default:
      {
        auto& p
            = macro.automate(interval, slotsToUse, process_ids[created.size()], addr, dom, tween);
        created.push_back(&p);
        break;
      }
    }
  }

  void operator()(const std::vector<ossia::value>& v)
  {
    State::AddressAccessor addr = as.address;
    // Then we set-up all the necessary values
    // min / max
    Curve::CurveDomain dom{as.domain.get(), as.value};
    bool tween = false;

    auto& acc = addr.qualifiers.get().accessors;
    if (acc.empty())
    {
      acc.resize(1);
      // TODO do a proper recursive algorithm here ; also change the count
      // algorithm before to match
      for (std::size_t c = 0; c < v.size(); c++)
      {
        const auto t = v[c].get_type();
        if (t == ossia::val_type::FLOAT || t == ossia::val_type::INT)
        {
          acc[0] = (int)c;
          // Send the command.
          auto& p = macro.automate(
              interval, slotsToUse, process_ids[created.size()], addr, dom, tween);
          created.push_back(&p);
        }
      }
    }
    else
    {
      auto& p
          = macro.automate(interval, slotsToUse, process_ids[created.size()], addr, dom, tween);
      created.push_back(&p);
    }
  }

  void operator()() { }
};

std::vector<Process::ProcessModel*> CreateCurvesFromAddress(
    const Scenario::IntervalModel& interval,
    const Device::FullAddressAccessorSettings& as,
    Scenario::Command::Macro& m)
{
  std::vector<Process::ProcessModel*> created;
  auto scenar = dynamic_cast<Scenario::ScenarioInterface*>(interval.parent());

  CategorizedAddress addresses{as};
  const auto N = addresses.automCount();

  // Generate brand new ids for the processes
  auto process_ids = getStrongIdRange<Process::ProcessModel>(N, interval.processes);

  if (interval.smallView().empty())
  {
    m.createSlot(interval);
  }

  // Put everything in first slot
  std::vector<SlotPath> slotsToUse{{interval, 0}};

  const auto& ss = startState(interval, *scenar);
  const auto& es = endState(interval, *scenar);

  // TODO check existing automations
  as.value.apply(
      AddressAccessorCurveCreator{as, ss, es, interval, process_ids, m, slotsToUse, created});

  if (created.size() > 0)
  {
    m.commit();
  }

  return created;
}

// Then the case for a lot of addresses (e.g. when there is a drop)
// Todo: the day we drop states is the day where this has to change towards a
// FullAddressAccessorSettings

struct CategorizedAddresses
{
  explicit CategorizedAddresses(const std::vector<Device::FullAddressSettings>& addrs)
  {
    for (auto& addr : addrs)
    {
      if (addr.value.v)
        ossia::apply_nonnull([&](const auto& v) { (*this)(v, addr); }, addr.value.v);
    }
  }

  void operator()(ossia::impulse, const Device::FullAddressSettings& addr)
  {
    impulse_addr.push_back(addr);
  }
  void operator()(bool, const Device::FullAddressSettings& addr) { bool_addr.push_back(addr); }
  void operator()(int, const Device::FullAddressSettings& addr) { int_addr.push_back(addr); }
  void operator()(float, const Device::FullAddressSettings& addr) { float_addr.push_back(addr); }
  void operator()(char, const Device::FullAddressSettings& addr) { char_addr.push_back(addr); }
  void operator()(const std::string&, const Device::FullAddressSettings& addr)
  {
    string_addr.push_back(addr);
  }
  void operator()(const ossia::vec2f&, const Device::FullAddressSettings& addr)
  {
    vec2f_addr.push_back(addr);
  }
  void operator()(const ossia::vec3f&, const Device::FullAddressSettings& addr)
  {
    vec3f_addr.push_back(addr);
  }
  void operator()(const ossia::vec4f&, const Device::FullAddressSettings& addr)
  {
    vec4f_addr.push_back(addr);
  }
  void operator()(const std::vector<ossia::value>& v, const Device::FullAddressSettings& addr)
  {
    if (!v.empty())
    {
      list_addr.push_back(addr);
    }
  }

  int automCount()
  {
    int c = 0;
    // c += impulse_addr.size();
    c += int_addr.size();
    // c += bool_addr.size();
    c += float_addr.size();
    // c += string_addr.size();
    c += char_addr.size();
    c += 2 * vec2f_addr.size();
    c += 3 * vec3f_addr.size();
    c += 4 * vec4f_addr.size();

    for (auto& addr : list_addr)
    {
      auto& v = addr.value.get<std::vector<ossia::value>>();
      c += v.size();
      // TODO only keep numeric indices ?
    }
    return c;
  }

  std::vector<Device::FullAddressSettings> impulse_addr;
  std::vector<Device::FullAddressSettings> int_addr;
  std::vector<Device::FullAddressSettings> bool_addr;
  std::vector<Device::FullAddressSettings> float_addr;
  std::vector<Device::FullAddressSettings> string_addr;
  std::vector<Device::FullAddressSettings> char_addr;
  std::vector<Device::FullAddressSettings> vec2f_addr;
  std::vector<Device::FullAddressSettings> vec3f_addr;
  std::vector<Device::FullAddressSettings> vec4f_addr;
  std::vector<Device::FullAddressSettings> list_addr;
};

struct CurveCreator
{
  const Device::FullAddressSettings& as;
  const Scenario::StateModel& ss;
  const Scenario::StateModel& es;
  const Scenario::IntervalModel& interval;
  const std::vector<Id<Process::ProcessModel>>& process_ids;
  Scenario::Command::Macro& macro;
  const std::vector<SlotPath>& slotsToUse;
  std::vector<Process::ProcessModel*>& created;

  void operator()(ossia::impulse) { }

  void operator()(bool b) { }

  void operator()(int v)
  {
    State::AddressAccessor addr{as.address, {}, as.unit};
    // Then we set-up all the necessary values
    // min / max
    Curve::CurveDomain dom{as.domain.get(), as.value};
    bool tween = false;

    // start value / end value
    Process::MessageNode* s_node
        = Device::try_getNodeFromString(ss.messages().rootNode(), stringList(as.address));
    if (s_node)
    {
      if (auto val = s_node->value())
      {
        dom.start = State::convert::value<int>(*val);
        dom.min = std::min(dom.start, dom.min);
        dom.max = std::max(dom.start, dom.max);
      }
    }
    else
    {
      tween = true;
    }

    Process::MessageNode* e_node
        = Device::try_getNodeFromString(es.messages().rootNode(), stringList(as.address));
    if (e_node)
    {
      if (auto val = e_node->value())
      {
        dom.end = State::convert::value<int>(*val);
        dom.min = std::min(dom.end, dom.min);
        dom.max = std::max(dom.end, dom.max);
      }
    }

    // Send the command.
    auto& p = macro.automate(interval, slotsToUse, process_ids[created.size()], addr, dom, tween);
    created.push_back(&p);
  }

  void operator()(float v)
  {
    State::AddressAccessor addr{as.address, {}, as.unit};
    // Then we set-up all the necessary values
    // min / max
    Curve::CurveDomain dom{as.domain.get(), as.value};
    bool tween = false;

    // start value / end value
    Process::MessageNode* s_node
        = Device::try_getNodeFromString(ss.messages().rootNode(), stringList(as.address));
    if (s_node)
    {
      if (auto val = s_node->value())
      {
        dom.start = State::convert::value<double>(*val);
        dom.min = std::min(dom.start, dom.min);
        dom.max = std::max(dom.start, dom.max);
      }
    }
    else
    {
      tween = true;
    }

    Process::MessageNode* e_node
        = Device::try_getNodeFromString(es.messages().rootNode(), stringList(as.address));
    if (e_node)
    {
      if (auto val = e_node->value())
      {
        dom.end = State::convert::value<double>(*val);
        dom.min = std::min(dom.end, dom.min);
        dom.max = std::max(dom.end, dom.max);
      }
    }

    // Send the command.
    auto& p = macro.automate(interval, slotsToUse, process_ids[created.size()], addr, dom, tween);
    created.push_back(&p);
  }

  void operator()(char v) { }

  void operator()(const std::string& v) { }

  template <std::size_t N>
  void operator()(const std::array<float, N>& v)
  {
    State::AddressAccessor addr{as.address, {}, as.unit};
    // Then we set-up all the necessary values
    // min / max
    Curve::CurveDomain dom{as.domain.get(), as.value};
    bool tween = false;

    auto& acc = addr.qualifiers.get().accessors;
    acc.resize(1);
    for (std::size_t c = 0; c < N; c++)
    {
      acc[0] = (int)c;
      // Send the command.
      auto& p
          = macro.automate(interval, slotsToUse, process_ids[created.size()], addr, dom, tween);
      created.push_back(&p);
    }
  }

  void operator()(const std::vector<ossia::value>& v)
  {
    State::AddressAccessor addr{as.address, {}, as.unit};
    // Then we set-up all the necessary values
    // min / max
    Curve::CurveDomain dom{as.domain.get(), as.value};
    bool tween = false;

    auto& acc = addr.qualifiers.get().accessors;
    acc.resize(1);
    // TODO do a proper recursive algorithm here ; also change the count
    // algorithm before to match
    for (std::size_t c = 0; c < v.size(); c++)
    {
      const auto t = v[c].get_type();
      if (t == ossia::val_type::FLOAT || t == ossia::val_type::INT)
      {
        acc[0] = (int)c;
        // Send the command.
        auto& p
            = macro.automate(interval, slotsToUse, process_ids[created.size()], addr, dom, tween);
        created.push_back(&p);
      }
    }
  }

  void operator()() { }
};

int CreateCurvesFromAddresses(
    const Scenario::IntervalModel& interval,
    const Scenario::ScenarioInterface& scenar,
    const CategorizedAddresses& addresses,
    int N,
    Scenario::Command::Macro& m,
    std::vector<Process::ProcessModel*>& created)
{
  // Generate brand new ids for the processes
  auto process_ids = getStrongIdRange<Process::ProcessModel>(N, interval.processes);
  auto macro = Scenario::Command::makeAddProcessMacro(interval, N);

  auto slots = macro->slotsToUse;
  m.submit(macro);
  const auto& ss = startState(interval, scenar);
  const auto& es = endState(interval, scenar);

  // TODO not only for standard automations
  /*
  std::vector<State::AddressAccessor> existing_automations;
  for (const auto& proc : interval.processes)
  {
    if (auto autom = dynamic_cast<const Automation::ProcessModel*>(&proc))
      existing_automations.push_back(autom->address());
  }
  */

  for (const Device::FullAddressSettings& as : addresses.float_addr)
  {
    CurveCreator{as, ss, es, interval, process_ids, m, slots, created}(float{});
  }
  for (const Device::FullAddressSettings& as : addresses.int_addr)
  {
    CurveCreator{as, ss, es, interval, process_ids, m, slots, created}(int{});
  }
  for (const Device::FullAddressSettings& as : addresses.vec2f_addr)
  {
    CurveCreator{as, ss, es, interval, process_ids, m, slots, created}(ossia::vec2f{});
  }
  for (const Device::FullAddressSettings& as : addresses.vec3f_addr)
  {
    CurveCreator{as, ss, es, interval, process_ids, m, slots, created}(ossia::vec3f{});
  }
  for (const Device::FullAddressSettings& as : addresses.vec4f_addr)
  {
    CurveCreator{as, ss, es, interval, process_ids, m, slots, created}(ossia::vec4f{});
  }
  for (const Device::FullAddressSettings& as : addresses.list_addr)
  {
    CurveCreator{as, ss, es, interval, process_ids, m, slots, created}(
        as.value.get<std::vector<ossia::value>>());
  }
  return created.size();
}

std::vector<Process::ProcessModel*> CreateCurvesFromAddresses(
    const Scenario::IntervalModel& interval,
    const std::vector<Device::FullAddressSettings>& a,
    Scenario::Command::Macro& m)
{
  std::vector<Process::ProcessModel*> created;
  auto scenar = dynamic_cast<Scenario::ScenarioInterface*>(interval.parent());

  CategorizedAddresses addresses{a};
  const auto N = addresses.automCount();

  int count = CreateCurvesFromAddresses(interval, *scenar, addresses, N, m, created);
  if (count > 0)
  {
    m.commit();
  }

  return created;
}

std::vector<Process::ProcessModel*> CreateCurvesFromAddresses(
    const std::vector<const Scenario::IntervalModel*>& selected_intervals,
    const std::vector<Device::FullAddressSettings>& a,
    Scenario::Command::Macro& m)
{
  std::vector<Process::ProcessModel*> created;

  if (selected_intervals.empty())
    return created;

  // They should all be in the same scenario so we can select the first.
  // FIXME check that the other "cohesion" methods also use ScenarioInterface
  // and not Scenario::ProcessModel
  auto scenar = dynamic_cast<Scenario::ScenarioInterface*>(selected_intervals.front()->parent());

  bool added_processes = false;
  CategorizedAddresses addresses{a};
  const auto N = addresses.automCount();

  for (const auto& interval_ptr : selected_intervals)
  {
    int count = CreateCurvesFromAddresses(*interval_ptr, *scenar, addresses, N, m, created);
    if (count > 0)
      added_processes = true;
  }

  if (added_processes)
  {
    m.commit();
  }

  return created;
}

void CreateCurvesFromAddresses(
    const std::vector<const Scenario::IntervalModel*>& selected_intervals,
    const std::vector<Device::FullAddressSettings>& a,
    const score::CommandStackFacade& stack)
{
  Scenario::Command::Macro big_macro{
      new Scenario::Command::AddMultipleProcessesToMultipleIntervalsMacro, stack.context()};

  CreateCurvesFromAddresses(selected_intervals, a, big_macro);
}
}
