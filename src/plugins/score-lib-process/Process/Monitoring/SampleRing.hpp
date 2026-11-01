#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>

namespace Process
{
/**
 * The most recent samples of a multichannel signal, written by the execution
 * thread and read by the GUI without lock or allocation on the writing side.
 *
 * The writer never waits: it overwrites the oldest samples. It announces the
 * end of the block it is about to write before writing it, and publishes it
 * once written; the reader copies the span it wants and checks afterwards
 * that no block announced meanwhile reached it, as in a seqlock. A copy the
 * writer may have overwritten is reported as unavailable rather than
 * returned torn.
 *
 * Only what was written as one continuous stream is returned: the frames of
 * a channel from before its buffer existed or from before the writer skipped
 * it, and the frames from before a pause of the writer (see restart()), are
 * reported as unavailable.
 *
 * Channel buffers are allocated by the reader (provision()) when the writer
 * reports more channels than are allocated; until then the writer skips the
 * extra channels. Buffers are freed only with the ring.
 */
class SampleRing
{
public:
  static constexpr int max_channels = 64;

  explicit SampleRing(int capacity = 1 << 16)
      : m_capacity{std::bit_ceil((uint64_t)std::max(capacity, 1024))}
      , m_mask{m_capacity - 1}
  {
  }

  SampleRing(const SampleRing&) = delete;
  SampleRing& operator=(const SampleRing&) = delete;

  ~SampleRing()
  {
    for(auto& b : m_buffers)
      delete[] b.load(std::memory_order_relaxed);
  }

  uint64_t capacity() const noexcept { return m_capacity; }

  //! Writer: appends `frames` frames; `channel(c)` gives channel c's samples.
  //! When the previous block is older than a few blocks, the writer was
  //! paused (it writes only while a view watches) and restart() is implied.
  template <typename F>
  void write(int channel_count, int frames, F&& channel) noexcept
  {
    if(frames <= 0)
      return;
    channel_count = std::clamp(channel_count, 0, max_channels);
    m_channels.store(channel_count, std::memory_order_relaxed);

    const uint64_t w = m_written.load(std::memory_order_relaxed);
    const auto now = clock::now();
    const double rate = m_rate.load(std::memory_order_relaxed);
    const auto block = std::chrono::duration<double>(frames / std::max(rate, 1.));
    if(w > 0 && now - m_last_write > std::max(4 * block, std::chrono::duration<double>(0.05)))
      m_from.store(w, std::memory_order_relaxed);
    m_last_write = now;

    // Announce the block before touching the data
    m_writing.store(w + frames, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);

    for(int c = 0; c < channel_count; c++)
    {
      float* buf = m_buffers[c].load(std::memory_order_acquire);
      if(!buf)
        continue;
      // First block in a newly provisioned buffer, or after the channel was
      // skipped: what is before it is not this channel's signal
      if(m_since[c].load(std::memory_order_relaxed) == unwritten)
        m_since[c].store(w, std::memory_order_relaxed);
      const auto* src = channel(c);
      for(int i = 0; i < frames; i++)
        std::atomic_ref<float>{buf[(w + i) & m_mask]}.store(
            float(src[i]), std::memory_order_relaxed);
    }
    for(int c = channel_count; c < m_written_channels; c++)
      m_since[c].store(unwritten, std::memory_order_relaxed);
    m_written_channels = channel_count;

    m_written.store(w + frames, std::memory_order_release);
  }

  //! Writer: what was written so far is not continuous with what comes next.
  void restart() noexcept
  {
    m_from.store(m_written.load(std::memory_order_relaxed), std::memory_order_relaxed);
  }

  void set_rate(double rate) noexcept { m_rate.store(rate, std::memory_order_relaxed); }

  //! Reader: allocates the buffers of the channels the writer reported.
  void provision()
  {
    const int n = m_channels.load(std::memory_order_relaxed);
    for(int c = 0; c < n; c++)
    {
      if(!m_buffers[c].load(std::memory_order_relaxed))
      {
        m_since[c].store(unwritten, std::memory_order_relaxed);
        m_buffers[c].store(new float[m_capacity]{}, std::memory_order_release);
      }
    }
  }

  int channels() const noexcept { return m_channels.load(std::memory_order_relaxed); }
  double rate() const noexcept { return m_rate.load(std::memory_order_relaxed); }

  //! Absolute index one past the last frame written so far.
  uint64_t written() const noexcept { return m_written.load(std::memory_order_acquire); }

  //! Copies the frames [start, start + dst.size()) of channel c into dst.
  //! False when they are not written yet, already overwritten, not part of
  //! the current stream of the channel, or the channel has no buffer yet.
  bool read(int c, uint64_t start, std::span<float> dst) const noexcept
  {
    if(c < 0 || c >= max_channels || dst.size() > m_capacity / 2)
      return false;
    float* buf = m_buffers[c].load(std::memory_order_acquire);
    if(!buf)
      return false;

    const uint64_t end = start + dst.size();
    if(end > m_written.load(std::memory_order_acquire))
      return false;
    if(start < m_from.load(std::memory_order_relaxed)
       || start < m_since[c].load(std::memory_order_relaxed))
      return false;
    if(!untouched(start))
      return false;

    for(std::size_t i = 0; i < dst.size(); i++)
      dst[i] = std::atomic_ref<float>{buf[(start + i) & m_mask]}.load(
          std::memory_order_relaxed);

    // Pairs with the writer's fence: a block that overwrote part of the copy
    // was announced before, so it is seen here.
    std::atomic_thread_fence(std::memory_order_acquire);
    return untouched(start);
  }

  //! Copies the latest dst.size() frames of channel c; returns the absolute
  //! index of the first one, or nothing.
  bool read_latest(int c, std::span<float> dst, uint64_t& first) const noexcept
  {
    const uint64_t w = written();
    if(w < dst.size())
      return false;
    first = w - dst.size();
    return read(c, first, dst);
  }

private:
  using clock = std::chrono::steady_clock;
  static constexpr uint64_t unwritten = std::numeric_limits<uint64_t>::max();

  //! No block announced so far overwrites the frames from `start` on.
  bool untouched(uint64_t start) const noexcept
  {
    return m_writing.load(std::memory_order_relaxed) <= start + m_capacity;
  }

  const uint64_t m_capacity;
  const uint64_t m_mask;
  std::array<std::atomic<float*>, max_channels> m_buffers{};
  //! Per channel: first frame of its current stream
  std::array<std::atomic<uint64_t>, max_channels> m_since{};
  std::atomic<uint64_t> m_written{0};
  std::atomic<uint64_t> m_writing{0};
  //! First frame of the current stream of the writer
  std::atomic<uint64_t> m_from{0};
  std::atomic<int> m_channels{0};
  std::atomic<double> m_rate{48000.};

  // Writer only
  clock::time_point m_last_write{};
  int m_written_channels{};
};
}
