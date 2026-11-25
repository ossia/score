#pragma once
#include <Recording/Session/RecordOptions.hpp>

#include <State/Address.hpp>

#include <QPointF>

#include <functional>
#include <optional>
#include <vector>

class QMenu;
namespace score
{
struct DocumentContext;
}
namespace Scenario
{
class IntervalModel;
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
 * The entries recording addresses of the explorer into `box`: the timing
 * entries, values as automations (MIDI devices as notes, audio and video
 * inputs as files), and the same with values as messages.
 */
void addAddressEntries(
    QMenu& sub, QMenu& root, ApplicationPlugin& plug, Scenario::IntervalModel& box,
    std::vector<State::Address> addresses, std::optional<QPointF> pos,
    const score::DocumentContext& ctx);
}
