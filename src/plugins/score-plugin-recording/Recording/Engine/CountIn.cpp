#include "CountIn.hpp"

#include <ossia/dataflow/value_port.hpp>

namespace Recording
{
count_in_node::count_in_node(
    int rate, std::shared_ptr<ossia::graph_node> clock, std::function<bool()> waiting)
    : metro{rate}
    , m_clock{std::move(clock)}
    , m_waiting{std::move(waiting)}
{
  metro.sound = Media::Metro::Click;
  metro.accent = true;
  m_outlets.push_back(&metro.audio_out);
}

count_in_node::~count_in_node() = default;

void count_in_node::run(const ossia::token_request&, ossia::exec_state_facade e) noexcept
{
  if(!m_clock || !m_waiting || !m_waiting())
    return;
  auto& bangs = metro.bang_out.target<ossia::value_port>()->get_data();
  for(const auto& tk : m_clock->requested_tokens)
  {
    ossia::graph_node& m = metro;
    m.run(tk, e);
  }
  // Its bangs go nowhere: counted, then cleared, as the graph would
  beats.fetch_add(int(bangs.size()), std::memory_order_relaxed);
  bangs.clear();
}
}
