#pragma once
#include <Recording/Engine/StartGate.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/detail/lockfree_queue.hpp>

#include <libremidi/ump.hpp>

#include <score_plugin_recording_export.h>

#include <atomic>
#include <cstdint>
#include <memory>

namespace Recording
{
/**
 * What a MIDI recorder and the session that owns it share, as AudioTake:
 * the audio thread pushes events into a preallocated queue the session
 * drains; a start or a stop is asked through atomics.
 */
struct SCORE_PLUGIN_RECORDING_EXPORT MidiTake
{
  enum State : int
  {
    Idle,
    Waiting,
    Recording,
    Done
  };

  struct Event
  {
    //! Date in the box, in flicks
    int64_t date{};
    libremidi::ump message;
  };

  explicit MidiTake(std::size_t capacity);
  ~MidiTake();

  // GUI thread -> audio thread
  std::atomic<double> startRate{0.};
  //! Counted in: the bars that go by before the line it starts on
  std::atomic<double> leadBars{0.};
  std::atomic<int64_t> length{0};
  //! Bars after which the take stops on its own, counted in the musical time
  //! of the box from where it starts (exact through tempo changes); 0: none.
  //! Takes precedence over `length`.
  std::atomic<double> lengthBars{0.};
  std::atomic<bool> startRequested{false};
  std::atomic<bool> stopRequested{false};
  //! Wait for the first message rather than start at once
  std::atomic<bool> startOnFirstEvent{false};

  // Audio thread -> GUI thread
  std::atomic<int> state{Idle};
  std::atomic<int64_t> startDate{0};
  std::atomic<double> startPosition{0.};
  std::atomic<int64_t> endDate{0};
  std::atomic<double> tempo{0.};
  //! The tempo changed while it recorded: a Sound take has one tempo
  std::atomic<bool> tempoChanged{false};
  //! It recorded at a speed other than 1
  std::atomic<bool> offSpeed{false};
  //! The meter when the take started
  std::atomic<int> meterNumerator{4};
  std::atomic<int> meterDenominator{4};
  std::atomic<int64_t> events{0};
  std::atomic<int64_t> droppedEvents{0};

  ossia::spsc_queue<Event> queue;
};

/**
 * Records the MIDI messages reaching its inlet, each with its date in the
 * box (see audio_record_node for the clock and the start / stop rules).
 */
class SCORE_PLUGIN_RECORDING_EXPORT midi_record_node final : public ossia::graph_node
{
public:
  midi_record_node(std::shared_ptr<MidiTake> take, std::shared_ptr<ossia::graph_node> clock);
  ~midi_record_node() override;

  std::string label() const noexcept override { return "MIDI recorder"; }
  void run(const ossia::token_request&, ossia::exec_state_facade e) noexcept override;

private:
  std::shared_ptr<MidiTake> m_take;
  std::shared_ptr<ossia::graph_node> m_clock;
  double m_endPosition{-1.};
  StartGate m_gate;
  //! The speed when the take started: the tempo ticks give follows it
  double m_startSpeed{1.};
  //! Ticks its box did not play, while it recorded
  int m_idleTicks{};
};
}
