#pragma once

#include <Scenario/Commands/Scenario/Displacement/MoveNewEvent.hpp>
#include <State/Address.hpp>
#include <Process/TimeValue.hpp>
#include <Device/Address/AddressSettings.hpp>

#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>

#include <QObject>
#include <QTimer>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <vector>

namespace RedoStrategy {
struct Quiet;
}
namespace Explorer
{
class DeviceExplorerModel;
}
namespace Scenario {
class ProcessModel;
class ConstraintModel;
class EventModel;
class RackModel;
class SlotModel;
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
using RecordListening = std::vector<std::vector<Device::FullAddressSettings> >;
using RecordCommandDispatcher = GenericMacroCommandDispatcher<
RedoStrategy::Quiet,
SendStrategy::UndoRedo>;

struct Box
{
        Scenario::ConstraintModel& constraint;
        Scenario::RackModel& rack;
        Scenario::SlotModel& slot;

        Scenario::Command::MoveNewEvent& moveCommand;
        Id<Scenario::EventModel> endEvent;
};

// Only the selected addresses
RecordListening GetAddressesToRecord(
        Explorer::DeviceExplorerModel& m_explorer);

// The selected addresses and all their children
RecordListening GetAddressesToRecordRecursive(
        Explorer::DeviceExplorerModel& explorer);

Box CreateBox(RecordContext&);

inline double GetTimeDifferenceInDouble(std::chrono::steady_clock::time_point start)
{
    using namespace std::chrono;
    return duration_cast<microseconds>(
               steady_clock::now() - start).count() / 1000.;
}
inline TimeValue GetTimeDifference(std::chrono::steady_clock::time_point start)
{
    using namespace std::chrono;
    return TimeValue::fromMsecs(GetTimeDifferenceInDouble(start));
}

/**
 * @brief ReasonableUpdateInterval an interval for graphical updating of recording box to prevent lag
 * @return update interval in milliseconds
 */
constexpr int ReasonableUpdateInterval(int numberOfCurves)
{
    return numberOfCurves < 10
            ? 8
            : numberOfCurves < 50
              ? 16
              : numberOfCurves < 100
                ? 100
                : numberOfCurves < 1000
                  ? 1000
                  : 5000;
}
}
