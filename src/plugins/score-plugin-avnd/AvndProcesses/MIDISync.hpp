#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/detail/lockfree_queue.hpp>
#include <ossia/detail/thread.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/protocols/midi/midi_protocol.hpp>
#include <ossia/protocols/midi/midi_stream.hpp>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <halp/midifile_port.hpp>
#include <libremidi/message.hpp>

#include <AvndProcesses/MIDIClockEngine.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <initializer_list>
#include <span>
#include <thread>

namespace mtk
{
enum class MidiClockMode
{
  Disabled,
  Enabled
};
enum class MidiStartStopMode
{
  Disabled,
  Enabled
};
enum class MidiTimeCodeMode
{
  Disabled,
  Enabled
};
enum class MidiTimeCodeFrameRate
{
  SMPTE_24 = 0b00,
  SMPTE_25 = 0b01,
  SMPTE_2997 = 0b10,
  SMPTE_30 = 0b11
};

enum class MidiClockSource
{
  Score,
  Free
};

enum class MidiClockRate
{
  x1,
  quarter,
  third,
  half,
  x2,
  x3,
  x4
};

// https://blat-blatnik.github.io/computerBear/making-accurate-sleep-function/
struct sleep_accurate
{
  double estimate = 5e-3;
  double mean = 5e-3;
  double m2 = 0;
  int64_t count = 1;

  void operator()(double seconds)
  {
    using namespace std;
    using namespace std::chrono;

    while(seconds > estimate)
    {
      auto start = high_resolution_clock::now();
      this_thread::sleep_for(milliseconds(1));
      auto end = high_resolution_clock::now();

      double observed = (end - start).count() / 1e9;
      seconds -= observed;

      ++count;
      double delta = observed - mean;
      mean += delta / count;
      m2 += delta * (observed - mean);
      double stddev = std::sqrt(m2 / (count - 1));
      estimate = mean + stddev;

      // FIXME that's missing a rolling behaviour to be more
      // precise for semi-large timescales, e.g.
      // unplugging a laptop and powersave changing frequency
    }

    // spin lock
    auto start = high_resolution_clock::now();
    while((high_resolution_clock::now() - start).count() / 1e9 < seconds)
      ;
  }
};

/**
 * Send MIDI Clock and TimeCode
 */
struct MIDISyncOut
{
  halp_meta(name, "MIDI Sync Out")
  halp_meta(author, "ossia team")
  halp_meta(c_name, "avnd_helpers_midisync")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-sync.html")
  halp_meta(uuid, "aa7c1ae5-495e-436e-a079-e3f1a19861bb")
  halp_meta(category, "Timing/Midi")
  halp_meta(description, "Send MIDI Clock, transport messages and MIDI Time Code synchronized to score playback.")
  halp_flag(process_exec);

  ossia::exec_state_facade ossia_state;
  /**
   * The MIDI device the output is bound to, as of the last tick: audio thread
   * only. The clock thread gets its own copy through to_device, in order with
   * the messages, and lets go of it when the process stops or pauses: a
   * process that is not ticked cannot tell when its device is removed. In Free
   * mode the clock then sends to the output port until the next tick.
   */
  ossia::net::midi::midi_stream* midi_out{};

  struct ins
  {
    halp::enum_t<MidiClockMode, "MIDI Clock"> clock;
    halp::enum_t<MidiStartStopMode, "MIDI Start/Stop"> clock_startstop;
    halp::enum_t<MidiTimeCodeMode, "MIDI TimeCode"> mtc;
    //halp::spinbox_i32<"Channel", halp::irange{1, 16, 1}> channel;
    halp::spinbox_i32<"MTC offset (s)", halp::irange{-128000, 128000, 0}> offset;
    struct : halp::enum_t<MidiTimeCodeFrameRate, "MTC rate">
    {
      struct range
      {
        std::string_view values[4] = {"24", "25", "29.97", "30"};
        MidiTimeCodeFrameRate init{};
      };
    } rate;

