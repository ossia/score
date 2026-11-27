#pragma once
#include <State/Address.hpp>
#include <State/Unit.hpp>

#include <QPointF>
#include <QSizeF>
#include <QString>

#include <optional>

#include <Recording/Engine/ValueRecorder.hpp>
#include <Recording/Takes/RecordedCurves.hpp>

#include <score_plugin_recording_export.h>

#include <vector>

namespace Scenario
{
class StateModel;
class IntervalModel;
class ProcessModel;
namespace Command
{
class Macro;
}
}

namespace Recording
{
//! Room a take's node takes in the node view
constexpr QSizeF takeNodeSize{320., 160.};

//! The first place from `wanted` down where a take's node covers no node of
//! the box
SCORE_PLUGIN_RECORDING_EXPORT
QPointF freeNodePosition(const Scenario::IntervalModel& box, QPointF wanted);

//! Where a take's node goes when no place was asked: in a column from the
//! top left of the box's nodes, under them, one take under the other
SCORE_PLUGIN_RECORDING_EXPORT
QPointF takeNodePosition(
    const Scenario::IntervalModel& box, std::optional<QPointF> asked = {});

//! What was recorded of an address, dated in the scenario it is placed in
struct AddressTake
{
  State::Address address;
  State::Unit unit;
  std::vector<ValueTake::Event> events;
  int64_t startDate{};
  int64_t endDate{};
};

//! A take written to a file (an audio or a texture input), dated in the
//! scenario it is placed in
struct FileTake
{
  QString file;
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
    CurveOptions curves, const std::vector<FileTake>& files = {});

/**
 * Places takes of addresses in a new box after `state`, from its date to the
 * end of the last take: the box follows the state when nothing does yet, else
 * starts from a new state of its event. The takes are dated in the
 * scenario's time.
 */
SCORE_PLUGIN_RECORDING_EXPORT
Scenario::IntervalModel* placeAddressTakesAfter(
    Scenario::Command::Macro& macro, const Scenario::ProcessModel& scenario,
    const Scenario::StateModel& state, const std::vector<AddressTake>& takes,
    AddressPlacement placement, CurveOptions curves,
    const std::vector<FileTake>& files = {});

/**
 * The same in a box that is there, whose time the takes are dated in: their
 * processes start where the takes started (a negative start offset), and play
 * the takes' last values up to the end of the box. False when no take has a
 * value.
 */
SCORE_PLUGIN_RECORDING_EXPORT
bool placeAddressTakesInBox(
    Scenario::Command::Macro& macro, const Scenario::IntervalModel& box,
    const std::vector<AddressTake>& takes, AddressPlacement placement,
    CurveOptions curves, const std::vector<FileTake>& files = {},
    std::optional<QPointF> nodePosition = {});
}
