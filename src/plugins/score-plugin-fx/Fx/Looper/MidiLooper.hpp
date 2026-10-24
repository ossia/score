#pragma once
#include <Fx/Looper/LooperView.hpp>
#include <Fx/Looper/MidiLoop.hpp>
#include <Fx/Looper/MidiTake.hpp>
#include <Fx/Looper/Sync.hpp>
#include <Fx/Looper/Transport.hpp>
#include <Fx/Types.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/token_request.hpp>

#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>

#include <Advanced/MidiScaler/MidiCommon/Messages.hpp>

#include <functional>
#include <memory>

namespace Nodes::MidiLooper
{
struct Node
{
  halp_meta(name, "Looper (MIDI)")
  halp_meta(c_name, "midi_looper")
  halp_meta(category, "Midi/Sequencers")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-looper.html")
  halp_meta(
      description,
      "Loop notes and controllers in passes, and turn the loop into a MIDI clip of "
      "the timeline")
  halp_meta(uuid, "0c0b7d3e-5a52-4b8f-9d1f-6e3a2b8c4f71");
  halp_meta(recommended_height, 200);
  halp_flag(process_exec);

  using LoopMode = Looper::LoopMode;
  using Postaction = Looper::Postaction;
  using Restart = Looper::Restart;
  using Passthrough = Looper::Passthrough;
  using OverdubMode = Looper::OverdubMode;
  using HeldNotes = Looper::HeldNotes;

  struct ins
  {
    halp::midi_bus<"in", libremidi::message> midi;

    struct : halp::enum_t<LoopMode, "Loop">
    {
      using halp::enum_t<LoopMode, "Loop">::operator=;
      halp_meta(
          description,
          "What the looper is doing: playing the loop, recording a new one, "
          "overdubbing on top of it, or nothing.")
    } mode;

    struct : quant_selector<"Quantif">
    {
      halp_meta(
          description,
          "Waits for the next point of this grid before changing mode. Whole is "
          "a bar. None changes at once.")
    } quantif;

    struct : Looper::enum_combobox<"Passthrough", Passthrough, Passthrough::Full>
    {
      using enum_combobox::operator=;
      halp_meta(
          description,
          "Whether what is played is heard as well as the loop: never, only "
          "while recording or overdubbing, or always.")
    } passthrough;

    struct : halp::enum_t<Postaction, "Post-action">
    {
      using halp::enum_t<Postaction, "Post-action">::operator=;
      halp_meta(
          description, "What recording gives way to once it has run for Bars bars")
    } postaction;

    struct : halp::spinbox_i32<"Bars", halp::irange{0, 64, 4}>
    {
      halp_meta(
          description,
          "How long a recording runs before the post-action takes over. Zero "
          "leaves it to be ended by hand.")
    } postaction_bars;

    struct : halp::enum_t<Restart, "Restart">
    {
      using halp::enum_t<Restart, "Restart">::operator=;
      halp_meta(
          description,
          "Where the loop is read from when the mode changes: from its "
          "beginning, or, but for a recording, from where it was.")
    } restart;

    struct : halp::impulse_button<"Undo">
    {
      halp_meta(description, "Removes the last pass, or the one in progress")
    } undo;
    struct : halp::impulse_button<"Redo">
    {
      halp_meta(description, "Brings back the last pass undone")
    } redo;
    struct : halp::impulse_button<"Clear">
    {
      halp_meta(description, "Empties the loop and forgets every pass")
    } clear;

    struct : halp::enum_t<OverdubMode, "Overdub">
    {
      using halp::enum_t<OverdubMode, "Overdub">::operator=;
      halp_meta(
          description,
          "Merge keeps every pass. Replace silences a pitch of earlier passes "
          "where a pass plays it. Controllers always replace their own earlier "
          "values where a pass moves them.")
    } overdub_mode;

    struct : halp::enum_t<HeldNotes, "Held notes">
    {
      using halp::enum_t<HeldNotes, "Held notes">::operator=;
      halp_meta(
          description,
          "A note held across the end of the loop carries on from its start "
          "(Wrap), or ends with the loop (Truncate).")
    } held;

    struct : musical_duration_selector<"Grid">
    {
      halp_meta(description, "Grid the notes are pulled towards when played back")
    } grid;

    struct : halp::knob_f32<"Quantize", halp::range{0., 1., 0.}>
    {
      halp_meta(
          description,
          "How far notes are pulled towards the grid when played back. The "
          "recording keeps its own timing.")
    } strength;

    struct : halp::toggle<"Quantize ends">
    {
      halp_meta(description, "Pulls the ends of the notes to the grid as well")
    } quantize_ends;

    struct : halp::spinbox_i32<"Grace (ms)", halp::irange{0, 250, 60}>
    {
      halp_meta(
          description,
          "A note played this long before a recording or an overdub starts "
          "belongs to its first beat.")
    } grace;

    struct : halp::spinbox_i32<"Late stop (ms)", halp::irange{0, 500, 120}>
    {
      halp_meta(
          description,
          "A recording asked to stop this long after a point of the grid ends "
          "on that point.")
    } late;

    struct : halp::spinbox_i32<"Transpose", halp::irange{-48, 48, 0}>
    {
      halp_meta(description, "Semitones added to the notes of the loop")
    } transpose;

    struct : halp::spinbox_i32<"In channel", halp::irange{0, 16, 0}>
    {
      halp_meta(description, "Channel recorded; 0 records them all")
    } in_channel;

    struct : halp::spinbox_i32<"Out channel", halp::irange{0, 16, 0}>
    {
      halp_meta(description, "Channel the loop is played on; 0 keeps the recorded ones")
    } out_channel;

