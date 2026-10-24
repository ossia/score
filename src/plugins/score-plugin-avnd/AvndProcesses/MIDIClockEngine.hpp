#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

/**
 * Time-dependent logic of MIDI Sync Out, free of any clock source and of any
 * output: every function takes the current time as an argument, so that it
 * can be driven by std::chrono::steady_clock in the sending thread and by a
 * fake clock in tests.
 */
namespace mtk::midi_clock
{
//! Clock rate factors, in the order of the "Clock rate" control's values.
inline constexpr double clock_rate_factor(int index) noexcept
{
  constexpr double factors[] = {1., 0.25, 1. / 3., 0.5, 2., 3., 4.};
  return (index >= 0 && index < int(std::size(factors))) ? factors[index] : 1.;
}

//! Tempo factor while a nudge button is held: -1 is down, +1 is up.
inline constexpr double nudge_factor(int nudge) noexcept
{
  return nudge > 0 ? 1.1 : nudge < 0 ? 0.9 : 1.;
}

inline constexpr double min_tempo = 20.;
inline constexpr double max_tempo = 300.;

/**
 * Tap tempo: the average interval of up to 32 consecutive taps.
 * A tap more than 3 s after the previous one starts a new series.
 */
struct tap_tempo
{
  static constexpr int max_taps = 32;
  static constexpr int64_t timeout_ns = 3'000'000'000;

  void tap(int64_t now) noexcept
  {
    if(m_count > 0)
    {
      const int64_t since = now - last();
      if(since > timeout_ns || since <= 0)
        m_count = 0;
    }

    m_head = (m_head + 1) % max_taps;
    m_taps[m_head] = now;
    m_count = std::min(m_count + 1, max_taps);
  }

  void reset() noexcept { m_count = 0; }

  int count() const noexcept { return m_count; }
  int64_t last() const noexcept { return m_taps[m_head]; }

  //! Average interval between the taps of the series, 0 with less than 2 taps.
  double interval_ns() const noexcept
  {
    if(m_count < 2)
      return 0.;
    const int first = (m_head - (m_count - 1) + max_taps) % max_taps;
    return double(m_taps[m_head] - m_taps[first]) / double(m_count - 1);
  }

  //! Tempo of the series in BPM, 0 with less than 2 taps.
  double tempo() const noexcept
  {
    const double iv = interval_ns();
    return iv > 0. ? std::clamp(60e9 / iv, min_tempo, max_tempo) : 0.;
  }

private:
  std::array<int64_t, max_taps> m_taps{};
  int m_head{max_taps - 1};
  int m_count{};
};

struct message
{
  //! Time at which the message is due, in the caller's clock.
  int64_t at{};
  //! Large enough for a full MTC frame.
  std::array<uint8_t, 10> bytes{};
  int size{};
};

enum class transport_command : int
{
  None,
  Start,
  Stop,
  Continue
};

struct free_clock_params
{
  //! The BPM control. A tapped tempo replaces it until it changes.
  double bpm = 120.;
  double rate = 1.;
  int nudge = 0;
  double offset_ms = 0.;
  bool send_clock = true;
  bool send_transport = true;
};

/**
 * A MIDI clock running at its own tempo, with its own transport.
 *
 * Start sends Song Position 0, Start and a first clock; Continue sends the
 * current Song Position, Continue and a first clock; Stop sends Stop and
 * keeps the position for Continue. Clocks are only sent while running.
 *
 * When stopped, four taps (one bar of 4/4) start the clock on the beat after
 * the last tap, at the tapped tempo.
 *
 * A positive offset delays the start and shifts the phase of a running
 * clock; changing it while running moves the next clocks by the difference.
 * There is no timeline to be early against, so negative offsets count as 0.
 *
 * Ticks are dated from the previous one, not from the time process() is
 * called: a late call sends the overdue clocks at once with their due dates,
 * and the clock does not drift.
 */
struct free_clock
{
  static constexpr int count_in_taps = 4;
  static constexpr int64_t never = std::numeric_limits<int64_t>::max();