    struct : halp::enum_t<MidiClockSource, "Clock source">
    {
      halp_meta(
          description,
          "Score: clock and transport follow score playback. Free: the clock runs at "
          "its own tempo, driven by Start, Stop, Continue and Tap")
      struct range
      {
        std::string_view values[2] = {"Score", "Free"};
        MidiClockSource init{};
      };
    } source;

    struct : halp::spinbox_f32<"BPM", halp::range{20., 300., 120.}>
    {
      halp_meta(description, "Tempo of the free clock, until a tempo is tapped")
    } bpm;

    struct : halp::impulse_button<"Start">
    {
      halp_meta(description, "Free clock: send Start and clock from the beginning")
    } start_button;

    struct : halp::impulse_button<"Stop">
    {
      halp_meta(description, "Free clock: send Stop and stop the clock")
    } stop_button;

    struct : halp::impulse_button<"Continue">
    {
      halp_meta(description, "Free clock: send Continue and clock from where it stopped")
    } continue_button;

    struct : halp::impulse_button<"Tap">
    {
      halp_meta(
          description,
          "Free clock: tap the tempo (average of the last 32 taps, a pause of 3 s "
          "starts over). When stopped, 4 taps start the clock on the next beat")
    } tap;

    struct : halp::maintained_button<"Nudge down">
    {
      halp_meta(description, "Clock 10% slower while held, to move its phase back")
    } nudge_down;

    struct : halp::maintained_button<"Nudge up">
    {
      halp_meta(description, "Clock 10% faster while held, to move its phase forward")
    } nudge_up;

    struct : halp::enum_t<MidiClockRate, "Clock rate">
    {
      halp_meta(description, "Multiplies the number of clocks sent per beat")
      struct range
      {
        std::string_view values[7] = {"x1", "x1/4", "x1/3", "x1/2", "x2", "x3", "x4"};
        MidiClockRate init{};
      };
    } clock_rate;

    struct : halp::spinbox_f32<"Clock offset (ms)", halp::range{-1000., 1000., 0.}>
    {
      halp_meta(
          description,
          "Positive values delay the clock and the transport messages. Negative values "
          "send the score clock early by extrapolating the current tempo, on the output "
          "port or to a MIDI device")
    } clock_offset;
  } inputs;

  struct
  {
    struct : halp::midi_bus<"MIDI output">
    {
      ossia::net::node_base* ossia_node{};
    } midi;

    struct : halp::val_port<"Tempo", float>
    {
      halp_meta(description, "Tempo of the clock before nudge and rate")
    } tempo;
  } outputs;

  struct ui
  {
    halp_meta(name, "MIDI Sync Out")
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::background_dark)

    struct
    {
      halp_meta(name, "Send")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::clock> clock;
      halp::item<&ins::clock_startstop> clock_startstop;
      halp::item<&ins::mtc> mtc;
      halp::item<&ins::offset> offset;
      halp::item<&ins::rate> rate;
    } send;

    struct
    {
      halp_meta(name, "Clock")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      halp::item<&ins::source> source;
      halp::item<&ins::clock_rate, halp::look{.widget = halp::control_widget::combo}>
          clock_rate;
      halp::item<&ins::clock_offset> clock_offset;
    } clock;

    struct
    {
      halp_meta(name, "Free clock")
      halp_meta(layout, halp::layouts::section)
      halp_meta(background, halp::colors::background_mid)
      static constexpr auto condition()
      {
        return halp::enabled_when<&ins::source, MidiClockSource::Free>{};
      }
      halp::item<&ins::bpm> bpm;
      struct
      {
        halp_meta(layout, halp::layouts::hbox)
        halp::item<&ins::start_button> start;
        halp::item<&ins::stop_button> stop;
        halp::item<&ins::continue_button> cont;
      } transport;
      struct
      {
        halp_meta(layout, halp::layouts::hbox)
        halp::item<&ins::tap> tap;
        halp::item<&ins::nudge_down> nudge_down;
        halp::item<&ins::nudge_up> nudge_up;
      } tempo;
    } free_clock;
  };