    struct : Looper::enum_combobox<"Sync", Looper::SyncGroup, Looper::SyncGroup::None>
    {
      using enum_combobox::operator=;
      halp_meta(
          description,
          "Loopers of one group play together: the first to record sets the "
          "loop, the loops recorded after it are a whole number of it or a "
          "half, quarter or eighth of it, in phase with it.")
    } sync;

    struct : halp::spinbox_i32<"Capture bars", halp::irange{1, 64, 4}>
    {
      halp_meta(description, "How many bars Capture takes")
    } capture_bars;

    struct : halp::impulse_button<"Capture">
    {
      halp_meta(
          description,
          "Makes a loop of the last bars played, recorded or not, and plays it "
          "at once.")
    } capture;

    struct : halp::impulse_button<"Send to timeline">
    {
      halp_meta(
          description,
          "Places the last take in the timeline, where it was recorded, as a "
          "MIDI clip.")
    } send;

    // Controls added after the first presets go last: presets address the
    // inputs by their index.
    struct : Looper::enum_combobox<"Takes", Looper::TakeFiles, Looper::TakeFiles::Mix>
    {
      using enum_combobox::operator=;
      halp_meta(
          description,
          "What a take is written as: what is heard of the loop (Mix), each "
          "pass in a MIDI file of its own (Layers), or both. Sending or dragging "
          "a take places every file of it, each on a lane of its own.")
    } takes;

    struct : Looper::enum_combobox<"Past the end", Looper::PastEnd, Looper::PastEnd::Wrap>
    {
      using enum_combobox::operator=;
      halp_meta(
          description,
          "What an overdub does at the end of the loop: go round it again "
          "(Wrap), or make the loop longer by its own length each time, the "
          "passes under it playing again (Extend). An extended loop ends when "
          "overdubbing stops.")
    } past_end;

    struct : halp::toggle<"Follow tempo", halp::default_on_toggle>
    {
      halp_meta(
          description,
          "Plays the loop at the tempo of the score: it keeps to the bars when "
          "the tempo changes. Off, it keeps the speed it was recorded at.")
    } follow_tempo;
  } inputs;

  struct
  {
    halp::midi_out_bus<"out", libremidi::message> midi;
    struct : halp::val_port<"Phase", float>
    {
      halp_meta(description, "Position in the loop, from 0 to 1")
    } phase;
  } outputs;

  struct worker_t
  {
    std::function<void(Looper::midi_take_job)> request;
    static std::function<void(Node&)> work(Looper::midi_take_job&& job)
    {
      const bool ok = job.run();
      return [slot = job.slot, number = job.number, ok](Node& self) {
        self.take_written(slot, number, ok);
      };
    }
  } worker;

  std::function<void(Looper::view_message)> send_message;

  const score::DocumentContext* ossia_document_context{};
  ossia::exec_state_facade ossia_state;

  Looper::transport transport;
  Looper::loop_sync sync;
  Looper::midi_loop loop;
  std::shared_ptr<Looper::take_writer> writer;
  std::shared_ptr<Looper::midi_export> export_data;

  Node() { export_data = std::make_shared<Looper::midi_export>(); }

  void prepare(halp::setup s)
  {
    if(ossia_document_context)
      writer = Looper::take_writer::make(*ossia_document_context, "MIDI looper", "mid");
  }

  void stop() { m_stop_requested = true; }

  int64_t looper_run(int64_t start, int64_t length);
  void looper_close_take(
      const ossia::token_request& tk, LoopMode mode, bool ended_by_postaction);
  void looper_trim_recording(int64_t frames)
  {
    const int64_t end = std::max<int64_t>(0, loop.length - frames);
    // What was played after the grid point belongs to the next pass.
    loop.note_count = int(
        std::remove_if(
            loop.notes.begin(), loop.notes.begin() + loop.note_count,
            [end](const Looper::midi_note& n) { return n.start >= end; })
        - loop.notes.begin());
    loop.control_count = int(
        std::remove_if(
            loop.controls.begin(), loop.controls.begin() + loop.control_count,
            [end](const Looper::midi_control& c) { return c.pos >= end; })
        - loop.controls.begin());
    loop.length = end;
    loop.pass_written = end;
  }

  using tick = ossia::token_request;
  void operator()(const tick& tk)
  {
    m_out_count = 0;
    m_tick_frames = ossia_state.timings(tk).length;

    if(m_stop_requested)
    {
      m_stop_requested = false;
      flush_loop(0);
      flush_input(0);
      transport.reset();
      if(loop.taking)
        loop.end_pass(inputs.overdub_mode);
    }

    if(tk.date == tk.prev_date)
    {
      push_outputs();
      return;
    }

    m_rate = ossia_state.sampleRate();
    m_tempo = tk.tempo;
    m_signature = tk.signature;
    m_in_index = 0;

    if(inputs.transpose.value != m_transpose || inputs.out_channel.value != m_out_channel)
    {
      flush_loop(0);
      m_transpose = inputs.transpose.value;
      m_out_channel = inputs.out_channel.value;
    }

    sync.set_group(inputs.sync.value);
    apply_buttons();
    if(inputs.capture.value)
      capture(tk);

    m_mode = transport.effective(inputs.mode, loop.empty());
    const Looper::transport_settings settings{
        .mode = m_mode,
        .quantif = inputs.quantif.value,
        .postaction = inputs.postaction,
        .postaction_bars = inputs.postaction_bars,
        .restart = inputs.restart,
        .late_seconds = inputs.late.value / 1000.};
    transport(tk, ossia_state, settings, *this);

    // Input past the last span, e.g. on a sub-sample tick.
    handle_input(m_tick_frames, m_tick_frames + 1, -1, false);

    run_export();
    outputs.phase.value = loop.length > 0 ? float(transport.playbackPos % loop.length)
                                                / float(loop.length)
                                          : 0.f;
    m_now += m_tick_frames;
    send_view(m_tick_frames);
    push_outputs();
  }

