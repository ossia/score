#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/detail/thread.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/protocols/midi/midi_protocol.hpp>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <halp/midifile_port.hpp>
#include <libremidi/message.hpp>

#include <cmath>

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
  SMPTE_30 = 0b10,
  SMPTE_2997 = 0b11
};

enum class MidiStartStopEvent
{
  None,
  Start,
  Stop,
  Continue
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
  halp_flag(process_exec);

  ossia::exec_state_facade ossia_state;
  std::atomic<ossia::net::midi::midi_protocol*> midi_out{};
  std::atomic<MidiStartStopEvent> next_event_midiclock{};
  std::atomic<MidiStartStopEvent> next_event_mtc{};
  std::atomic<double> current_song_pos{};

  struct
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
  } inputs;

  struct
  {
    struct : halp::midi_bus<"MIDI output">
    {
      ossia::net::node_base* ossia_node{};
    } midi;
  } outputs;

  union storage
  {
    uint64_t u;
    struct alignas(uint64_t) impl
    {
      float tempo = 0.f;
      uint32_t has_clock : 1 = 0;
      uint32_t has_startstop : 1 = 0;
      uint32_t has_mtc : 1 = 0;

      uint32_t frame_rate : 2 = 0b10;
      uint32_t h : 5 = 0;
      uint32_t m : 6 = 0;
      uint32_t s : 6 = 0;
      uint32_t f : 5 = 0;
    };
    static_assert(sizeof(impl) == 8);
  };

  using tick = halp::tick_flicks;
  std::thread clock_thread;
  std::thread mtc_thread;
  std::atomic_bool clock_thread_running = true;
  std::atomic_bool mtc_thread_running = true;

  std::atomic<uint64_t> current_state = 0;

  template <typename... T>
  void send_midi(T... bytes)
    requires(!(std::is_pointer_v<T> || ...))
  {
    if(auto proto = midi_out.load())
      proto->push_value(libremidi::message{
          libremidi::midi_bytes{static_cast<unsigned char>(bytes)...}, 0});
  }

  void send_midi(std::span<const uint8_t> bytes)
  {
    if(auto proto = midi_out.load())
      proto->push_value(libremidi::message{{std::begin(bytes), std::end(bytes)}, 0});
  }

  [[nodiscard]]
  auto load_state() noexcept
  {
    auto u = this->current_state.load(std::memory_order_acquire);
    auto state = std::bit_cast<storage::impl>(u);
    if(state.tempo <= 0.)
      state.tempo = 120.;
    return state;
  }

  [[nodiscard]]
  static auto compute_time_between_ticks(storage::impl state) noexcept
  {
    const double duration_of_quarter_note_in_seconds = 60. / state.tempo;
    const std::chrono::nanoseconds time_between_ticks = std::chrono::nanoseconds(
        int64_t(1e9 * duration_of_quarter_note_in_seconds / 24.));
    return time_between_ticks;
  };

  void full_songpos_message(double quarters)
  {
    // A midi beat = a 16th note
    // Note that this means due to having only 14 bits of storage,
    // that a song is limited to 1024 bars.. half an hour at 120 bpm lol
    // To prevent unwanted looping we will make the editorial choice to not send the message
    // if it ends up > to that limit
    double midi_beats = quarters * 4.;

    uint64_t res = std::floor(midi_beats);
    if(res < 16384)
    {
      // 0b0111'1111 0b0001'1111
      uint8_t message[3] = {0xF2, 0x00, 0x00};
      message[1] = (res & 0b0011'1111'1000'0000) >> 7;
      message[2] = (res & 0b0000'0000'0111'1111);
      send_midi(message);
    }
  }

  void full_mtc_message(storage::impl state)
  {
    static_assert(0b0000'0011 << 5 == 0b01100000);
    const uint8_t h = state.h | (state.frame_rate << 5);
    const uint8_t m = state.m;
    const uint8_t s = state.s;
    const uint8_t f = state.f;

    const uint8_t bytes[10]{0xF0, 0x7F, 0x7F, 0x01, 0x00, h, m, s, f, 0xF7};
    send_midi(bytes);
  }

  void current_mtc_message(int& index, storage::impl state)
  {
    uint8_t bytes[2]{0xF1, 0};
    // thanks wikipedia my good friend i promise i will donate
    // 0 	0000 ffff 	Frame number lsbits
    // 1 	0001 000f 	Frame number msbit
    // 2 	0010 ssss 	Second lsbits
    // 3 	0011 00ss 	Second msbits
    // 4 	0100 mmmm 	Minute lsbits
    // 5 	0101 00mm 	Minute msbits
    // 6 	0110 hhhh 	Hour lsbits
    // 7 	0111 0rrh 	Rate and hour msbit
    switch(index)
    {
      case 0:
        bytes[1] = 0b0000'0000 | (0b1111 & state.f);
        index++;
        break;
      case 1:
        bytes[1] = 0b0001'0000 | (0b0001 & (state.f >> 4));
        index++;
        break;
      case 2:
        bytes[1] = 0b0010'0000 | (0b1111 & state.s);
        index++;
        break;
      case 3:
        bytes[1] = 0b0011'0000 | (0b0011 & (state.s >> 4));
        index++;
        break;
      case 4:
        bytes[1] = 0b0100'0000 | (0b1111 & state.m);
        index++;
        break;
      case 5:
        bytes[1] = 0b0101'0000 | (0b0011 & (state.m >> 4));
        index++;
        break;
      case 6:
        bytes[1] = 0b0110'0000 | (0b0011 & state.h);
        index++;
        break;
      case 7:
        bytes[1] = 0b0111'0000 | (state.frame_rate << 1) | (0b0001 & (state.h >> 4));
        index = 0;
        break;
    }
    send_midi(bytes);
  }

  [[nodiscard]]
  static constexpr auto from_mtc_framerate(uint32_t frame_rate)
  {
    switch(frame_rate)
    {
      case 0b00:
        return 24.;
        break;
      case 0b01:
        return 25.;
        break;
      case 0b10:
        return 30.;
        break;
      case 0b11:
        return 29.97;
        break;
    }
    return 30.;
  }

  MIDISyncOut()
  {
    // Midi Clock handling
    clock_thread = std::thread{[this] {
      ossia::set_thread_name("ossia midi clock");
      ossia::set_thread_pinned(ossia::thread_type::Midi, 0);

      sleep_accurate precise_sleep;

      std::chrono::steady_clock::time_point last_tick_sent{}, now{};
      // Send one now
      last_tick_sent = std::chrono::steady_clock::now();
      now = last_tick_sent;

      // if(load_state().has_clock)
      //   send_midi(0xF8);

      while(clock_thread_running.load(std::memory_order_acquire))
      {
        auto state = load_state();
        auto msg_to_send = this->next_event_midiclock.exchange(MidiStartStopEvent::None);
        if(state.has_startstop)
        {
          switch(msg_to_send)
          {
            case MidiStartStopEvent::None:
              break;
            case MidiStartStopEvent::Start:
              full_songpos_message(0.);
              send_midi(0xFA);
              if(state.has_clock)
              {
                send_midi(0xF8);
                last_tick_sent = std::chrono::steady_clock::now();
                now = last_tick_sent;
              }
              break;
            case MidiStartStopEvent::Continue:
              full_songpos_message(
                  this->current_song_pos.load(std::memory_order_acquire));
              send_midi(0xFB);
              break;
            case MidiStartStopEvent::Stop:
              full_songpos_message(0.);
              send_midi(0xFC);
              break;
          }
        }

        if(state.has_clock)
        {
          auto time_between_ticks = compute_time_between_ticks(state);
          auto elapsed_nsecs = std::chrono::duration_cast<std::chrono::nanoseconds>(
              now - last_tick_sent);

          if(elapsed_nsecs < time_between_ticks)
            precise_sleep((time_between_ticks - elapsed_nsecs).count() / 1e9);

          send_midi(0xF8);

          last_tick_sent = now;
        }
        else
        {
          std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }
      }
    }};

    // Midi Clock handling
    mtc_thread = std::thread{[this] {
      ossia::set_thread_name("ossia midi mtc");
      ossia::set_thread_pinned(ossia::thread_type::Midi, 0);

      sleep_accurate precise_sleep;

      std::chrono::steady_clock::time_point last_tick_sent{}, now{};
      // Send one now
      last_tick_sent = std::chrono::steady_clock::now();
      now = last_tick_sent;

      storage::impl state;
      int current_index = 0;
      if(state = load_state(); state.has_mtc)
        current_mtc_message(current_index, state);

      while(mtc_thread_running.load(std::memory_order_acquire))
      {
        auto new_state = load_state();
        auto msg_to_send = this->next_event_mtc.exchange(MidiStartStopEvent::None);
        if(state.has_startstop)
        {
          switch(msg_to_send)
          {
            case MidiStartStopEvent::None:
              break;
            case MidiStartStopEvent::Start:
              full_mtc_message(make_state(state.tempo, 0.));
              break;
            case MidiStartStopEvent::Continue:
              full_mtc_message(state);
              break;
            case MidiStartStopEvent::Stop:
              full_mtc_message(make_state(state.tempo, 0.));
              break;
          }
        }

        if(new_state.has_mtc)
        {
          // We don't want to change the timing in the middle
          // of packets
          if(current_index == 0)
            state = new_state;

          double frame_rate = from_mtc_framerate(state.frame_rate);
          // We send messages in quarter frames
          frame_rate *= 4.;

          auto time_between_ticks = std::chrono::nanoseconds(int64_t(1e9 / frame_rate));
          auto elapsed_nsecs = std::chrono::duration_cast<std::chrono::nanoseconds>(
              now - last_tick_sent);

          if(elapsed_nsecs < time_between_ticks)
            precise_sleep((time_between_ticks - elapsed_nsecs).count() / 1e9);

          current_mtc_message(current_index, state);

          last_tick_sent = now;
        }
        else
        {
          current_index = 0;
          state = new_state;
          std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }
      }
    }};
  }

  ~MIDISyncOut()
  {
    clock_thread_running.store(false, std::memory_order_release);
    mtc_thread_running.store(false, std::memory_order_release);
    clock_thread.join();
    mtc_thread.join();
  }

  void start()
  {
    next_event_midiclock.store(MidiStartStopEvent::Start, std::memory_order_release);
    current_song_pos.store(0., std::memory_order_release);
  }

  void stop()
  {
    if(inputs.clock_startstop == MidiStartStopMode::Enabled)
      send_midi(0xFC);

    next_event_midiclock.store(MidiStartStopEvent::Stop, std::memory_order_release);
    current_song_pos.store(0., std::memory_order_release);
  }

  void pause()
  {
    auto u = this->current_state.load(std::memory_order_acquire);
    auto state = std::bit_cast<storage::impl>(u);
    state.has_clock = false;
    state.has_mtc = false;
    this->current_state.store(std::bit_cast<uint64_t>(state), std::memory_order_release);
  }

  void resume()
  {
    next_event_midiclock.store(MidiStartStopEvent::Continue, std::memory_order_release);

    auto u = this->current_state.load(std::memory_order_acquire);
    auto state = std::bit_cast<storage::impl>(u);
    state.has_clock = inputs.clock == MidiClockMode::Enabled;
    state.has_mtc = inputs.mtc == MidiTimeCodeMode::Enabled;
    this->current_state.store(std::bit_cast<uint64_t>(state), std::memory_order_release);
  }

  void transport(auto time)
  {
    // FIXME
    /*
    auto nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(time);
    auto state = make_state(0., nsec.count() / 1e9);
    if(inputs.mtc == MidiTimeCodeMode::Emit)
      full_mtc_message(state);

    if(inputs.clock == MidiClockMode::Emit)
      full_songpos_message(state);
    */
  }

  [[nodiscard]]
  storage::impl make_state(double tempo, double total_seconds)
  {
    storage::impl state;
    state.tempo = tempo;

    total_seconds += this->inputs.offset;

    auto h = std::div((long long)total_seconds, (long long)3600).quot;
    total_seconds -= h * 3600;
    auto m = std::div((long long)total_seconds, (long long)60).quot;
    total_seconds -= m * 60;
    float s;
    auto frames = std::modf(total_seconds, &s);

    state.has_clock = inputs.clock == MidiClockMode::Enabled;
    state.has_startstop = inputs.clock_startstop == MidiStartStopMode::Enabled;
    state.has_mtc = inputs.mtc == MidiTimeCodeMode::Enabled;
    state.frame_rate
        = static_cast<std::underlying_type_t<MidiTimeCodeFrameRate>>(inputs.rate.value);
    state.h = h % 24;
    state.m = m % 60;
    state.s = std::floor(s);
    state.f = from_mtc_framerate(state.frame_rate) * frames;

    return state;
  }

  halp::setup setup;
  void prepare(halp::setup s) { setup = s; }
  void operator()(halp::tick_flicks tk)
  {
    if(setup.rate <= 0)
      return;

    if(outputs.midi.ossia_node)
    {
      auto& proto = outputs.midi.ossia_node->get_device().get_protocol();
      if(auto mp = dynamic_cast<ossia::net::midi::midi_protocol*>(&proto))
        midi_out = mp;
    }

    auto state = make_state(tk.tempo, tk.start_in_flicks / 705'600'000.);
    // FIXME midi out : mpmc for output
    // Global MTC start / stop input has to be done in a device

    handle_audio_thread_output(tk, state);

    this->current_state.store(std::bit_cast<uint64_t>(state), std::memory_order_release);
    this->current_song_pos.store(tk.start_position_in_quarters);
  }

  void
  handle_audio_thread_output(const halp::tick_flicks& tk, const storage::impl& state)
  {
    const double current_time = tk.start_in_flicks / 705'600'000.0;
    const double frame_duration
        = std::abs(tk.end_in_flicks - tk.start_in_flicks) / 705'600'000.0;

    // Handle transport start/stop/continue
    if(tk.relative_position == 0 && !main_state.transport_started)
    {
      if(inputs.clock_startstop == MidiStartStopMode::Enabled)
      {
        // Send Start message
        outputs.midi.push_back({.bytes = {0xFA}, .timestamp = 0});
        main_state.transport_started = true;
      }

      // Send initial Song Position
      if(inputs.clock == MidiClockMode::Enabled)
      {
        uint16_t pos = tk.start_position_in_quarters * 4; // Convert to 16th notes
        if(pos < 16384)
        {
          outputs.midi.push_back(
              {.bytes = {0xF2, uint8_t(pos & 0x7F), uint8_t((pos >> 7) & 0x7F)},
               .timestamp = 0});
        }
      }
    }

    // Generate MIDI Clock messages for this buffer
    if(inputs.clock == MidiClockMode::Enabled)
    {
      const double seconds_per_tick = 60.0 / (state.tempo * 24.0);

      // Calculate how many clock ticks should occur in this buffer
      double buffer_start_time = current_time;
      double buffer_end_time = current_time + frame_duration;

      // Find the next clock tick time
      double next_tick_time = main_state.last_clock_time + seconds_per_tick;

      while(next_tick_time < buffer_end_time)
      {
        if(next_tick_time >= buffer_start_time)
        {
          // Calculate sample offset within this buffer
          int64_t sample_offset
              = ((next_tick_time - buffer_start_time) / frame_duration) * tk.frames;
          sample_offset = std::clamp<int64_t>(sample_offset, 0, tk.frames - 1);

          outputs.midi.push_back(
              {.bytes = {0xF8}, // MIDI Clock
               .timestamp = sample_offset});

          main_state.clock_tick_count++;
        }

        main_state.last_clock_time = next_tick_time;
        next_tick_time += seconds_per_tick;
      }
    }

    // Generate MTC Quarter Frame messages for this buffer
    if(inputs.mtc == MidiTimeCodeMode::Enabled)
    {
      double fps = from_mtc_framerate(state.frame_rate);
      double seconds_per_quarter_frame = 1.0 / (fps * 4.0);

      double next_qf_time
          = main_state.last_mtc_quarter_frame_time + seconds_per_quarter_frame;

      while(next_qf_time < current_time + frame_duration)
      {
        if(next_qf_time >= current_time)
        {
          int64_t sample_offset
              = ((next_qf_time - current_time) / frame_duration) * tk.frames;
          sample_offset = std::clamp<int64_t>(sample_offset, 0, tk.frames - 1);

          // Generate the quarter frame message
          uint8_t qf_data = 0;
          switch(main_state.mtc_quarter_frame_index)
          {
            case 0:
              qf_data = 0x00 | (state.f & 0x0F);
              break;
            case 1:
              qf_data = 0x10 | ((state.f >> 4) & 0x01);
              break;
            case 2:
              qf_data = 0x20 | (state.s & 0x0F);
              break;
            case 3:
              qf_data = 0x30 | ((state.s >> 4) & 0x03);
              break;
            case 4:
              qf_data = 0x40 | (state.m & 0x0F);
              break;
            case 5:
              qf_data = 0x50 | ((state.m >> 4) & 0x03);
              break;
            case 6:
              qf_data = 0x60 | (state.h & 0x0F);
              break;
            case 7:
              qf_data = 0x70 | (state.frame_rate << 1) | ((state.h >> 4) & 0x01);
              break;
          }

          outputs.midi.push_back({.bytes = {0xF1, qf_data}, .timestamp = sample_offset});

          main_state.mtc_quarter_frame_index
              = (main_state.mtc_quarter_frame_index + 1) % 8;
        }

        main_state.last_mtc_quarter_frame_time = next_qf_time;
        next_qf_time += seconds_per_quarter_frame;
      }
    }
  }

  struct MainThreadState
  {
    double last_clock_time = 0.0;
    double last_mtc_quarter_frame_time = 0.0;
    int mtc_quarter_frame_index = 0;
    uint32_t clock_tick_count = 0;
    bool transport_started = false;
  } main_state;
};
}
