#pragma once
#include <Recording/Engine/StartGate.hpp>
#include <Recording/Engine/Take.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/detail/lockfree_queue.hpp>
#include <ossia/network/value/value.hpp>

#include <score_plugin_recording_export.h>

#include <atomic>
#include <cstdint>
#include <memory>

namespace Recording
{
/**
 * What a value recorder and its session share, as MidiTake: the values are
 * pushed with their date in the box into a queue the session drains.
 */
struct SCORE_PLUGIN_RECORDING_EXPORT ValueTake : TakeState
{
  struct Event
  {
    //! Date in the box, in flicks
    int64_t date{};
    ossia::value value;
  };

  explicit ValueTake(std::size_t capacity);
  ~ValueTake();

  // GUI thread -> audio thread
  std::atomic<bool> startOnFirstEvent{false};

  // Audio thread -> GUI thread
  std::atomic<int64_t> events{0};
  //! The date in the box of the last value recorded
  std::atomic<int64_t> lastEventDate{0};
  std::atomic<int64_t> droppedEvents{0};

  ossia::spsc_queue<Event> queue;
};

/**
 * Records the values reaching its inlet, each with its date in the box (see
 * audio_record_node for the clock and the start / stop rules). A value
 * without a timestamp is dated at the start of the tick it came in.
 */
class SCORE_PLUGIN_RECORDING_EXPORT value_record_node final : public ossia::graph_node
{
public:
  value_record_node(std::shared_ptr<ValueTake> take, std::shared_ptr<ossia::graph_node> clock);
  ~value_record_node() override;

  std::string label() const noexcept override { return "value recorder"; }
  void run(const ossia::token_request&, ossia::exec_state_facade e) noexcept override;

private:
  std::shared_ptr<ValueTake> m_take;
  std::shared_ptr<ossia::graph_node> m_clock;
  double m_endPosition{-1.};
  StartGate m_gate;
  //! Ticks its box did not play, while it recorded
  int m_idleTicks{};
};
}