  struct ui
  {
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::background_dark)
    struct
    {
      halp_meta(layout, halp::layouts::vbox)
      struct
      {
        halp_meta(layout, halp::layouts::hbox)
        halp::item<&ins::mode> mode;
        halp::item<&ins::quantif> quantif;
      } transport;
      struct
      {
        halp_meta(layout, halp::layouts::hbox)
        halp::item<&ins::undo> undo;
        halp::item<&ins::redo> redo;
        halp::item<&ins::clear> clear;
        halp::item<&ins::send> send;
        halp::item<&ins::takes> takes;
      } actions;
      struct
      {
        halp_meta(name, "Settings")
        halp_meta(layout, halp::layouts::tabs)
        halp_meta(background, halp::colors::background_darker)
        struct
        {
          halp_meta(name, "Length")
          halp_meta(layout, halp::layouts::hbox)
          halp::item<&ins::postaction> postaction;
          halp::item<&ins::postaction_bars> bars;
          halp::item<&ins::late> late;
          halp::item<&ins::grace> grace;
          halp::item<&ins::sync> sync;
        } length;
        struct
        {
          halp_meta(name, "Overdub")
          halp_meta(layout, halp::layouts::hbox)
          halp::item<&ins::overdub_mode> overdub_mode;
          halp::item<&ins::held> held;
          halp::item<&ins::past_end> past_end;
          halp::item<&ins::restart> restart;
        } overdub;
        struct
        {
          halp_meta(name, "Quantize")
          halp_meta(layout, halp::layouts::hbox)
          halp::item<&ins::grid> grid;
          halp::item<&ins::strength> strength;
          halp::item<&ins::quantize_ends> ends;
          halp::item<&ins::follow_tempo> follow_tempo;
        } quantize;
        struct
        {
          halp_meta(name, "Notes")
          halp_meta(layout, halp::layouts::hbox)
          halp::item<&ins::passthrough> passthrough;
          halp::item<&ins::transpose> transpose;
          halp::item<&ins::in_channel> in_channel;
          halp::item<&ins::out_channel> out_channel;
        } notes;
        struct
        {
          halp_meta(name, "Capture")
          halp_meta(layout, halp::layouts::hbox)
          halp::item<&ins::capture> capture;
          halp::item<&ins::capture_bars> capture_bars;
        } capture;
      } settings;
    } controls;

    struct
    {
      halp_meta(layout, halp::layouts::container)
      halp_meta(background, halp::colors::background_mid)
      halp::custom_actions_item<Looper::looper_view> view;
    } view;

    struct bus
    {
      static void process_message(ui& self, Looper::view_message msg)
      {
        self.view.view.receive(msg);
      }
    };
  };

private:
  static constexpr int max_out = 1024;
  struct out_event
  {
    int64_t ts;
    libremidi::message m;
    int order;
  };
  std::array<out_event, max_out> m_out{};
  int m_out_count{};

  struct range_event
  {
    int64_t pos;
    bool on;
    int note;
  };
  std::array<range_event, 2048> m_range{};

  // Notes sounding on the output, and how many of them the loop started.
  std::array<std::array<uint16_t, 128>, 16> m_sounding{};
  std::array<std::array<uint16_t, 128>, 16> m_loop_sounding{};

  // Note-ons played just before a pass started, for the grace window.
  struct early_note
  {
    int64_t on;  //!< frame of the note-on, on m_now's count
    int64_t off; //!< frame of the note-off, -1 while held
    uint8_t channel, pitch, velocity;
  };
  std::array<early_note, 16> m_early{};
  int m_early_count{};

  double m_rate{48000.};
  double m_tempo{120.};
  ossia::time_signature m_signature{4, 4};
  int64_t m_now{};
  int64_t m_tick_frames{};
  int64_t m_expected_pos{};
  int m_loop_total{};
  std::size_t m_in_index{};
  int m_transpose{};
  int m_out_channel{};
  bool m_stop_requested{};

  bool m_export_pending{};
  bool m_writing{};
  double m_export_date{};
  //! Tempo the loop was recorded at: the loop keeps its own speed.
  double m_loop_tempo{120.};
  //! The mode asked for, as the transport takes it up.
  LoopMode m_mode{LoopMode::Stop};
  double m_job_date{};
  int m_take_slot{};
  int m_takes_written{};
  bool m_send_requested{};
  int64_t m_view_countdown{};
  Looper::view_message m_view{};

  void apply_buttons();
  void capture(const tick& tk);

  // What was played lately, recorded or not, for Capture.
  struct played
  {
    int64_t frame;
    uint8_t bytes[3];
    uint8_t size;
  };
  static constexpr int history_size = 8192;
  std::array<played, history_size> m_history{};
  int64_t m_history_count{};
  void handle_input(int64_t from, int64_t to, double pos0, bool taking, double rate = 1.);
  void record_note_on(int64_t pos, const libremidi::message& m);
  void record_note_off(int64_t pos, const libremidi::message& m);
  void play_range(double a, double b, double out0, double r);
  void advance(int64_t start, int64_t length, bool overdub);
  double rate() const noexcept;
  double m_phase{};
  int64_t m_extend_unit{};
  bool m_extended{};
  void begin_pass_grace(int64_t pos);

  struct placed_note
  {
    int64_t start;
    int64_t length;
  };
  double grid_frames() const noexcept;
  int64_t pull(int64_t pos) const noexcept;
  placed_note place(const Looper::midi_note& n, int64_t period = 0) const noexcept;

