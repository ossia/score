#include "MidiRecorder.hpp"

#include <cmath>
#include <ossia/dataflow/midi_port.hpp>

#include <Process/Execution/TapLink.hpp>

#include <algorithm>

namespace Recording
{
namespace
{
}
MidiTake::MidiTake(std::size_t capacity)
    : queue(std::max<std::size_t>(16, capacity))
{
}

MidiTake::~MidiTake() = default;

midi_record_node::midi_record_node(
    std::shared_ptr<MidiTake> take, std::shared_ptr<ossia::graph_node> clock)
    : m_take{std::move(take)}
    , m_clock{std::move(clock)}
{
  m_inlets.push_back(new ossia::midi_inlet);
}

midi_record_node::~midi_record_node() = default;

void midi_record_node::run(
    const ossia::token_request&, ossia::exec_state_facade e) noexcept
{
  auto& take = *m_take;
  int st = take.state.load(std::memory_order_acquire);

  if(take.stopRequested.exchange(false))
  {
    if(st != MidiTake::Done)
    {
      take.startRequested.store(false);
      take.state.store(MidiTake::Done, std::memory_order_release);
    }
    return;
  }
  if(take.startRequested.exchange(false) && st == MidiTake::Idle)
  {
    m_gate.reset();
    st = MidiTake::Waiting;
    take.state.store(st, std::memory_order_release);
  }
  if(st != MidiTake::Waiting && st != MidiTake::Recording)
    return;
  if(!m_clock || m_clock->requested_tokens.empty())
  {
    // Its box stopped (its end, a trigger): the take ends where it was
    if(st == MidiTake::Recording && ++m_idleTicks >= 2)
    {
      take.state.store(MidiTake::Done, std::memory_order_release);
    }
    return;
  }
  m_idleTicks = 0;

  const auto& messages = m_inlets.front()->target<ossia::midi_port>()->messages;
  const double ratio = e.modelToSamples();

  for(const auto& tk : m_clock->requested_tokens)
  {
    if(tk.date.impl <= tk.prev_date.impl)
      continue;
    // A seek, or the box going back round: the take ends where it was
    if(st == MidiTake::Recording
       && std::abs(tk.prev_date.impl - take.endDate.load(std::memory_order_relaxed))
              > seek_tolerance)
    {
      take.state.store(MidiTake::Done, std::memory_order_release);
      return;
    }
    const int64_t span_start = tk.physical_start(ratio);
    const int64_t span_len = tk.physical_write_duration(ratio);
    if(span_len <= 0)
      continue;

    // Where a sample of the span is in the box, and musically
    const auto date_at = [&](int64_t in_span) {
      return tk.prev_date.impl
             + int64_t(double(in_span) / span_len * double((tk.date - tk.prev_date).impl));
    };
    const auto position_at = [&](int64_t in_span) {
      return tk.musical_start_position
             + double(in_span) / span_len
                   * (tk.musical_end_position - tk.musical_start_position);
    };

    int64_t from = 0;
    if(st == MidiTake::Waiting)
    {
      if(take.startOnFirstEvent.load(std::memory_order_relaxed))
      {
        // The first message of this span starts the take
        int64_t first = span_len;
        for(const auto& m : messages)
        {
          const int64_t at = int64_t(m.timestamp) - span_start;
          if(at >= 0 && at < span_len)
            first = std::min(first, at);
        }
        if(first == span_len)
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
      st = MidiTake::Recording;
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

    for(const auto& m : messages)
    {
      const int64_t at = int64_t(m.timestamp) - span_start;
      if(at < from || at >= until)
        continue;
      if(take.queue.try_enqueue(MidiTake::Event{date_at(at), m}))
        take.events.fetch_add(1, std::memory_order_relaxed);
      else
        take.droppedEvents.fetch_add(1, std::memory_order_relaxed);
    }

    if(ends)
    {
      take.endDate.store(end_date, std::memory_order_relaxed);
      take.state.store(MidiTake::Done, std::memory_order_release);
      return;
    }
    // The tempo is how fast the box goes through its music, its speed
    // included: a take has the one it started at
    if(std::abs(tk.tempo - take.tempo.load(std::memory_order_relaxed)) > 1e-6)
      take.tempoChanged.store(true, std::memory_order_relaxed);
    take.endDate.store(tk.date.impl, std::memory_order_relaxed);
  }
}
}
