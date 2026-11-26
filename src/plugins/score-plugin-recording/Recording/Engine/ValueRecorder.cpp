#include "ValueRecorder.hpp"

#include <cmath>
#include <Process/Execution/TapLink.hpp>

#include <ossia/dataflow/value_port.hpp>

#include <algorithm>

namespace Recording
{
namespace
{
//! Dates further apart than this between two ticks are a jump (flicks: 2 ms)
constexpr int64_t seek_tolerance = 1411200;
}
ValueTake::ValueTake(std::size_t capacity)
    : queue(std::max<std::size_t>(16, capacity))
{
}

ValueTake::~ValueTake() = default;

value_record_node::value_record_node(
    std::shared_ptr<ValueTake> take, std::shared_ptr<ossia::graph_node> clock)
    : m_take{std::move(take)}
    , m_clock{std::move(clock)}
{
  // From an address, only the values it receives, not the one it holds
  auto* in = new ossia::value_inlet;
  (*in)->is_event = true;
  m_inlets.push_back(in);
}

value_record_node::~value_record_node() = default;

void value_record_node::run(
    const ossia::token_request&, ossia::exec_state_facade e) noexcept
{
  auto& take = *m_take;
  int st = take.state.load(std::memory_order_acquire);

  if(take.stopRequested.exchange(false))
  {
    if(st != ValueTake::Done)
    {
      take.startRequested.store(false);
      take.state.store(ValueTake::Done, std::memory_order_release);
    }
    return;
  }
  if(take.startRequested.exchange(false) && st == ValueTake::Idle)
  {
    m_gate.reset();
    st = ValueTake::Waiting;
    take.state.store(st, std::memory_order_release);
  }
  if(st != ValueTake::Waiting && st != ValueTake::Recording)
    return;
  if(!m_clock || m_clock->requested_tokens.empty())
  {
    // Its box stopped (its end, a trigger): the take ends where it was
    if(st == ValueTake::Recording && ++m_idleTicks >= 2)
    {
      take.state.store(ValueTake::Done, std::memory_order_release);
    }
    return;
  }
  m_idleTicks = 0;

  const auto& values = m_inlets.front()->target<ossia::value_port>()->get_data();
  const double ratio = e.modelToSamples();

  for(const auto& tk : m_clock->requested_tokens)
  {
    if(tk.date.impl <= tk.prev_date.impl)
      continue;
    // A seek, or the box going back round: the take ends where it was
    if(st == ValueTake::Recording
       && std::abs(tk.prev_date.impl - take.endDate.load(std::memory_order_relaxed))
              > seek_tolerance)
    {
      take.state.store(ValueTake::Done, std::memory_order_release);
      return;
    }
    const int64_t span_start = tk.physical_start(ratio);
    const int64_t span_len = tk.physical_write_duration(ratio);
    if(span_len <= 0)
      continue;

    const auto date_at = [&](int64_t in_span) {
      return tk.prev_date.impl
             + int64_t(double(in_span) / span_len * double((tk.date - tk.prev_date).impl));
    };
    const auto position_at = [&](int64_t in_span) {
      return tk.musical_start_position
             + double(in_span) / span_len
                   * (tk.musical_end_position - tk.musical_start_position);
    };
    // Where a value is in the span, or outside of [0; span_len) when it is
    // in another token's. One dated before the tick's first span is at its
    // start, one after its last at its end.
    const bool first_span = &tk == &m_clock->requested_tokens.front();
    const bool last_span = &tk == &m_clock->requested_tokens.back();
    const auto at_of = [&](const ossia::timed_value& v) -> int64_t {
      const int64_t at = int64_t(v.timestamp) - span_start;
      if(at < 0)
        return first_span ? 0 : -1;
      if(at >= span_len)
        return last_span ? span_len - 1 : span_len;
      return at;
    };

    int64_t from = 0;
    if(st == ValueTake::Waiting)
    {
      if(take.startOnFirstEvent.load(std::memory_order_relaxed))
      {
        int64_t first = span_len;
        for(const auto& v : values)
          if(const int64_t at = at_of(v); at >= 0)
            first = std::min(first, at);
        // None in this span
        if(first >= span_len)
          continue;
        from = first;
        take.startDate.store(date_at(from), std::memory_order_relaxed);
        take.startPosition.store(position_at(from), std::memory_order_relaxed);
      }
      else if(const double rate = take.startRate.load(std::memory_order_relaxed); rate > 0.)
      {
        m_gate.waitFrom(tk);
        const auto pt = tk.get_quantification_point(rate);
        if(!pt
           || !m_gate.accepts(
               pt->position, take.leadBars.load(std::memory_order_relaxed), tk.signature))
          continue;
        from = tk.physical_position(pt->position, ratio);
        take.startDate.store(pt->date.impl, std::memory_order_relaxed);
        take.startPosition.store(pt->position, std::memory_order_relaxed);
      }
      else
      {
        take.startDate.store(tk.prev_date.impl, std::memory_order_relaxed);
        take.startPosition.store(tk.musical_start_position, std::memory_order_relaxed);
      }
      take.tempo.store(tk.tempo, std::memory_order_relaxed);
      m_startSpeed = tk.speed;
      take.meterNumerator.store(tk.signature.upper, std::memory_order_relaxed);
      take.meterDenominator.store(tk.signature.lower, std::memory_order_relaxed);
      m_endPosition = -1.;
      if(const double bars = take.lengthBars.load(std::memory_order_relaxed); bars > 0.)
      {
        const auto sig = tk.signature;
        const double quarters_per_bar
            = (sig.upper > 0 && sig.lower > 0) ? 4. * sig.upper / sig.lower : 4.;
        m_endPosition
            = take.startPosition.load(std::memory_order_relaxed) + bars * quarters_per_bar;
      }
      st = ValueTake::Recording;
      take.state.store(st, std::memory_order_release);
    }

    int64_t until = span_len;
    bool ends = false;
    int64_t end_date = 0;
    const int64_t length = take.length.load(std::memory_order_relaxed);
    const int64_t start_date = take.startDate.load(std::memory_order_relaxed);
    if(m_endPosition >= 0.)
    {
      if(tk.musical_end_position >= m_endPosition)
      {
        until = tk.musical_end_position == m_endPosition
                    ? span_len
                    : tk.physical_position(m_endPosition, ratio);
        end_date = date_at(until);
        ends = true;
      }
    }
    else if(length > 0 && tk.date.impl >= start_date + length)
    {
      until = std::clamp<int64_t>(
          tk.to_physical_time_in_tick(ossia::time_value{start_date + length}, ratio)
              - span_start,
          0, span_len);
      end_date = start_date + length;
      ends = true;
    }

    for(const auto& v : values)
    {
      const int64_t at = at_of(v);
      if(at < from || at >= until)
        continue;
      if(take.queue.try_enqueue(ValueTake::Event{date_at(at), v.value}))
      {
        take.lastEventDate.store(date_at(at), std::memory_order_relaxed);
        take.events.fetch_add(1, std::memory_order_relaxed);
      }
      else
        take.droppedEvents.fetch_add(1, std::memory_order_relaxed);
    }

    if(ends)
    {
      take.endDate.store(end_date, std::memory_order_relaxed);
      take.state.store(ValueTake::Done, std::memory_order_release);
      return;
    }
    // The tempo of the box, whatever the speed it plays at
    if(tk.speed > 1e-9 && m_startSpeed > 1e-9
       && std::abs(
              tk.tempo / tk.speed
              - take.tempo.load(std::memory_order_relaxed) / m_startSpeed)
              > 1e-6)
      take.tempoChanged.store(true, std::memory_order_relaxed);
    if(std::abs(tk.speed - 1.) > 1e-6)
      take.offSpeed.store(true, std::memory_order_relaxed);
    take.endDate.store(tk.date.impl, std::memory_order_relaxed);
  }
}
}
