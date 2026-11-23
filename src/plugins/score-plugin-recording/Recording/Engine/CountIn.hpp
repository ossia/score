#pragma once
#include <Media/Metro/MetroNode.hpp>

#include <ossia/dataflow/graph_node.hpp>

#include <score_plugin_recording_export.h>

#include <atomic>
#include <functional>
#include <memory>

namespace Recording
{
/**
 * Clicks the beats of a box while the takes of a recording wait for their
 * start, so that the player hears where the bar is (a count-in).
 *
 * It plays the metronome of the Metronome process on the box's clock, the
 * tokens of the node it is given, as long as `waiting` says so: a function of
 * atomics the session hands it, called on the audio thread. Its audio outlet
 * goes to the main output.
 */
class SCORE_PLUGIN_RECORDING_EXPORT count_in_node final
    : public ossia::nonowning_graph_node
{
public:
  count_in_node(
      int rate, std::shared_ptr<ossia::graph_node> clock,
      std::function<bool()> waiting);
  ~count_in_node() override;

  std::string label() const noexcept override { return "count-in"; }
  void run(const ossia::token_request&, ossia::exec_state_facade e) noexcept override;

  Media::Metro::metronome_node metro;
  //! The beats clicked so far
  std::atomic<int> beats{0};

private:
  std::shared_ptr<ossia::graph_node> m_clock;
  std::function<bool()> m_waiting;
};
}
