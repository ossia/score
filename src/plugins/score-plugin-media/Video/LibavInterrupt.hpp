#pragma once
#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV

extern "C" {
#include <libavformat/avformat.h>
}

#include <atomic>
#include <chrono>

namespace Video
{

/**
 * @brief Deadline for the blocking libav I/O of one AVFormatContext.
 *
 * avformat_open_input, avformat_find_stream_info and av_read_frame have no
 * timeout of their own: on a device that stops answering they block until the
 * kernel gives up, which for some USB cameras never happens. The only way out
 * is the interrupt callback, which libav polls from inside those calls.
 *
 * install() must be called on a freshly allocated context, before
 * avformat_open_input: libav frees the context on failure, so a retry has to
 * allocate and install again.
 *
 * Safe to arm from the thread doing the I/O and to abort from another one.
 */
class LibavInterrupt
{
public:
  using clock = std::chrono::steady_clock;

  void install(AVFormatContext& ctx) noexcept
  {
    ctx.interrupt_callback.callback = &LibavInterrupt::on_interrupt;
    ctx.interrupt_callback.opaque = this;
  }

  void arm(std::chrono::milliseconds timeout) noexcept
  {
    m_deadline.store(
        (clock::now() + timeout).time_since_epoch().count(), std::memory_order_release);
  }

  void disarm() noexcept { m_deadline.store(none, std::memory_order_release); }

  //! Unblocks the pending call and every subsequent one until reset()
  void abort() noexcept { m_aborted.store(true, std::memory_order_release); }

  void reset() noexcept
  {
    m_aborted.store(false, std::memory_order_release);
    disarm();
  }

  bool aborted() const noexcept { return m_aborted.load(std::memory_order_acquire); }

  bool expired() const noexcept
  {
    if(aborted())
      return true;

    const auto deadline = m_deadline.load(std::memory_order_acquire);
    return deadline != none && clock::now().time_since_epoch().count() >= deadline;
  }

private:
  static constexpr int64_t none = 0;

  static int on_interrupt(void* opaque) noexcept
  {
    return static_cast<const LibavInterrupt*>(opaque)->expired() ? 1 : 0;
  }

  std::atomic<int64_t> m_deadline{none};
  std::atomic_bool m_aborted{};
};

//! Arms an interrupt for the duration of a scope
struct LibavTimeout
{
  explicit LibavTimeout(LibavInterrupt& itr, std::chrono::milliseconds t) noexcept
      : m_itr{itr}
  {
    m_itr.arm(t);
  }
  ~LibavTimeout() { m_itr.disarm(); }

  LibavTimeout(const LibavTimeout&) = delete;
  LibavTimeout& operator=(const LibavTimeout&) = delete;

private:
  LibavInterrupt& m_itr;
};

}
#endif