  // Free clock: written by operator(), read by the clock thread.
  std::atomic_bool free_mode{false};
  std::atomic_bool free_send_clock{false};
  std::atomic_bool free_send_transport{false};
  std::atomic<float> free_bpm{120.f};
  std::atomic<float> clock_rate_factor{1.f};
  std::atomic<int> nudge{0};
  std::atomic<float> clock_offset_ms{0.f};
  std::atomic<midi_clock::transport_command> free_command{
      midi_clock::transport_command::None};
  //! steady_clock date of a tap not yet seen by the clock thread, or 0.
  std::atomic<int64_t> pending_tap{0};
  //! Written by the clock thread.
  std::atomic<float> free_tempo{120.f};
  //! What the free clock sends when the output has no MIDI device to write to:
  //! operator() forwards it through the output port, and drops what waited
  //! longer than a tick or two: nothing drains it while the process is not
  //! ticked.
  ossia::spsc_queue<midi_clock::message> to_output_port{1024};

  //! A message for the device, or a change of the device itself.
  struct device_message
  {
    midi_clock::message msg;
    ossia::net::midi::midi_stream* device{};
    bool rebind{};
  };

  /**
   * What operator() makes when the output is a MIDI device: score clock,
   * transport and timecode, dated in steady_clock nanoseconds. The clock thread
   * writes each to the device at its date; the output port stays empty, else
   * the graph would write everything a second time, in a burst at the end of
   * the tick. A full queue drops messages.
   */
  ossia::spsc_queue<device_message> to_device{4096};

  //! Timecode fields of a timeline position.
  struct mtc_time
  {
    uint8_t frame_rate = 0b10;
    uint8_t h = 0;
    uint8_t m = 0;
    uint8_t s = 0;
    uint8_t f = 0;
  };

  using tick = halp::tick_flicks;
  std::thread clock_thread;
  std::atomic_bool clock_thread_running = true;

  //! Set by start(), stop(), pause() and resume(); handled by the next tick.
  std::atomic_bool start_requested{false};
  std::atomic_bool stop_requested{false};
  std::atomic_bool pause_requested{false};

  static void send_midi(
      ossia::net::midi::midi_stream* device, std::span<const uint8_t> bytes)
  {
    if(device)
      device->push_value(libremidi::message{{std::begin(bytes), std::end(bytes)}, 0});
  }

  [[nodiscard]]
  static constexpr auto from_mtc_framerate(uint32_t frame_rate)
  {
    switch(frame_rate)
    {
      case 0b00:
        return 24.;
      case 0b01:
        return 25.;
      case 0b10:
        return 29.97;
      case 0b11:
        return 30.;
    }
    return 30.;
  }

  static int64_t steady_now_ns() noexcept
  {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }

  midi_clock::free_clock_params load_free_params() const noexcept
  {
    return midi_clock::free_clock_params{
        .bpm = free_bpm.load(std::memory_order_relaxed),
        .rate = clock_rate_factor.load(std::memory_order_relaxed),
        .nudge = nudge.load(std::memory_order_relaxed),
        .offset_ms = clock_offset_ms.load(std::memory_order_relaxed),
        .send_clock = free_send_clock.load(std::memory_order_relaxed),
        .send_transport = free_send_transport.load(std::memory_order_relaxed)};
  }

  void send_free_clock_message(
      ossia::net::midi::midi_stream* device, const midi_clock::message& m)
  {
    if(device)
      send_midi(device, std::span<const uint8_t>(m.bytes.data(), m.size));
    else
      to_output_port.try_enqueue(m);
  }