  void emit(int64_t ts, const libremidi::message& m)
  {
    if(m_out_count < max_out)
      m_out[m_out_count++] = {ts, m, 0};
  }
  void note_on(int ch, int pitch, int vel, int64_t ts, bool from_loop);
  void note_off(int ch, int pitch, int64_t ts, bool from_loop);
  void flush_loop(int64_t ts);
  void flush_input(int64_t ts);
  void push_outputs();

  void run_export();
  void take_written(int slot, int number, bool ok);
  void write_export_item(int item);
  int m_export_item{};
  int m_export_layers{};
  bool m_export_mix{};
  int m_export_number{};
  void send_view(int64_t frames);
};

inline void Node::note_on(int ch, int pitch, int vel, int64_t ts, bool from_loop)
{
  if(ch < 1 || ch > 16 || pitch < 0 || pitch > 127)
    return;
  if(from_loop)
  {
    m_loop_sounding[ch - 1][pitch]++;
    m_loop_total++;
  }
  if(m_sounding[ch - 1][pitch]++ == 0)
    emit(ts, libremidi::channel_events::note_on(ch, pitch, std::clamp(vel, 1, 127)));
}

inline void Node::note_off(int ch, int pitch, int64_t ts, bool from_loop)
{
  if(ch < 1 || ch > 16 || pitch < 0 || pitch > 127)
    return;
  if(from_loop)
  {
    if(m_loop_sounding[ch - 1][pitch] == 0)
      return;
    m_loop_sounding[ch - 1][pitch]--;
    m_loop_total--;
  }
  auto& s = m_sounding[ch - 1][pitch];
  if(s == 0)
    return;
  if(--s == 0)
    emit(ts, libremidi::channel_events::note_off(ch, pitch, 0));
}

inline void Node::flush_loop(int64_t ts)
{
  if(m_loop_total == 0)
    return;
  for(int c = 0; c < 16; c++)
    for(int p = 0; p < 128; p++)
      while(m_loop_sounding[c][p] > 0)
        note_off(c + 1, p, ts, true);
}

inline void Node::flush_input(int64_t ts)
{
  for(int c = 0; c < 16; c++)
    for(int p = 0; p < 128; p++)
      while(m_sounding[c][p] > m_loop_sounding[c][p])
        note_off(c + 1, p, ts, false);
}

inline void Node::push_outputs()
{
  // Note-offs first at a given time, so that a note ending where the same
  // note starts again is heard twice; otherwise in the order they came.
  for(int i = 0; i < m_out_count; i++)
    m_out[i].order = i;
  std::sort(m_out.begin(), m_out.begin() + m_out_count, [](const out_event& a, const out_event& b) {
    if(a.ts != b.ts)
      return a.ts < b.ts;
    const bool a_off = mtk::classify(a.m) == mtk::midi_kind::note_off;
    const bool b_off = mtk::classify(b.m) == mtk::midi_kind::note_off;
    if(a_off != b_off)
      return a_off;
    return a.order < b.order;
  });
  for(int i = 0; i < m_out_count; i++)
  {
    auto m = m_out[i].m;
    m.timestamp = std::clamp<int64_t>(m_out[i].ts, 0, std::max<int64_t>(0, m_tick_frames - 1));
    outputs.midi.push_back(std::move(m));
  }
  m_out_count = 0;
}

inline void Node::apply_buttons()
{
  if(inputs.clear.value)
  {
    sync.release();
    flush_loop(0);
    loop.clear();
    transport.playbackPos = 0;
    if(transport.is_taking())
      transport.taking(LoopMode::Stop);
  }

  if(inputs.undo.value)
  {
    flush_loop(0);
    if(transport.is_taking())
    {
      if(transport.actualMode == LoopMode::Record)
      {
        loop.clear();
        transport.playbackPos = 0;
      }
      else
      {
        loop.taking = false;
        loop.undo();
        loop.drop_redo();
      }
      transport.taking(LoopMode::Stop);
    }
    else
    {
      loop.undo();
    }
  }

  if(inputs.redo.value && !transport.is_taking())
  {
    flush_loop(0);
    loop.redo();
  }

  if(inputs.send.value)
    m_send_requested = true;
}

//! The last bars played become the loop, ending on the last grid point when
//! it was just passed, as a late stop would.
inline void Node::capture(const tick& tk)
{
  if(transport.is_taking())
    return;

  const double bar = m_rate * 4. * (double(tk.signature.upper) / tk.signature.lower)
                     * (60. / std::max(1., tk.tempo));
  int64_t end = m_now;
  if(transport.quantif > 0.f && transport.lastPointFrame)
  {
    const int64_t late = transport.frame - *transport.lastPointFrame;
    if(late >= 0 && late <= int64_t(inputs.late.value * m_rate / 1000.))
      end -= late;
  }
  const int64_t n = std::min<int64_t>(int64_t(inputs.capture_bars.value * bar), end);
  if(n <= 0)
    return;
  const int64_t from = end - n;

  flush_loop(0);
  loop.begin_recording();
  loop.length = n;
  const int64_t count = std::min<int64_t>(m_history_count, history_size);
  for(int64_t i = m_history_count - count; i < m_history_count; i++)
  {
    const auto& h = m_history[i % history_size];
    if(h.frame < from || h.frame >= end)
      continue;
    const int64_t pos = h.frame - from;
    libremidi::message m;
    if(h.size == 3)
      m = libremidi::message{h.bytes[0], h.bytes[1], h.bytes[2]};
    else if(h.size == 2)
      m = libremidi::message{h.bytes[0], h.bytes[1]};
    else
      continue;
    switch(mtk::classify(m))
    {
      case mtk::midi_kind::note_on:
        loop.add_note(pos, mtk::channel_of(m), m.bytes[1], m.bytes[2]);
        break;
      case mtk::midi_kind::note_off:
        if(auto* note = loop.open_note(mtk::channel_of(m), m.bytes[1]))
          note->length = std::max<int64_t>(1, pos - note->start);
        break;
      case mtk::midi_kind::control_change:
      case mtk::midi_kind::pitch_bend:
      case mtk::midi_kind::channel_pressure:
      case mtk::midi_kind::poly_pressure:
        loop.add_control(pos, m.bytes[0], m.bytes[1], h.size > 2 ? m.bytes[2] : 0);
        break;
      default:
        break;
    }
  }
  // Notes let go after the window, e.g. when it was moved back to a grid
  // point, end there; the ones still held end when they are let go.
  for(int64_t i = m_history_count - count; i < m_history_count; i++)
  {
    const auto& h = m_history[i % history_size];
    if(h.frame < end || h.size < 2)
      continue;
    const libremidi::message m{h.bytes[0], h.bytes[1], h.size > 2 ? h.bytes[2] : uint8_t(0)};
    if(mtk::classify(m) == mtk::midi_kind::note_off)
      if(auto* note = loop.open_note(mtk::channel_of(m), m.bytes[1]))
        note->length = std::max<int64_t>(1, h.frame - from - note->start);
  }

  loop.pass_written = n;
  loop.end_pass(inputs.overdub_mode);

  transport.play_captured((m_now - end) % n);
  m_expected_pos = transport.playbackPos;
  m_loop_tempo = tk.tempo;

  const double model_per_frame = ossia_state.samplesToModel();
  m_export_date = (tk.prev_date.impl - (m_now - from) * model_per_frame)
                  / double(ossia::flicks_per_second<int64_t>);
  m_export_pending = true;
}