  //! The tempo before nudge and rate: the tapped one, or the BPM control.
  double tempo(const free_clock_params& p) const noexcept
  {
    if(m_tapped_tempo > 0.)
      return m_tapped_tempo;
    return std::clamp(p.bpm, min_tempo, max_tempo);
  }

  int64_t period_ns(const free_clock_params& p) const noexcept
  {
    const double rate = p.rate > 0. ? p.rate : 1.;
    return int64_t(60e9 / (tempo(p) * nudge_factor(p.nudge) * 24. * rate));
  }

  bool running() const noexcept { return m_state == state::running; }
  bool starting() const noexcept { return m_state == state::starting; }
  bool count_in_pending() const noexcept { return m_starting_from_taps; }

  //! Position in MIDI beats (16th notes) of the receiver: 6 clocks each.
  int64_t song_position() const noexcept { return m_clocks / 6; }

  const tap_tempo& taps() const noexcept { return m_taps; }

  //! When process() next has something to send.
  int64_t next_deadline(const free_clock_params& p) const noexcept
  {
    switch(m_state)
    {
      case state::starting:
        return m_start_at;
      case state::running:
        return m_last_tick + period_ns(p);
      default:
        return never;
    }
  }

  template <typename Send>
  void command(transport_command c, int64_t now, const free_clock_params& p, Send&& send)
  {
    update_params(p);
    switch(c)
    {
      case transport_command::None:
        break;
      case transport_command::Start:
        m_state = state::starting;
        m_start_is_continue = false;
        m_starting_from_taps = false;
        m_start_at = now + delay_ns(p);
        break;
      case transport_command::Continue:
        if(m_state == state::stopped)
        {
          m_state = state::starting;
          m_start_is_continue = true;
          m_starting_from_taps = false;
          m_start_at = now + delay_ns(p);
        }
        break;
      case transport_command::Stop:
        if(m_state != state::stopped)
        {
          m_state = state::stopped;
          m_starting_from_taps = false;
          if(p.send_transport)
            send(message{now, {0xFC}, 1});
        }
        m_taps.reset();
        break;
    }
  }

  void tap(int64_t now, const free_clock_params& p) noexcept
  {
    update_params(p);
    m_taps.tap(now);
    if(m_taps.count() >= 2)
      m_tapped_tempo = m_taps.tempo();

    const bool idle
        = m_state == state::stopped || (m_state == state::starting && m_starting_from_taps);
    if(idle && m_taps.count() >= count_in_taps)
    {
      m_state = state::starting;
      m_start_is_continue = false;
      m_starting_from_taps = true;
      m_start_at = m_taps.last() + int64_t(m_taps.interval_ns()) + delay_ns(p);
    }
  }

  //! Sends what is due at or before `now`.
  template <typename Send>
  void process(int64_t now, const free_clock_params& p, Send&& send)
  {
    update_params(p);

    if(m_state == state::starting && now >= m_start_at)
    {
      const int64_t t = m_start_at;
      if(!m_start_is_continue)
        m_clocks = 0;
      if(p.send_transport)
      {
        const int64_t pos = std::clamp<int64_t>(song_position(), 0, 16383);
        send(message{t, {0xF2, uint8_t(pos & 0x7F), uint8_t((pos >> 7) & 0x7F)}, 3});
        send(message{t, {uint8_t(m_start_is_continue ? 0xFB : 0xFA)}, 1});
      }
      if(p.send_clock)
        send(message{t, {0xF8}, 1});
      m_clocks++;
      m_last_tick = t;
      m_state = state::running;
      m_starting_from_taps = false;
    }

    if(m_state == state::running)
    {
      for(int64_t next = m_last_tick + period_ns(p); next <= now;
          next = m_last_tick + period_ns(p))
      {
        if(p.send_clock)
          send(message{next, {0xF8}, 1});
        m_clocks++;
        m_last_tick = next;
      }
    }
  }

private:
  enum class state
  {
    stopped,
    starting,
    running
  };