  /**
   * The clock thread: runs the free clock, and writes to the MIDI device what
   * operator() dated for it. Every message is sent at its own date, from the
   * steady clock: waiting never adds up from one message to the next.
   */
  void clock_thread_main()
  {
    using namespace std::chrono;
    ossia::set_thread_name("ossia midi clock");
    ossia::set_thread_pinned(ossia::thread_type::Midi, 0);

    sleep_accurate precise_sleep;
    midi_clock::free_clock free_clock;
    bool was_free = false;
    device_message pending;
    bool has_pending = false;
    ossia::net::midi::midi_stream* device{};
    auto send = [this, &device](const midi_clock::message& m) {
      send_free_clock_message(device, m);
    };

    while(clock_thread_running.load(std::memory_order_acquire))
    {
      const auto p = load_free_params();
      const int64_t now = steady_now_ns();
      int64_t deadline = midi_clock::free_clock::never;
      if(free_mode.load(std::memory_order_acquire))
      {
        was_free = true;
        if(auto c = free_command.exchange(midi_clock::transport_command::None);
           c != midi_clock::transport_command::None)
          free_clock.command(c, now, p, send);
        if(auto t = pending_tap.exchange(0); t != 0)
          free_clock.tap(t, p);

        free_clock.process(now, p, send);
        free_tempo.store(free_clock.tempo(p), std::memory_order_relaxed);
        deadline = free_clock.next_deadline(p);
      }
      else
      {
        if(was_free)
        {
          was_free = false;
          free_clock.command(midi_clock::transport_command::Stop, now, p, send);
        }
        free_command.exchange(midi_clock::transport_command::None);
        pending_tap.exchange(0);
      }

      for(;;)
      {
        if(!has_pending)
          has_pending = to_device.try_dequeue(pending);
        if(!has_pending)
          break;
        if(pending.rebind)
        {
          device = pending.device;
          has_pending = false;
          continue;
        }
        if(pending.msg.at > steady_now_ns())
        {
          deadline = std::min(deadline, pending.msg.at);
          break;
        }
        send_midi(device, std::span<const uint8_t>(pending.msg.bytes.data(), pending.msg.size));
        has_pending = false;
      }

      // Sleep 1 ms at most, to see new commands and messages.
      const int64_t wait = deadline - steady_now_ns();
      if(wait > 3'000'000)
        std::this_thread::sleep_for(milliseconds(1));
      else if(wait > 0)
        precise_sleep(wait / 1e9);
    }
  }

  MIDISyncOut()
  {
    clock_thread = std::thread{[this] { clock_thread_main(); }};
  }

  ~MIDISyncOut()
  {
    clock_thread_running.store(false, std::memory_order_release);
    clock_thread.join();
  }

  void start() { start_requested.store(true, std::memory_order_release); }
  void stop() { stop_requested.store(true, std::memory_order_release); }
  void pause() { pause_requested.store(true, std::memory_order_release); }
  void resume() { }


  [[nodiscard]]
  mtc_time make_mtc_time(double total_seconds) const noexcept
  {
    mtc_time t;
    t.frame_rate
        = static_cast<std::underlying_type_t<MidiTimeCodeFrameRate>>(inputs.rate.value)
          & 0b11;

    total_seconds += this->inputs.offset;
    if(total_seconds < 0.)
      total_seconds = 0.;

    auto h = std::div((long long)total_seconds, (long long)3600).quot;
    total_seconds -= h * 3600;
    auto m = std::div((long long)total_seconds, (long long)60).quot;
    total_seconds -= m * 60;
    double s;
    auto frames = std::modf(total_seconds, &s);

    t.h = h % 24;
    t.m = m % 60;
    t.s = uint8_t(s);
    t.f = std::min(uint8_t(from_mtc_framerate(t.frame_rate) * frames), uint8_t(29));
    return t;
  }

  halp::setup setup;
  void prepare(halp::setup s) { setup = s; }