inline double Node::grid_frames() const noexcept
{
  // The loop's own beats: it keeps the speed it was recorded at.
  const double tempo = loop.empty() ? m_tempo : m_loop_tempo;
  return m_rate * 60. / std::max(1., tempo) * 4. * inputs.grid.value;
}

//! A position pulled towards the grid by the strength, not wrapped.
inline int64_t Node::pull(int64_t pos) const noexcept
{
  const float strength = std::clamp(inputs.strength.value, 0.f, 1.f);
  const double grid = grid_frames();
  if(strength <= 0.f || grid < 1.)
    return pos;
  const double target = std::round(pos / grid) * grid;
  return int64_t(std::llround(pos + strength * (target - pos)));
}

//! Where a note plays, quantize applied: its start in the loop and its
//! length. A pulled end never comes before the pulled start: a note whose
//! two ends fall on one grid point keeps a grid step, or its own length when
//! it was shorter.
inline Node::placed_note
Node::place(const Looper::midi_note& n, int64_t period) const noexcept
{
  const int64_t len = std::max<int64_t>(1, period > 0 ? period : loop.length);
  const int64_t length = std::clamp<int64_t>(n.length, 1, len - 1);
  const int64_t on = pull(n.start);
  int64_t played = length;
  if(inputs.quantize_ends.value)
  {
    played = pull(n.start + length) - on;
    const int64_t shortest
        = std::max<int64_t>(1, std::min<int64_t>(length, int64_t(grid_frames())));
    played = std::clamp<int64_t>(played, shortest, len - 1);
  }
  int64_t start = on % len;
  if(start < 0)
    start += len;
  return {start, played};
}

//! The input messages of [from; to[ of the tick: recorded at pos0 and on when
//! taking in, passed through when the passthrough says so.
inline void Node::handle_input(
    int64_t from, int64_t to, double pos0, bool taking, double rate)
{
  const Passthrough pt = inputs.passthrough;
  const bool echo = taking ? pt != Passthrough::None : pt == Passthrough::Full;
  const int in_ch = inputs.in_channel.value;
  auto& msgs = inputs.midi.midi_messages;

  for(; m_in_index < msgs.size(); m_in_index++)
  {
    const auto& m = msgs[m_in_index];
    const int64_t ts = m.timestamp;
    if(ts >= to)
      return;

    const auto kind = mtk::classify(m);
    const int ch = mtk::channel_of(m);
    const bool ours = ch > 0 && (in_ch == 0 || ch == in_ch);
    if(ours && m.size() > 0)
    {
      auto& h = m_history[m_history_count++ % history_size];
      h.frame = m_now + ts;
      h.size = uint8_t(std::min<std::size_t>(3, m.size()));
      for(int b = 0; b < h.size; b++)
        h.bytes[b] = m.bytes[b];
    }

    // Where the message falls in the loop. A recording's loop is as long as
    // what was recorded so far, so its positions are not wrapped.
    int64_t pos = -1;
    if(pos0 >= 0)
    {
      pos = int64_t(pos0 + (std::max(ts, from) - from) * rate);
      if(transport.actualMode != LoopMode::Record && loop.length > 0)
        pos %= loop.length;
    }

    switch(kind)
    {
      case mtk::midi_kind::note_on:
        if(ours)
        {
          if(taking && pos >= 0)
            record_note_on(pos, m);
          else if(m_early_count < int(m_early.size()))
            m_early[m_early_count++]
                = {m_now + ts, -1, uint8_t(ch), m.bytes[1], m.bytes[2]};
          else
          {
            std::rotate(m_early.begin(), m_early.begin() + 1, m_early.end());
            m_early.back() = {m_now + ts, -1, uint8_t(ch), m.bytes[1], m.bytes[2]};
          }
        }
        if(echo)
          note_on(ch, m.bytes[1], m.bytes[2], ts, false);
        break;
      case mtk::midi_kind::note_off:
        if(ours)
        {
          record_note_off(pos, m);
          for(int i = 0; i < m_early_count; i++)
            if(m_early[i].channel == ch && m_early[i].pitch == m.bytes[1]
               && m_early[i].off < 0)
              m_early[i].off = m_now + ts;
        }
        // A note let go is let go even if the passthrough changed meanwhile.
        if(m_sounding[(ch - 1) & 15][m.bytes[1] & 127]
           > m_loop_sounding[(ch - 1) & 15][m.bytes[1] & 127])
          note_off(ch, m.bytes[1], ts, false);
        break;
      case mtk::midi_kind::control_change:
      case mtk::midi_kind::pitch_bend:
      case mtk::midi_kind::channel_pressure:
      case mtk::midi_kind::poly_pressure:
        if(ours && taking && pos >= 0)
          loop.add_control(pos, m.bytes[0], m.size() > 1 ? m.bytes[1] : 0, m.size() > 2 ? m.bytes[2] : 0);
        if(echo)
          emit(ts, m);
        break;
      default:
        if(echo)
          emit(ts, m);
        break;
    }
  }
}