  static int64_t delay_ns(const free_clock_params& p) noexcept
  {
    return p.offset_ms > 0. ? int64_t(p.offset_ms * 1e6) : 0;
  }

  void update_params(const free_clock_params& p) noexcept
  {
    if(p.bpm != m_bpm_control)
    {
      if(m_has_bpm_control)
        m_tapped_tempo = 0.;
      m_bpm_control = p.bpm;
      m_has_bpm_control = true;
    }

    const int64_t delay = delay_ns(p);
    if(delay != m_delay)
    {
      const int64_t delta = delay - m_delay;
      m_delay = delay;
      if(m_state == state::starting)
        m_start_at += delta;
      else if(m_state == state::running)
        m_last_tick += delta;
    }
  }

  tap_tempo m_taps;
  double m_tapped_tempo{};
  double m_bpm_control{};
  bool m_has_bpm_control{};
  int64_t m_delay{};

  state m_state{state::stopped};
  bool m_start_is_continue{};
  bool m_starting_from_taps{};
  int64_t m_start_at{};
  int64_t m_last_tick{};
  int64_t m_clocks{};
};

/**
 * Clocks locked to the score's timeline, as the audio thread sends them with
 * sample-accurate timestamps.
 *
 * The clock at timeline time T is sent at T + offset. A negative offset sends
 * clocks ahead of the timeline by extrapolating the current tempo: a tempo
 * change is followed from the next clock on, and the clocks already sent are
 * not taken back.
 *
 * Clocks the timeline jumps over (seek, transport) are skipped. Clocks that a
 * smaller offset moves before the current tick, including at the first tick
 * with a negative offset, are sent at the start of the tick instead, so that
 * a receiver counting clocks keeps its position.
 */
struct timeline_clock
{
  double last_clock_time = 0.0;
  uint32_t clock_tick_count = 0;
  double previous_offset = 0.0;

  /**
   * Puts the clock back on the grid of the timeline at a new position, as at
   * the start: the next clock is the first grid point after `quarters`.
   * `time` is the timeline time of `quarters`, in seconds.
   */
  void locate(double time, double quarters, double tempo, double factor) noexcept
  {
    const double clocks_per_quarter = 24.0 * factor;
    const double grid = std::floor(quarters * clocks_per_quarter + 1e-9);
    const double since_grid = quarters - grid / clocks_per_quarter;
    last_clock_time = time - since_grid * 60.0 / tempo;
    previous_offset = 0.0;
  }

  //! factor: nudge times clock rate; offset in seconds.
  template <typename Emit>
  void process(
      double current_time, double frame_duration, int64_t frames, double tempo,
      double factor, double offset, Emit&& emit)
  {
    const double seconds_per_tick = 60.0 / (tempo * 24.0 * factor);

    const double buffer_start_time = current_time - offset;
    const double buffer_end_time = buffer_start_time + frame_duration;
    double gap_start_time = buffer_start_time;
    if(frames > 0)
    {
      if(offset < previous_offset)
        gap_start_time -= previous_offset - offset;
      previous_offset = offset;
    }

    double next_tick_time = last_clock_time + seconds_per_tick;

    while(next_tick_time < buffer_end_time)
    {
      if(next_tick_time >= buffer_start_time)
      {
        int64_t sample_offset
            = ((next_tick_time - buffer_start_time) / frame_duration) * frames;
        sample_offset = std::clamp<int64_t>(sample_offset, 0, frames - 1);

        emit(sample_offset);
        clock_tick_count++;
      }
      else if(next_tick_time >= gap_start_time)
      {
        emit(int64_t(0));
        clock_tick_count++;
      }

      last_clock_time = next_tick_time;
      next_tick_time += seconds_per_tick;
    }
  }
};
}
