#include "AudioRecorder.hpp"

#include <cmath>
#include <ossia/dataflow/audio_port.hpp>

#include <Process/Execution/TapLink.hpp>

#include <algorithm>

namespace Recording
{
namespace
{
}
AudioTake::AudioTake(int channels, int blockFrames, int blockCount)
    : channels{std::max(1, channels)}
    , blockFrames{std::max(1, blockFrames)}
    , freeBlocks(std::size_t(std::max(1, blockCount)))
    , filledBlocks(std::size_t(std::max(1, blockCount)))
{
  m_pool.reserve(std::max(1, blockCount));
  for(int i = 0; i < std::max(1, blockCount); i++)
  {
    auto b = std::make_unique<Block>();
    b->data.resize(std::size_t(this->channels) * std::size_t(this->blockFrames));
    b->channels = this->channels;
    b->capacity = this->blockFrames;
    freeBlocks.try_enqueue(b.get());
    m_pool.push_back(std::move(b));
  }
}

AudioTake::~AudioTake() = default;

audio_record_node::audio_record_node(
    std::shared_ptr<AudioTake> take, std::shared_ptr<ossia::graph_node> clock)
    : m_take{std::move(take)}
    , m_clock{std::move(clock)}
{
  m_inlets.push_back(new ossia::audio_inlet);
}

audio_record_node::~audio_record_node() = default;

void audio_record_node::record(
    const ossia::audio_port& in, int64_t from, int64_t count) noexcept
{
  auto& take = *m_take;
  const int in_channels = int(in.channels());
  int channels = take.activeChannels.load(std::memory_order_relaxed);
  if(channels == 0)
  {
    // As many as fit in a block with a frame at least
    const int limit = std::min(AudioTake::maxChannels, take.channels * take.blockFrames);
    channels = std::clamp(in_channels, 1, limit);
    take.activeChannels.store(channels, std::memory_order_relaxed);
    take.lostChannels.store(std::max(0, in_channels - limit), std::memory_order_relaxed);
  }
  // The samples of a block, shared between the channels
  const int64_t capacity = int64_t(take.channels) * take.blockFrames / channels;
  while(count > 0)
  {
    if(!m_current)
    {
      if(!take.freeBlocks.try_dequeue(m_current))
      {
        // The writer is behind: what does not fit is lost, and counted.
        take.droppedFrames.fetch_add(count, std::memory_order_relaxed);
        return;
      }
      m_current->frames = 0;
      m_current->channels = channels;
      m_current->capacity = capacity;
    }

    auto& b = *m_current;
    const int64_t n = std::min<int64_t>(count, b.capacity - b.frames);
    for(int c = 0; c < channels; c++)
    {
      float* dst = b.data.data() + std::size_t(c) * std::size_t(b.capacity) + b.frames;
      // What the source has of [from; from + n), silence around it
      const int64_t size = c < in_channels ? int64_t(in.channel(c).size()) : 0;
      const int64_t lo = std::clamp<int64_t>(-from, 0, n);
      const int64_t hi = std::clamp<int64_t>(size - from, lo, n);
      std::fill_n(dst, lo, 0.f);
      if(hi > lo)
      {
        const double* src = in.channel(c).data() + from + lo;
        for(int64_t k = 0; k < hi - lo; k++)
          dst[lo + k] = float(src[k]);
      }
      std::fill_n(dst + hi, n - hi, 0.f);
    }
    b.frames += n;
    from += n;
    count -= n;
    take.frames.fetch_add(n, std::memory_order_relaxed);

    if(b.frames == b.capacity)
    {
      take.filledBlocks.try_enqueue(m_current);
      m_current = nullptr;
    }
  }
}

void audio_record_node::flush() noexcept
{
  if(m_current)
  {
    if(m_current->frames > 0)
      m_take->filledBlocks.try_enqueue(m_current);
    else
      m_take->freeBlocks.try_enqueue(m_current);
    m_current = nullptr;
  }
}

void audio_record_node::run(
    const ossia::token_request&, ossia::exec_state_facade e) noexcept
{
  auto& take = *m_take;
  int st = take.state.load(std::memory_order_acquire);

  if(take.stopRequested.exchange(false))
  {
    // Also before it was ever started: a session stopped at once ends
    if(st != AudioTake::Done)
    {
      flush();
      take.startRequested.store(false);
      take.state.store(AudioTake::Done, std::memory_order_release);
    }
    return;
  }
  if(take.startRequested.exchange(false) && st == AudioTake::Idle)
  {
    m_gate.reset();
    st = AudioTake::Waiting;
    take.state.store(st, std::memory_order_release);
  }
  if(st != AudioTake::Waiting && st != AudioTake::Recording)
    return;

  // The box does not play: nothing of it to record
  if(!m_clock || m_clock->requested_tokens.empty())
  {
    // Its box stopped (its end, a trigger): the take ends where it was
    if(st == AudioTake::Recording && ++m_idleTicks >= 2)
    {
      flush();
      take.state.store(AudioTake::Done, std::memory_order_release);
    }
    return;
  }
  m_idleTicks = 0;

  const auto& in = *m_inlets.front()->target<ossia::audio_port>();
  const double ratio = e.modelToSamples();

  for(const auto& tk : m_clock->requested_tokens)
  {
    // Only forward time is recorded
    if(tk.date.impl <= tk.prev_date.impl)
      continue;
    // A seek, or the box going back round: the take ends where it was
    if(st == AudioTake::Recording
       && std::abs(tk.prev_date.impl - take.endDate.load(std::memory_order_relaxed))
              > seek_tolerance)
    {
      flush();
      take.state.store(AudioTake::Done, std::memory_order_release);
      return;
    }
    const int64_t span_start = tk.physical_start(ratio);
    const int64_t span_len = tk.physical_write_duration(ratio);
    if(span_len <= 0)
      continue;

    int64_t from = 0;
    if(st == AudioTake::Waiting)
    {
      const double rate = take.startRate.load(std::memory_order_relaxed);
      if(rate > 0.)
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
      st = AudioTake::Recording;
      take.state.store(st, std::memory_order_release);
    }

    int64_t count = span_len - from;
    bool ends = false;
    int64_t end_date = 0;
    const int64_t length = take.length.load(std::memory_order_relaxed);
    if(m_endPosition >= 0.)
    {
      if(tk.musical_end_position >= m_endPosition)
      {
        const int64_t end_in_span = tk.musical_end_position == m_endPosition
                                        ? span_len
                                        : tk.physical_position(m_endPosition, ratio);
        count = std::clamp<int64_t>(end_in_span - from, 0, count);
        end_date = tk.prev_date.impl
                   + int64_t(double(end_in_span) / span_len
                             * double((tk.date - tk.prev_date).impl));
        ends = true;
      }
    }
    else if(length > 0)
    {
      const int64_t length_end = take.startDate.load(std::memory_order_relaxed) + length;
      if(tk.date.impl >= length_end)
      {
        const int64_t end_in_span
            = tk.to_physical_time_in_tick(ossia::time_value{length_end}, ratio) - span_start;
        count = std::clamp<int64_t>(end_in_span - from, 0, count);
        end_date = length_end;
        ends = true;
      }
    }

    record(in, span_start + from, count);

    if(ends)
    {
      take.endDate.store(end_date, std::memory_order_relaxed);
      flush();
      take.state.store(AudioTake::Done, std::memory_order_release);
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