  void operator()(halp::tick_flicks tk)
  {
    if(setup.rate <= 0)
      return;

    bind_output();
    publish_clock_controls();
    begin_tick(tk.frames);
    const bool is_free = inputs.source.value == MidiClockSource::Free;

    if(start_requested.exchange(false, std::memory_order_acq_rel))
      main_state.running = false;

    // The executor ticks once more, with no frame, after stop().
    const bool stopping = stop_requested.exchange(false, std::memory_order_acq_rel);
    const bool pausing = pause_requested.exchange(false, std::memory_order_acq_rel);
    if(stopping || pausing)
    {
      // No tick may come to send them later: on the port, what was delayed
      // goes out now, then Stop.
      flush_delayed();
      if(main_state.running && !is_free
         && inputs.clock_startstop == MidiStartStopMode::Enabled)
      {
        if(main_state.to_device)
          emit_transport({0xFC});
        else
          emit(0, {0xFC});
      }
      main_state.running = false;
    }
    else
    {
      release_delayed(tk.frames);
    }

    if(tk.frames > 0)
      process_timeline(tk, is_free);

    if(tk.frames > 0)
      main_state.out_frame += tk.frames;

    outputs.tempo.value = free_mode.load(std::memory_order_relaxed)
                              ? free_tempo.load(std::memory_order_relaxed)
                              : float(tk.tempo);

    // Free clock messages arrive when they are due: they go out at the start
    // of the tick, one tick of latency at most. Older ones were sent while
    // the process was not ticked, and are dropped.
    const int64_t stale_before
        = steady_now_ns() - 2 * main_state.latency_ns - 2'000'000;
    midi_clock::message m;
    while(to_output_port.try_dequeue(m))
    {
      if(m.at < stale_before)
        continue;
      halp::midi_msg msg;
      msg.bytes.assign(m.bytes.begin(), m.bytes.begin() + m.size);
      msg.timestamp = 0;
      outputs.midi.push_back(std::move(msg));
    }
    sort_output();

    // No tick may come before the device is removed: the clock thread lets go
    // of it once it has sent what this tick dated for it.
    if(stopping || pausing)
      set_device(nullptr);
  }

  //! Hands the device over to the clock thread, after what is queued for the
  //! previous one.
  void set_device(ossia::net::midi::midi_stream* stream) noexcept
  {
    if(stream == midi_out)
      return;
    device_message d;
    d.device = stream;
    d.rebind = true;
    if(to_device.try_enqueue(d))
      midi_out = stream;
  }

  //! The MIDI device the output is bound to, if any.
  void bind_output() noexcept
  {
    ossia::net::midi::midi_stream* stream{};
    if(outputs.midi.ossia_node)
    {
      auto& proto = outputs.midi.ossia_node->get_device().get_protocol();
      stream = dynamic_cast<ossia::net::midi::midi_stream*>(&proto);
    }
    set_device(stream);
  }

  //! Hands the controls the clock thread reads over to it.
  void publish_clock_controls() noexcept
  {
    const bool is_free = inputs.source.value == MidiClockSource::Free;
    free_send_clock.store(inputs.clock == MidiClockMode::Enabled, std::memory_order_relaxed);
    free_send_transport.store(
        inputs.clock_startstop == MidiStartStopMode::Enabled, std::memory_order_relaxed);
    free_bpm.store(inputs.bpm.value, std::memory_order_relaxed);
    clock_rate_factor.store(
        midi_clock::clock_rate_factor(int(inputs.clock_rate.value)),
        std::memory_order_relaxed);
    nudge.store(
        int(inputs.nudge_up.value) - int(inputs.nudge_down.value),
        std::memory_order_relaxed);
    clock_offset_ms.store(inputs.clock_offset.value, std::memory_order_relaxed);
    free_mode.store(is_free, std::memory_order_release);

    if(!is_free)
      return;

    using enum midi_clock::transport_command;
    if(inputs.stop_button)
      free_command.store(Stop, std::memory_order_release);
    else if(inputs.start_button)
      free_command.store(Start, std::memory_order_release);
    else if(inputs.continue_button)
      free_command.store(Continue, std::memory_order_release);

    if(inputs.tap)
    {
      const int64_t t = steady_now_ns();
      pending_tap.store(t != 0 ? t : 1, std::memory_order_release);
    }
  }