inline void Node::record_note_on(int64_t pos, const libremidi::message& m)
{
  loop.add_note(pos, mtk::channel_of(m), m.bytes[1], m.bytes[2]);
}

inline void Node::record_note_off(int64_t pos, const libremidi::message& m)
{
  auto* n = loop.open_note(mtk::channel_of(m), m.bytes[1]);
  if(!n)
    return;
  if(pos < 0)
    pos = transport.playbackPos;
  if(transport.actualMode == LoopMode::Record || loop.length <= 0)
    n->length = std::max<int64_t>(1, pos - n->start);
  else
    n->length = loop.distance(n->start, pos);
}

//! Notes played just before the pass started belong to its first frame.
inline void Node::begin_pass_grace(int64_t pos)
{
  const int64_t grace = int64_t(inputs.grace.value * m_rate / 1000.);
  for(int i = 0; i < m_early_count; i++)
  {
    const auto& e = m_early[i];
    if(m_now - e.on > grace)
      continue;
    if(auto* n = loop.add_note(pos, e.channel, e.pitch, e.velocity); n && e.off >= 0)
      n->length = std::max<int64_t>(1, e.off - e.on);
  }
  m_early_count = 0;
}

inline int64_t Node::looper_run(int64_t start, int64_t length)
{
  // The playhead jumped, e.g. a mode change read the loop from its start
  // again: the notes the loop was playing would end at the wrong time.
  if(!loop.empty() && transport.playbackPos % loop.length != m_expected_pos)
  {
    flush_loop(start);
    m_phase = double(transport.playbackPos % loop.length);
  }

  const LoopMode mode = transport.actualMode;
  switch(mode)
  {
    case LoopMode::Stop:
      flush_loop(start);
      handle_input(start, start + length, -1, false);
      break;

    case LoopMode::Play:
      advance(start, length, false);
      break;

    case LoopMode::Record: {
      if(!transport.is_taking())
      {
        flush_loop(start);
        loop.begin_recording();
        transport.playbackPos = 0;
        m_export_pending = false;
        begin_pass_grace(0);
      }
      transport.taking(LoopMode::Record);
      const int64_t pos0 = loop.length;
      handle_input(start, start + length, double(pos0), true);
      loop.length += length;
      loop.pass_written = loop.length;
      transport.playbackPos = loop.length;
      m_phase = 0.;
      m_expected_pos = 0;
      return length;
    }

    case LoopMode::Overdub: {
      if(loop.empty())
      {
        handle_input(start, start + length, -1, false);
        break;
      }
      if(!transport.is_taking())
      {
        const int64_t k = int64_t(m_phase) % loop.length;
        loop.begin_overdub(k);
        begin_pass_grace(k);
        // An extending pass grows the loop by the loop it started over.
        m_extend_unit = loop.length;
        m_extended = false;
      }
      transport.taking(LoopMode::Overdub);
      advance(start, length, true);
      break;
    }
  }

  if(!loop.empty())
  {
    transport.playbackPos = int64_t(m_phase) % loop.length;
    m_expected_pos = transport.playbackPos;
  }
  return length;
}

//! How fast the loop goes for the score: with Follow tempo, at the score's
//! tempo over the loop's own.
inline double Node::rate() const noexcept
{
  if(!inputs.follow_tempo.value || loop.empty() || m_loop_tempo <= 0.)
    return 1.;
  return std::clamp(m_tempo / m_loop_tempo, 0.05, 20.);
}

//! Plays the loop over [start; start + length[ of the tick, and takes in what
//! is played when overdubbing. The loop's positions go `rate()` times as fast
//! as the tick's frames; at the loop's end, an overdub extends it or the
//! playhead goes round.
inline void Node::advance(int64_t start, int64_t length, bool overdub)
{
  const double r = rate();
  const bool extend = overdub && inputs.past_end.value == Looper::PastEnd::Extend;
  // Undoing an extension can leave the playhead past the end of the loop.
  double ph = std::fmod(std::max(0., m_phase), double(loop.length));
  double out = double(start);
  const double out_end = double(start + length);

  while(out < out_end - 1e-9)
  {
    const double L = double(loop.length);
    const double b = std::min(L, ph + (out_end - out) * r);
    const double seg_out = (b - ph) / r;

    play_range(ph, b, out, r);
    // The input of the frames this part of the loop covers.
    const int64_t from = int64_t(std::ceil(out - 1e-9));
    const int64_t to = b >= L ? int64_t(std::ceil(out + seg_out - 1e-9))
                              : int64_t(std::ceil(out_end - 1e-9));
    handle_input(from, to, ph + (from - out) * r, overdub, r);
    if(overdub)
      loop.pass_written += int64_t(b - ph);

    out += seg_out;
    ph = b;
    if(ph >= L - 1e-9)
    {
      if(extend && loop.extend(m_extend_unit))
      {
        m_extended = true;
      }
      else
      {
        ph -= L;
        if(ph < 0.)
          ph = 0.;
      }
    }
  }
  // Input on the last frame, when the span ended on the loop's end.
  if(!overdub)
    handle_input(start + length, start + length, -1, false);
  m_phase = ph;
}

