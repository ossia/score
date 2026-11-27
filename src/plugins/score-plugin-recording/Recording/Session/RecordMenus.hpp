#pragma once
#include <score/selection/Selection.hpp>
#include <Recording/Session/RecordOptions.hpp>
#include <score_plugin_recording_export.h>

#include <Scenario/Palette/ScenarioPoint.hpp>
#include <State/Address.hpp>

#include <QPointF>

#include <functional>
#include <optional>
#include <vector>

class QMenu;
namespace score
{
struct ApplicationContext;
struct DocumentContext;
}
namespace Scenario
{
class StateModel;
class IntervalModel;
class ProcessModel;
}
namespace Recording
{
class ApplicationPlugin;

//! What the sources of a menu can do
struct RecordMenuSources
{
  //! MIDI or values: they can wait for their first note or value
  bool events{true};
  //! MIDI: "first note" rather than "first value"
  bool notes{false};
  //! Bars: a take of a number of bars (not for video)
  bool bars{true};
  //! Passes of a bar kept as alternatives (with bars; not for the values of
  //! addresses, which make one curve)
  bool passes{true};
  //! "Record only the file": they are recorded into files
  bool files{true};
};

//! Where the takes of addresses go: into a box, or into a new box of a
//! scenario at a point
struct RecordMenuTarget
{
  Scenario::IntervalModel* box{};
  //! A new box after this state of `scenario`
  const Scenario::StateModel* after{};
  Scenario::ProcessModel* scenario{};
  Scenario::Point point{};
  //! In the node view of `box`: where the take's node goes
  std::optional<QPointF> nodePosition;
};

/**
 * When a recording starts and how long it lasts, the same for every place a
 * recording starts from (a port, a cable, the explorer, a node view): now,
 * from the next bar (counting in or not), a number of bars as a loop, passes
 * of one bar, from the first note or value, when playback starts. `record`
 * gets the options of the entry chosen, from the Recording settings.
 * `root` is the menu closed when the custom number of bars is chosen.
 */
void addTimingEntries(
    QMenu& sub, QMenu& root, ApplicationPlugin& plug,
    std::function<void(RecordOptions)> record, RecordMenuSources sources);

/**
 * The Record menu of every background a recording starts from: a scenario, a
 * node view, the device explorer. The same entries everywhere: the selected
 * (or armed) processes, the addresses selected in the explorer, each with the
 * timing entries, and Capture. Stop recording instead while one runs.
 */
/**
 * What a menu opened on the selected element of `scenario` records into: a box
 * selected, that box; a state, after it; an event or a time sync, after its
 * first state. Nothing changes when the selection is not one of them.
 */
SCORE_PLUGIN_RECORDING_EXPORT
void targetOfSelection(
    Scenario::ProcessModel& scenario, const Selection& selection, RecordMenuTarget& target);

/**
 * How the count-in clicks, in a "Count-in" submenu of `menu`: its length in
 * bars, its sound, its volume and the accent of the first beat. They change
 * the Recording settings, and with them every recording counted in.
 */
SCORE_PLUGIN_RECORDING_EXPORT
void addCountInEntries(QMenu& menu, const score::ApplicationContext& ctx);

SCORE_PLUGIN_RECORDING_EXPORT
void addRecordEntries(
    QMenu& menu, ApplicationPlugin& plug, const score::DocumentContext& ctx,
    RecordMenuTarget target);
}