  /**
   * Dates the frames of the tick for the device: the start of the tick
   * follows the previous one by its length, corrected slowly towards the time
   * operator() is called, so that the jitter of the audio callback does not
   * reach the clock. One buffer of latency puts every message of the tick in
   * the future of the clock thread.
   */
  void begin_tick(int frames) noexcept
  {
    const int64_t now = steady_now_ns();
    const double ns_per_frame = 1e9 / setup.rate;
    const int64_t buffer_ns = int64_t(std::max(setup.frames, frames) * ns_per_frame);
    const int64_t drift = now - main_state.next_tick_ns;
    if(main_state.next_tick_ns == 0 || std::abs(drift) > buffer_ns + 2'000'000)
      main_state.tick_start_ns = now;
    else
      main_state.tick_start_ns = main_state.next_tick_ns + drift / 16;
    main_state.ns_per_frame = ns_per_frame;
    main_state.latency_ns = buffer_ns;
    main_state.next_tick_ns
        = main_state.tick_start_ns + int64_t(std::max(frames, 0) * ns_per_frame);
    main_state.to_device = midi_out != nullptr;
    main_state.tick_frames = std::max(frames, 0);
  }

  //! Sends a message at a frame of the tick, to the port or to the device.
  void emit(int64_t frame, std::initializer_list<uint8_t> bytes) noexcept
  {
    if(main_state.to_device)
    {
      device_message d;
      auto& m = d.msg;
      m.at = main_state.tick_start_ns + main_state.latency_ns
             + int64_t(frame * main_state.ns_per_frame);
      m.size = int(std::min(bytes.size(), m.bytes.size()));
      std::copy_n(bytes.begin(), m.size, m.bytes.begin());
      to_device.try_enqueue(d);
    }
    else
    {
      halp::midi_msg msg;
      msg.bytes.assign(bytes.begin(), bytes.end());
      msg.timestamp = frame;
      outputs.midi.push_back(std::move(msg));
    }
  }

  /**
   * Start, Continue, Stop and Song Position, delayed by a positive clock
   * offset as the clocks are, so that a receiver sees them in the same
   * relation to the clock. What falls past the current tick waits in
   * main_state.delayed, dated in frames since the object started.
   */
  void emit_transport(std::initializer_list<uint8_t> bytes) noexcept
  {
    const double offset = inputs.clock_offset.value / 1000.;
    const int64_t delay = offset > 0. ? int64_t(std::llround(offset * setup.rate)) : 0;
    if(main_state.to_device || delay < main_state.tick_frames)
    {
      emit(delay, bytes);
      return;
    }

    auto& d = main_state.delayed;
    if(main_state.delayed_count == int(d.size()))
    {
      // Out of room: late rather than lost.
      emit(std::max<int64_t>(main_state.tick_frames - 1, 0), bytes);
      return;
    }
    auto& e = d[main_state.delayed_count++];
    e.at = main_state.out_frame + delay;
    e.size = int(std::min(bytes.size(), e.bytes.size()));
    std::copy_n(bytes.begin(), e.size, e.bytes.begin());
  }

  void emit_message(int64_t frame, const midi_clock::message& m) noexcept
  {
    halp::midi_msg msg;
    msg.bytes.assign(m.bytes.begin(), m.bytes.begin() + m.size);
    msg.timestamp = frame;
    outputs.midi.push_back(std::move(msg));
  }

  //! Sends the delayed transport messages due in this tick.
  void release_delayed(int frames) noexcept
  {
    const int64_t end = main_state.out_frame + std::max(frames, 0);
    int kept = 0;
    for(int i = 0; i < main_state.delayed_count; i++)
    {
      auto& e = main_state.delayed[i];
      if(e.at < end)
        emit_message(std::max<int64_t>(e.at - main_state.out_frame, 0), e);
      else
        main_state.delayed[kept++] = e;
    }
    main_state.delayed_count = kept;
  }