//! The notes and controllers of the loop between positions a and b, sent at
//! the frames of the tick they fall on: out0 is the frame of position a.
inline void Node::play_range(double a, double b, double out0, double r)
{
  const int64_t len = loop.length;
  const auto mode = inputs.overdub_mode.value;
  const bool truncate = inputs.held.value == HeldNotes::Truncate;
  auto in_range = [&](int64_t q) { return q >= a && q < b; };
  auto frame = [&](int64_t q) { return int64_t(out0 + (q - a) / r); };

  // The note-ons and note-offs of the range, applied in time order: a note
  // starting and ending in one range must not end before it starts.
  int count = 0;
  auto add = [&](int64_t pos, bool on, int note) {
    if(count < int(m_range.size()))
      m_range[count++] = {pos, on, note};
  };
  for(int i = 0; i < loop.note_count; i++)
  {
    const auto& n = loop.notes[i];
    // A note still held on the input is heard from the input: playing it
    // again each round would start it with nothing to end it.
    if(n.length < 0 || n.pass >= loop.count)
      continue;
    // A note an overdub replaces may be sounding when it is replaced: its
    // end still goes out, the counts keep it from ending anything else.
    const bool heard = loop.heard(n, mode);

    // A pass shorter than the loop plays again over the rest of it.
    const int64_t period = std::clamp<int64_t>(loop.period_of(n.pass), 1, len);
    const auto [first, length] = place(n, period);
    for(int64_t on = first; on < len; on += period)
    {
      int64_t off = on + length;
      if(truncate && off >= len)
        off = 0; // the loop's end, which is where the next round starts
      off %= len;
      if(in_range(off))
        add(off, false, i);
      if(heard && in_range(on))
        add(on, true, i);
    }
  }
  std::sort(m_range.begin(), m_range.begin() + count, [](const range_event& a, const range_event& b) {
    if(a.pos != b.pos)
      return a.pos < b.pos;
    if(a.on != b.on)
      return !a.on;
    return a.note < b.note;
  });
  for(int i = 0; i < count; i++)
  {
    const auto& e = m_range[i];
    const auto& n = loop.notes[e.note];
    const int ch = m_out_channel > 0 ? m_out_channel : n.channel;
    const int pitch = n.pitch + m_transpose;
    if(e.on)
      note_on(ch, pitch, n.velocity, frame(e.pos), true);
    else
      note_off(ch, pitch, frame(e.pos), true);
  }

  for(int i = 0; i < loop.control_count; i++)
  {
    const auto& c = loop.controls[i];
    if(!loop.heard(c))
      continue;
    const int64_t period = std::clamp<int64_t>(loop.period_of(c.pass), 1, len);
    for(int64_t q = c.pos % period; q < len; q += period)
    {
      if(!in_range(q))
        continue;
      libremidi::message m;
      const uint8_t kind = c.status & 0xF0;
      const int ch = m_out_channel > 0 ? m_out_channel : (c.status & 0x0F) + 1;
      const uint8_t status = uint8_t(kind | ((ch - 1) & 0x0F));
      if(kind == 0xD0)
        m = libremidi::message{status, c.data1};
      else
        m = libremidi::message{status, c.data1, c.data2};
      emit(frame(q), m);
    }
  }
}

inline void Node::looper_close_take(
    const ossia::token_request& tk, LoopMode mode, bool ended_by_postaction)
{
  if(mode == LoopMode::Record)
  {
    m_loop_tempo = transport.loop_tempo(tk, loop.length);
    const double bar_samples = m_rate * 4.
                               * (double(tk.signature.upper) / tk.signature.lower)
                               * (60. / tk.tempo);
    const int64_t total = int64_t(std::floor(transport.postaction_bars * bar_samples));
    if(ended_by_postaction && transport.quantif > 0.f && transport.postaction_bars > 0
       && total > 0)
    {
      if(total < loop.length)
        looper_trim_recording(loop.length - total);
      loop.length = total;
    }
    // In a sync group, the loop fits the group's.
    const auto synced
        = sync.close(loop.length, transport.engine_frame(tk), transport.playbackPos);
    if(synced.length != loop.length)
      looper_trim_recording(std::max<int64_t>(0, loop.length - synced.length));
    loop.length = synced.length;
    if(sync.current())
      transport.playbackPos = synced.position % std::max<int64_t>(1, loop.length);
    loop.pass_written = loop.length;
    m_export_date = transport.recordStartDate.impl / double(ossia::flicks_per_second<int64_t>);
  }
  else if(m_extended && m_extend_unit > 0)
  {
    // An extending pass asked to stop just after the loop grew ends on the
    // old end, as a late stop would.
    const int64_t into_last = transport.playbackPos - (loop.length - m_extend_unit);
    if(into_last >= 0 && into_last <= int64_t(inputs.late.value * m_rate / 1000.)
       && loop.length - m_extend_unit >= m_extend_unit)
    {
      const int64_t end = loop.length - m_extend_unit;
      const int pass = loop.count - 1;
      loop.note_count = int(
          std::remove_if(
              loop.notes.begin(), loop.notes.begin() + loop.note_count,
              [=](const Looper::midi_note& n) { return n.pass == pass && n.start >= end; })
          - loop.notes.begin());
      loop.control_count = int(
          std::remove_if(
              loop.controls.begin(), loop.controls.begin() + loop.control_count,
              [=](const Looper::midi_control& c) { return c.pass == pass && c.pos >= end; })
          - loop.controls.begin());
      loop.length = end;
      transport.playbackPos = into_last;
      m_phase = double(into_last);
      m_expected_pos = into_last;
    }
  }
  m_extended = false;
  if(loop.count > 0)
    loop.loop_after[loop.count - 1] = loop.length;
  loop.end_pass(inputs.overdub_mode);
  m_export_pending = true;
}

