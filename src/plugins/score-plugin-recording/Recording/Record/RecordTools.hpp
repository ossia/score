#pragma once

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Process/TimeValue.hpp>
#include <Recording/Commands/Record.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveNewEvent.hpp>
#include <State/Address.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/tools/std/HashMap.hpp>

#include <chrono>
#include <memory>
#include <vector>

namespace RedoStrategy
{
struct Quiet;
}
namespace Explorer
{
class DeviceExplorerModel;
}
namespace Scenario
{
class ProcessModel;
class IntervalModel;
class EventModel;
struct Point;
}

namespace std
{
// MOVEME
template <>
struct hash<Device::FullAddressSettings>
{
  std::size_t operator()(const Device::FullAddressSettings& k) const
  {
    return std::hash<State::Address>{}(k.address);
  }
};
}

namespace Recording
{
struct RecordContext;
/** @typedef RecordListening
 *
 * The set of nodes that should be recorded.
 * Sorted by devices as an optimization.
 * All the susb-vectors are assumed to be non-empty.
 */
using RecordListening = std::vector<std::vector<Device::Node*>>;
using RecordCommandDispatcher = GenericMacroCommandDispatcher<
    Recording::Record,
    RedoStrategy::Quiet,
    SendStrategy::UndoRedo>;

struct Box
{
  Scenario::IntervalModel& interval;
  // In the first slot.

  Scenario::Command::MoveNewEvent& moveCommand;
  Id<Scenario::EventModel> endEvent;
};
/*
// Only the selected addresses
RecordListening GetAddressesToRecord(
        Explorer::DeviceExplorerModel& m_explorer);
*/
// The selected addresses and all their children
RecordListening GetAddressesToRecordRecursive(Explorer::DeviceExplorerModel& explorer);

Box CreateBox(RecordContext&);

inline double GetTimeDifferenceInDouble(std::chrono::steady_clock::time_point start)
{
  using namespace std::chrono;
  return duration_cast<microseconds>(steady_clock::now() - start).count() / 1000.;
}
inline TimeVal GetTimeDifference(std::chrono::steady_clock::time_point start)
{
  using namespace std::chrono;
  return TimeVal::fromMsecs(GetTimeDifferenceInDouble(start));
}

/**
 * @brief ReasonableUpdateInterval an interval for graphical updating of
 * recording box to prevent lag
 * @return update interval in milliseconds
 */
constexpr int ReasonableUpdateInterval(int numberOfCurves)
{
  return numberOfCurves < 10
             ? 8
             : numberOfCurves < 50
                   ? 16
                   : numberOfCurves < 100 ? 100 : numberOfCurves < 1000 ? 1000 : 5000;
}
}