  void flush_delayed() noexcept
  {
    for(int i = 0; i < main_state.delayed_count; i++)
      emit_message(0, main_state.delayed[i]);
    main_state.delayed_count = 0;
  }

  //! The port's messages in time order: delayed ones are added out of order.
  void sort_output() noexcept
  {
    auto& v = outputs.midi.midi_messages;
    for(std::size_t i = 1; i < v.size(); i++)
      for(std::size_t j = i; j > 0 && v[j - 1].timestamp > v[j].timestamp; j--)
        std::swap(v[j - 1], v[j]);
  }

  void process_timeline(const halp::tick_flicks& tk, bool is_free)
  {
    const double current_time = tk.start_in_flicks / 705'600'000.0;
    const double frame_duration
        = std::abs(tk.end_in_flicks - tk.start_in_flicks) / 705'600'000.0;
    const auto time = make_mtc_time(current_time);
    const double factor
        = midi_clock::nudge_factor(
              int(inputs.nudge_up.value) - int(inputs.nudge_down.value))
          * midi_clock::clock_rate_factor(int(inputs.clock_rate.value));

    // A tick that does not start where the previous one ended is a jump of
    // the timeline: a seek, or a loop starting over.
    const bool jumped
        = main_state.running && tk.start_in_flicks != main_state.next_start_in_flicks;
    if(!main_state.running || jumped)
      locate(tk, time, current_time, factor, jumped, is_free);
    main_state.running = true;
    main_state.next_start_in_flicks = tk.end_in_flicks;

    if(!is_free && inputs.clock == MidiClockMode::Enabled)
    {
      main_state.clock.process(
          current_time, frame_duration, tk.frames, tk.tempo, factor,
          inputs.clock_offset.value / 1000.,
          [this](int64_t sample_offset) { emit(sample_offset, {0xF8}); });
    }

    if(inputs.mtc == MidiTimeCodeMode::Enabled)
      send_quarter_frames(tk, time, current_time, frame_duration);
  }

  /**
   * Starting, resuming, seeking and looping: Song Position then Start or
   * Continue, as a receiver expects when it is stopped, after a Stop when it
   * was running; Start at the beginning of the timeline. The clock and the
   * timecode start over from the new position.
   */
  void locate(
      const halp::tick_flicks& tk, const mtc_time& time, double current_time,
      double factor, bool jumped, bool is_free)
  {
    const bool transport
        = !is_free && inputs.clock_startstop == MidiStartStopMode::Enabled;
    const bool clock = !is_free && inputs.clock == MidiClockMode::Enabled;
    const double quarters = tk.start_position_in_quarters;
    const bool at_start = std::abs(quarters) < 1e-9;

    if(jumped && transport)
      emit_transport({0xFC});

    if(clock || transport)
    {
      // A MIDI beat is a 16th note, 14 bits: past 1024 bars there is no
      // position to send.
      const int64_t beats = int64_t(std::floor(quarters * 4. + 1e-9));
      if(!(transport && at_start) && beats >= 0 && beats < 16384)
        emit_transport({0xF2, uint8_t(beats & 0x7F), uint8_t((beats >> 7) & 0x7F)});
      if(transport)
        emit_transport({uint8_t(at_start ? 0xFA : 0xFB)});
    }

    main_state.clock.locate(current_time, quarters, tk.tempo, factor);

    if(inputs.mtc == MidiTimeCodeMode::Enabled)
    {
      emit(
          0, {0xF0, 0x7F, 0x7F, 0x01, 0x01, uint8_t(time.h | (time.frame_rate << 5)),
              time.m, time.s, time.f, 0xF7});
    }
    const double fps = from_mtc_framerate(time.frame_rate);
    main_state.mtc_quarter_frame_index = 0;
    main_state.last_mtc_quarter_frame_time
        = next_even_frame(current_time, fps) - 1.0 / (fps * 4.0);
  }

