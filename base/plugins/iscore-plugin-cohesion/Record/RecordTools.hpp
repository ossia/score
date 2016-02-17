#pragma once
#include <Explorer/DocumentPlugin/ListeningState.hpp>
#include <State/Address.hpp>
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
namespace Scenario {
class ScenarioModel;
class ConstraintModel;
class EventModel;
class RackModel;
class SlotModel;
struct Point;
namespace Command
{
class MoveNewEvent;
}
}

namespace std
{
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

Box CreateBox(
        Scenario::ScenarioModel& scenar,
        Scenario::Point pt,
        RecordCommandDispatcher& dispatcher);
}
