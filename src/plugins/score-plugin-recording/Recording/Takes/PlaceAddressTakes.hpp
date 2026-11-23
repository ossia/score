#pragma once
#include <State/Address.hpp>
#include <State/Unit.hpp>

#include <Recording/Engine/ValueRecorder.hpp>
#include <Recording/Takes/RecordedCurves.hpp>

#include <score_plugin_recording_export.h>

#include <vector>

namespace Scenario
{
class IntervalModel;
class ProcessModel;
namespace Command
{
class Macro;
}
}

namespace Recording
{
//! What was recorded of an address, dated in the scenario it is placed in
struct AddressTake
{
  State::Address address;
  State::Unit unit;
  std::vector<ValueTake::Event> events;
  int64_t startDate{};
  int64_t endDate{};
};

enum class AddressPlacement
{
  //! An automation for every number of the addresses; the values an
  //! automation cannot play (strings, impulses...) as messages
  Automations,
  //! A scenario whose states send the values, each at its date
  Messages
};

/**
 * Places takes of addresses in a new box of `scenario`, from the first take's
 * start to the last one's end, at height `y`: the "Record automations / messages
 * from here" entries. Everything goes through `macro`; nothing when no take
 * has a value.
 */
SCORE_PLUGIN_RECORDING_EXPORT
Scenario::IntervalModel* placeAddressTakes(
    Scenario::Command::Macro& macro, const Scenario::ProcessModel& scenario, double y,
    const std::vector<AddressTake>& takes, AddressPlacement placement,
    CurveOptions curves);
}