  /**
   * Timeline time of the first even frame of the timecode at or after `t`.
   * The eight quarter frames span two frames: a receiver reads the time of
   * the first piece on even frames.
   */
  double next_even_frame(double t, double fps) const noexcept
  {
    const double offset = this->inputs.offset;
    const double u = t + offset;
    const double second = std::floor(u);
    // As make_mtc_time numbers them: from 0 in each second
    const int per_second = int(std::ceil(fps - 1e-9));
    int k = int(std::ceil((u - second) * fps - 1e-6));
    if(k % 2 != 0)
      k++;
    const double frame = k < per_second ? second + k / fps : second + 1.;
    return frame - offset;
  }

  /**
   * The eight pieces of a timecode, a quarter of a frame apart. Piece 0 takes
   * the time at its own date, and the seven others carry the same time, so
   * that a receiver assembles one consistent value across a carry.
   */
  void send_quarter_frames(
      const halp::tick_flicks& tk, const mtc_time& time, double current_time,
      double frame_duration)
  {
    const double fps = from_mtc_framerate(time.frame_rate);
    const double seconds_per_quarter_frame = 1.0 / (fps * 4.0);

    double next_qf_time
        = main_state.last_mtc_quarter_frame_time + seconds_per_quarter_frame;

    while(next_qf_time < current_time + frame_duration)
    {
      // A piece due at the very start of the tick is not lost to rounding
      if(next_qf_time >= current_time - 1e-9)
      {
        int64_t sample_offset
            = ((next_qf_time - current_time) / frame_duration) * tk.frames;
        sample_offset = std::clamp<int64_t>(sample_offset, 0, tk.frames - 1);

        // Half a piece in, to stay clear of the frame boundary
        if(main_state.mtc_quarter_frame_index == 0)
          main_state.mtc_cycle
              = make_mtc_time(next_qf_time + seconds_per_quarter_frame / 2.);
        const mtc_time& cycle = main_state.mtc_cycle;

        uint8_t qf_data = 0;
        switch(main_state.mtc_quarter_frame_index)
        {
          case 0:
            qf_data = 0x00 | (cycle.f & 0x0F);
            break;
          case 1:
            qf_data = 0x10 | ((cycle.f >> 4) & 0x01);
            break;
          case 2:
            qf_data = 0x20 | (cycle.s & 0x0F);
            break;
          case 3:
            qf_data = 0x30 | ((cycle.s >> 4) & 0x03);
            break;
          case 4:
            qf_data = 0x40 | (cycle.m & 0x0F);
            break;
          case 5:
            qf_data = 0x50 | ((cycle.m >> 4) & 0x03);
            break;
          case 6:
            qf_data = 0x60 | (cycle.h & 0x0F);
            break;
          case 7:
            qf_data = 0x70 | (cycle.frame_rate << 1) | ((cycle.h >> 4) & 0x01);
            break;
        }

        emit(sample_offset, {0xF1, qf_data});

        main_state.mtc_quarter_frame_index
            = (main_state.mtc_quarter_frame_index + 1) % 8;
      }

      main_state.last_mtc_quarter_frame_time = next_qf_time;
      next_qf_time += seconds_per_quarter_frame;
    }
  }

  //! State of the audio thread.
  struct MainThreadState
  {
    midi_clock::timeline_clock clock;
    double last_mtc_quarter_frame_time = 0.0;
    int mtc_quarter_frame_index = 0;
    //! The time the pieces of the current timecode carry.
    mtc_time mtc_cycle{};
    //! Playing since the last start, stop or pause.
    bool running = false;
    int64_t next_start_in_flicks = 0;

    //! Frames of output since the object started, and in the current tick.
    int64_t out_frame = 0;
    int tick_frames = 0;
    std::array<midi_clock::message, 32> delayed{};
    int delayed_count = 0;

    bool to_device = false;
    int64_t tick_start_ns = 0;
    int64_t next_tick_ns = 0;
    int64_t latency_ns = 0;
    double ns_per_frame = 0.;
  } main_state;
};
}