inline void Node::run_export()
{
  if(m_export_pending && !m_writing && writer && !loop.empty())
  {
    m_export_pending = false;
    // A take closing while this one is written must not change its date.
    m_job_date = m_export_date;
    const auto files = inputs.takes.value;
    m_export_mix = files != Looper::TakeFiles::Layers;
    m_export_layers = files != Looper::TakeFiles::Mix ? loop.count : 0;
    m_export_number = 0;
    write_export_item(m_export_mix ? -1 : 0);
  }
}

//! Writes one file of the take: what is heard of the loop (item -1), or what
//! is heard of one of its passes.
inline void Node::write_export_item(int item)
{
  m_export_item = item;
  m_writing = true;

  auto& e = *export_data;
  e.note_count = 0;
  e.control_count = 0;
  const auto mode = inputs.overdub_mode.value;
  for(int i = 0; i < loop.note_count; i++)
  {
    auto n = loop.notes[i];
    if(!loop.heard(n, mode) || (item >= 0 && n.pass != item))
      continue;
    if(n.length < 0)
      n.length = loop.distance(n.start, transport.playbackPos % loop.length);
    // A pass shorter than the loop is in the file as many times as it plays.
    const int64_t period = std::clamp<int64_t>(loop.period_of(n.pass), 1, loop.length);
    const auto placed = place(n, period);
    for(int64_t on = placed.start; on < loop.length; on += period)
    {
      if(e.note_count >= Looper::midi_loop::max_notes)
        break;
      n.start = on;
      n.length = placed.length;
      e.notes[e.note_count++] = n;
    }
  }
  for(int i = 0; i < loop.control_count; i++)
  {
    const auto& c = loop.controls[i];
    if(!loop.heard(c) || (item >= 0 && c.pass != item))
      continue;
    const int64_t period = std::max<int64_t>(1, std::min<int64_t>(loop.period_of(c.pass), loop.length));
    for(int64_t q = c.pos % period; q < loop.length; q += period)
    {
      if(e.control_count >= Looper::midi_loop::max_controls)
        break;
      e.controls[e.control_count] = c;
      e.controls[e.control_count++].pos = q;
    }
  }
  e.length = loop.length;
  e.rate = m_rate;
  e.tempo = m_loop_tempo;
  e.meter_numerator = m_signature.upper;
  e.meter_denominator = m_signature.lower;
  e.wrap = inputs.held.value == HeldNotes::Wrap;

  Looper::midi_take_job job;
  job.writer = writer;
  job.data = export_data;
  job.slot = m_take_slot;
  job.number = m_export_number;
  job.layer = item;
  worker.request(std::move(job));
}

inline void Node::take_written(int slot, int number, bool ok)
{
  m_writing = false;
  if(!ok || !writer)
    return;

  m_export_number = number;
  if(m_export_item + 1 < m_export_layers)
  {
    write_export_item(m_export_item + 1);
    return;
  }

  m_takes_written++;
  m_view.set_take(writer->take_base(slot), "mid", m_export_mix, m_export_layers);
  m_view.take = m_takes_written;
  m_view.take_date = m_job_date;
  m_take_slot = (slot + 1) % Looper::take_writer::slots;
  m_view_countdown = 0;
}

inline void Node::send_view(int64_t frames)
{
  if(!send_message)
    return;

  if(m_send_requested && m_view.take > 0)
  {
    m_send_requested = false;
    m_view.send_serial++;
    m_view_countdown = 0;
  }

  m_view_countdown -= frames;
  if(m_view_countdown > 0)
    return;
  m_view_countdown = int64_t(m_rate / 30.);

  auto& v = m_view;
  v.midi = true;
  v.empty = loop.empty();
  v.mode = int(transport.actualMode);
  v.requested = int(transport.quantizedPlayMode);
  v.wanted = int(m_mode);
  v.layers = loop.count;
  v.redo = loop.total - loop.count;
  v.lanes = std::min(loop.count, Looper::view_message::max_lanes);
  v.full = loop.full;
  v.length_seconds = float(loop.length / m_rate);
  v.max_seconds = 0.f;
  v.position = loop.length > 0
                   ? float(transport.playbackPos % loop.length) / float(loop.length)
                   : 0.f;
  v.tempo = float(m_tempo);
  v.writing = m_writing;

  int shown = 0;
  const auto mode = inputs.overdub_mode.value;
  const int64_t len = std::max<int64_t>(1, loop.length);
  for(int i = 0; i < loop.note_count && shown < Looper::view_message::max_notes; i++)
  {
    const auto& n = loop.notes[i];
    if(!loop.heard(n, mode))
      continue;
    const int64_t l = n.length < 0 ? loop.distance(n.start, transport.playbackPos % len)
                                   : std::min(n.length, len - 1);
    // A pass shorter than the loop is drawn each time it plays.
    const int64_t period = std::max<int64_t>(1, std::min<int64_t>(loop.period_of(n.pass), len));
    for(int64_t on = n.start % period; on < len && shown < Looper::view_message::max_notes;
        on += period)
    {
      auto& d = v.note[shown++];
      d.start = float(on) / float(len);
      d.end = float((on + l) % len) / float(len);
      d.pitch = n.pitch;
      d.lane = uint8_t(n.pass % Looper::view_message::max_lanes);
      d.velocity = n.velocity;
    }
  }
  v.notes = shown;
  send_message(v);
}
}
