#pragma once
#include <Fx/Looper/LooperNode.hpp>
#include <Fx/Looper/MidiLoop.hpp>
#include <Fx/Looper/MidiTake.hpp>
#include <Fx/Types.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/token_request.hpp>

#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>

#include <Advanced/MidiScaler/MidiCommon/Messages.hpp>

#include <functional>
#include <limits>
#include <memory>

namespace Nodes::MidiLooper
{
struct Node : Looper::looper_node<Node>
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

  //! Inlets that presets which do not name their controls count in their
  //! indices, and that the looper does not have: Sync.
  static constexpr std::array<int, 1> removed_inlets() noexcept { return {20}; }

  using LoopMode = Looper::LoopMode;
  using Passthrough = Looper::Passthrough;
  using OverdubMode = Looper::OverdubMode;
  using HeldNotes = Looper::HeldNotes;

  struct ins
  {
    halp::midi_bus<"in", libremidi::message> midi;

    struct : halp::enum_t<LoopMode, "Mode">
    {
      using halp::enum_t<LoopMode, "Mode">::operator=;
      static constexpr std::string_view former_name() noexcept { return "Loop"; }
      halp_meta(
          description,
          "What the looper is doing, or about to do on the grid: playing the "
          "loop, recording it, overdubbing on top of it, or nothing. It follows "
          "every change, by an action or a recording's end. Record on a loop "
          "overdubs it: Clear it first to record a new one. Overdub on an "
          "empty loop records it, then overdubs.")
      //! Shows the mode actions and a recording's end change it to.
      std::function<void(std::string_view)> update_controller;
      //! A value came in, from the UI or a cable.
      void update(Node& n) noexcept { n.loop_arrived(); }
    } mode;

    Looper::quantif_selector quantif;

    struct : Looper::enum_combobox<"Passthrough", Passthrough, Passthrough::Full>
    {
      using enum_combobox::operator=;
      halp_meta(
          description,
          "Whether what is played is heard as well as the loop: never, only "
          "while recording or overdubbing, or always.")
    } passthrough;

    Looper::after_record_selector after_record;

    struct : halp::spinbox_i32<"Record bars", halp::irange{0, 64, 4}>
    {
      static constexpr std::string_view former_name() noexcept { return "Bars"; }
      halp_meta(
          description,
          "How long a recording runs before After record takes over. Zero "
          "leaves it to be ended by hand.")
    } postaction_bars;

    Looper::start_from_selector start_from;

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

    struct : halp::enum_t<OverdubMode, "Overdub mode">
    {
      using halp::enum_t<OverdubMode, "Overdub mode">::operator=;
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

    Looper::note_grid_selector grid;

    struct : halp::knob_f32<"Strength", halp::range{0., 1., 0.}>
    {
      static constexpr std::string_view former_name() noexcept { return "Quantize"; }
      halp_meta(
          description,
          "How far notes are pulled towards Note grid when played back: 0 "
          "leaves them where they were played, 1 puts them on the grid. The "
          "recording keeps its own timing.")
    } strength;

    struct : halp::toggle<"Note ends">
    {
      static constexpr std::string_view former_name() noexcept { return "Quantize ends"; }
      halp_meta(description, "Pulls the ends of the notes to Note grid as well")
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

    // Controls added after the first presets go last: presets address the
    // inputs by their index.
    struct : Looper::enum_combobox<"Export", Looper::TakeFiles, Looper::TakeFiles::Mix>
    {
      using enum_combobox::operator=;
      static constexpr std::string_view former_name() noexcept { return "Takes"; }
      halp_meta(
          description,
          "What a take writes: what is heard of the loop (Mix), each pass in a "
          "MIDI file of its own (Layers), or both. Sending or dragging a take "
          "places every file of it, each on a lane of its own.")
    } exported;

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

    Looper::erase_layer_button erase_layer;

    struct : halp::spinbox_i32<"Layer", halp::irange{1, 32, 1}>
    {
      halp_meta(
          description,
          "The layer Mute layer, Erase layer and Solo layer act on, counting "
          "from the oldest one: the lane of that number from the top while "
          "there are no more layers than lanes, and the file of that number of "
          "a take written as layers. It moves to each new layer as it is "
          "added.")
      std::function<void(int)> update_controller;
    } layer;

    struct : halp::toggle<"Reverse">
    {
      halp_meta(
          description,
          "Plays the loop backwards, from where the playhead is: notes start "
          "where they ended, controllers go back the way they came. An overdub "
          "records where the playhead goes, as on tape: what is played over "
          "the reversed loop plays reversed once it goes forwards again.")
      std::function<void(bool)> update_controller;
    } reverse;

    struct : Looper::speed_selector<"Speed">
    {
      halp_meta(
          description,
          "Plays the loop slower or faster. An overdub records where the "
          "playhead goes: what is played at x1/2 plays twice as fast at x1. "
          "The grid, the bars and the loop's length stay in the score's time; "
          "combines with Follow tempo.")
      std::function<void(float)> update_controller;
    } speed;

    struct : Looper::enum_combobox<"Passes", Looper::Passes, Looper::Passes::Whole>
    {
      using enum_combobox::operator=;
      struct range
      {
        std::string_view values[2] = {"Whole", "Per cycle"};
        Looper::Passes init = Looper::Passes::Whole;
      };
      halp_meta(
          description,
          "What an overdub that goes round the loop several times leaves: one "
          "layer (Whole), or a layer for each time round (Per cycle), cut on "
          "the loop's start, to keep the best one with Solo layer or Mute "
          "layer, erase the others, or splice them in the timeline from a take "
          "written as layers. With Past the end on Extend, a cut is made each "
          "time the loop grows.")
    } passes;

    Looper::mute_layer_button mute_layer;

    // The Switches tab: actions for footswitches.
    Looper::rec_play_dub_button rec_play_dub;
    Looper::after_overdub_selector after_overdub;
    Looper::play_stop_button play_stop;
    Looper::stop_at_selector stop_at;
    Looper::record_button record;
    Looper::overdub_button overdub;
    Looper::new_layer_button new_layer;
    Looper::play_button play;
    Looper::stop_button stop;
    Looper::retrigger_button retrigger;
    Looper::previous_layer_button previous_layer;
    Looper::next_layer_button next_layer;
    Looper::solo_layer_button solo_layer;
    Looper::reverse_button reverse_toggle;
    Looper::half_speed_button half_speed;
    Looper::double_speed_button double_speed;
    Looper::at_max_layers_selector at_max_layers;

    struct : halp::spinbox_i32<"Max layers", halp::irange{2, Looper::midi_loop::max_layers, 8}>
    {
      halp_meta(
          description,
          "Layers kept apart, for undo, mute, solo and erase. What a pass does "
          "past it is what At max layers says.")
    } max_layers;
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
      return [slot = job.slot, number = job.number, alone = job.alone, ok](Node& self) {
        self.take_written(slot, number, ok, alone);
      };
    }
  } worker;

  std::function<void(Looper::view_message)> send_message;

  //! From the view: applied at the start of the next tick.
  void process_message(const Looper::erase_request& r) { view_request(r); }

  const score::DocumentContext* ossia_document_context{};
  ossia::exec_state_facade ossia_state;

  Looper::midi_loop loop;
  std::shared_ptr<Looper::take_writer> writer;
  std::shared_ptr<Looper::midi_export> export_data;

  Node()
  {
    export_data = std::make_shared<Looper::midi_export>();
    stop_data = std::make_shared<Looper::midi_export>();
  }
  //! What the take written when the score stops is made of: the node may be
  //! gone before it is written.
  std::shared_ptr<Looper::midi_export> stop_data;

  void prepare(halp::setup s)
  {
    if(ossia_document_context)
      writer = Looper::take_writer::make(*ossia_document_context, "MIDI looper", "mid");
  }

  //! The notes the loop plays end on the next tick, which the score stopping
  //! still runs.
  void stop() { m_stop_requested = true; }

  int64_t looper_run(int64_t start, int64_t length);
  void looper_trim_recording(int64_t frames)
  {
    const int64_t end = std::max<int64_t>(0, loop.length - frames);
    // What was played after the grid point belongs to the next pass: it is
    // kept aside, from the grid point on, for an overdub that starts there.
    const int64_t cut = loop.length - end;
    for(int i = 0; i < m_carried_note_count; i++)
      m_carried_notes[i].start += cut;
    for(int i = 0; i < m_carried_control_count; i++)
      m_carried_controls[i].pos += cut;
    m_carried_span += cut;
    auto carry_note = [this, end](const Looper::midi_note& n) {
      if(n.start < end)
        return false;
      if(m_carried_note_count < int(m_carried_notes.size()))
      {
        auto& c = m_carried_notes[m_carried_note_count++];
        c = n;
        c.start -= end;
      }
      return true;
    };
    auto carry_control = [this, end](const Looper::midi_control& c) {
      if(c.pos < end)
        return false;
      if(m_carried_control_count < int(m_carried_controls.size()))
      {
        auto& k = m_carried_controls[m_carried_control_count++];
        k = c;
        k.pos -= end;
      }
      return true;
    };
    loop.note_count = int(
        std::remove_if(
            loop.notes.begin(), loop.notes.begin() + loop.note_count, carry_note)
        - loop.notes.begin());
    loop.control_count = int(
        std::remove_if(
            loop.controls.begin(), loop.controls.begin() + loop.control_count,
            carry_control)
        - loop.controls.begin());
    loop.length = end;
    loop.pass_written = end;
    loop.edits++;
  }
  //! Longest loop an extending overdub makes, as the audio looper's longest.
  static constexpr double max_loop_seconds = 3600.;

  void operator()(const tick& tk)
  {
    m_out_count = 0;
    m_tick_frames = ossia_state.timings(tk).length;
    if(std::exchange(m_stop_requested, false))
      stop_score();
    if(tk.date == tk.prev_date)
    {
      push_outputs();
      return;
    }
    run_tick(tk);
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
        halp_meta(layout, halp::layouts::vbox)
        Looper::item_in_columns<&ins::mode, 4> mode;
        struct
        {
          halp_meta(layout, halp::layouts::hbox)
          halp::item<&ins::quantif> quantif;
          halp::item<&ins::exported> exported;
        } timing;
      } transport;
      struct
      {
        halp_meta(name, "Settings")
        halp_meta(layout, halp::layouts::tabs)
        halp_meta(background, halp::colors::background_darker)
        struct
        {
          halp_meta(name, "Length")
          halp_meta(layout, halp::layouts::grid)
          halp_meta(alignment, halp::alignments::left)
          halp_meta(spacing, 6)
          halp_meta(columns, 3)
          halp::item<&ins::after_record> after_record;
          halp::item<&ins::postaction_bars> bars;
          halp::item<&ins::late> late;
          halp::item<&ins::grace> grace;
        } length;
        struct
        {
          halp_meta(name, "Play")
          halp_meta(layout, halp::layouts::grid)
          halp_meta(alignment, halp::alignments::left)
          halp_meta(spacing, 6)
          halp_meta(columns, 2)
          halp::item<&ins::start_from> start_from;
          halp::item<&ins::speed> speed;
          halp::item<&ins::follow_tempo> follow_tempo;
          halp::item<&ins::reverse> reverse;
        } play;
        Looper::switches_tab<ins> switches;
        struct
        {
          halp_meta(name, "Overdub")
          halp_meta(layout, halp::layouts::grid)
          halp_meta(alignment, halp::alignments::left)
          halp_meta(spacing, 6)
          halp_meta(columns, 3)
          halp::item<&ins::overdub_mode> overdub_mode;
          halp::item<&ins::held> held;
          halp::item<&ins::past_end> past_end;
          halp::item<&ins::passes> passes;
          halp::item<&ins::max_layers> max_layers;
          halp::item<&ins::at_max_layers> at_max_layers;
        } overdub;
        struct
        {
          halp_meta(name, "Note grid")
          halp_meta(layout, halp::layouts::hbox)
          halp::item<&ins::grid> grid;
          halp::item<&ins::strength> strength;
          halp::item<&ins::quantize_ends> ends;
        } quantize;
        struct
        {
          halp_meta(name, "Notes")
          halp_meta(layout, halp::layouts::grid)
          halp_meta(alignment, halp::alignments::left)
          halp_meta(spacing, 6)
          halp_meta(columns, 2)
          halp::item<&ins::passthrough> passthrough;
          halp::item<&ins::transpose> transpose;
          halp::item<&ins::in_channel> in_channel;
          halp::item<&ins::out_channel> out_channel;
        } notes;
        struct
        {
          halp_meta(name, "Capture")
          halp_meta(layout, halp::layouts::hbox)
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
      void init(ui& self)
      {
        self.view.view.erase = [this](Looper::erase_request r) {
          if(send_message)
            send_message(r);
        };
      }
      static void process_message(ui& self, Looper::view_message msg)
      {
        self.view.view.receive(msg);
      }
      std::function<void(Looper::erase_request)> send_message;
    };
  };

private:
  friend struct Looper::looper_node<Node>;

  // looper_node's hooks
  Looper::midi_loop& stack() noexcept { return loop; }
  const Looper::midi_loop& stack() const noexcept { return loop; }
  //! The notes of the loop end: the ones of a pass that goes would be left
  //! sounding, and the ones it replaced would start in the middle.
  void layers_changing() { flush_loop(0); }
  bool busy() const noexcept { return false; }
  void cleared() noexcept { }
  void pass_dropped() noexcept { loop.taking = false; }
  void begin_overdub(int64_t start, bool oldest, int replaced)
  {
    loop.begin_overdub(start, max_layers(), oldest, replaced);
  }
  bool can_merge_newest() const noexcept { return !loop.taking; }
  void merge_newest() noexcept { loop.merge(loop.count - 2, true); }
  bool pass_closing() noexcept { return true; }
  //! What it went over is replaced where it played the same pitch or moved
  //! the same controller.
  void pass_closed() { loop.end_pass(inputs.overdub_mode); }
  void tick_started(const tick&)
  {
    m_in_index = 0;
    if(inputs.transpose.value != m_transpose || inputs.out_channel.value != m_out_channel)
    {
      flush_loop(0);
      m_transpose = inputs.transpose.value;
      m_out_channel = inputs.out_channel.value;
    }
  }
  void prepare_loop() noexcept { }
  void forget_carried() noexcept
  {
    m_carried_note_count = 0;
    m_carried_control_count = 0;
    m_carried_span = 0;
  }
  void tick_finished(const tick&)
  {
    // Input past the last span, e.g. on a sub-sample tick.
    handle_input(m_tick_frames, m_tick_frames + 1, -1, false);
  }
  void close_recording(int64_t bars)
  {
    if(bars > 0)
      loop.length = bars;
    loop.pass_written = loop.length;
    loop.end_pass(inputs.overdub_mode);
  }
  void close_overdub()
  {
    if(loop.pass_backwards && loop.taking && !m_cycle_pending)
      close_backwards(int64_t(std::ceil(m_phase - 1e-9)));
    close_pass();
  }
  bool cut_after(int64_t end) const noexcept { return loop.taking && loop.pass_start >= end; }
  void cut_pass_dropped() noexcept { }
  void trim_top_pass(int64_t end);
  void head_moved(int64_t pos) noexcept
  {
    m_phase = double(pos);
    m_expected_pos = pos;
  }
  double head_y(int64_t len) const noexcept
  {
    const double l = double(len);
    const double ph = std::fmod(std::max(0., m_phase), l);
    double y = m_reverse ? l - ph : ph;
    if(y >= l)
      y -= l;
    return y;
  }
  double rate_for(LoopMode) const noexcept { return rate(); }
  bool by_quarters() const noexcept { return inputs.follow_tempo.value && m_loop_tempo > 0.; }
  //! The run that follows sees the playhead move, and starts from there.
  void set_head(double y, int64_t len) noexcept
  {
    double h = m_reverse ? double(len) - y : y;
    if(h >= double(len))
      h -= double(len);
    transport.playbackPos = std::clamp<int64_t>(int64_t(h), 0, len - 1);
  }
  int64_t capture_clock() const noexcept { return m_now; }
  int64_t capture_limit() const noexcept { return std::numeric_limits<int64_t>::max(); }
  void capture_into(int64_t end, int64_t n);
  void captured() noexcept { m_expected_pos = transport.playbackPos; }
  void stopping()
  {
    flush_loop(0);
    flush_input(0);
  }
  void close_on_stop(bool recording)
  {
    if(recording)
      loop.end_pass(inputs.overdub_mode);
    else
      close_pass();
  }
  bool export_ready() const noexcept { return true; }
  void start_export(int passes, bool open, bool recording);
  void continue_export() noexcept { }
  void export_item(int item) { write_export_item(item); }
  bool take_cancelled() noexcept { return false; }
  void take_released() noexcept { }
  void stop_take_written() noexcept { }
  static constexpr const char* take_extension = "mid";
  //! A MIDI clip follows the tempo by its beats.
  static constexpr halp::clip_stretch take_stretch() noexcept
  {
    return halp::clip_stretch::automatic;
  }
  bool view_full() const noexcept { return loop.full; }
  bool view_writing() const noexcept { return m_writing; }

  static constexpr int max_out = 1024;
  struct out_event
  {
    int64_t ts;
    libremidi::message m;
    int order;
  };
  std::array<out_event, max_out> m_out{};
  int m_out_count{};

  //! A note-on of the loop in the range played, and where its note ends.
  struct range_event
  {
    int64_t pos;
    int64_t end;
    int note;
  };
  std::array<range_event, 2048> m_range{};

  // Notes sounding on the output, and how many of them the loop started.
  std::array<std::array<uint16_t, 128>, 16> m_sounding{};
  std::array<std::array<uint16_t, 128>, 16> m_loop_sounding{};

  //! A note of the loop sounding on the output. Where it ends is fixed when
  //! it starts: Strength, Note grid or Note ends changed while it sounds move
  //! its next rounds, not this one, so it neither loses its note-off nor
  //! starts twice.
  struct playing_note
  {
    //! Position it ends at, on the round being played: past the loop's end
    //! when it ends in the next one.
    int64_t end;
    //! The note of the loop: its index, and what tells it from another one
    //! that came to that index since.
    int64_t start;
    int note;
    uint8_t note_channel, note_pitch;
    //! As it went out.
    uint8_t channel, pitch;
  };
  static constexpr int max_playing = 512;
  //! By end.
  std::array<playing_note, max_playing> m_playing{};
  int m_playing_count{};

  // Note-ons played just before a pass started, for the grace window.
  struct early_note
  {
    int64_t on;  //!< frame of the note-on, on m_now's count
    int64_t off; //!< frame of the note-off, -1 while held
    uint8_t channel, pitch, velocity;
  };
  std::array<early_note, 16> m_early{};
  int m_early_count{};

  // What a recording cut back to a grid point had recorded past it, from the
  // grid point on, and how many frames were cut.
  std::array<Looper::midi_note, 128> m_carried_notes{};
  std::array<Looper::midi_control, 1024> m_carried_controls{};
  int m_carried_note_count{};
  int m_carried_control_count{};
  int64_t m_carried_span{};

  int64_t m_now{};
  int64_t m_tick_frames{};
  int64_t m_expected_pos{};
  int m_loop_total{};
  std::size_t m_in_index{};
  int m_transpose{};
  int m_out_channel{};
  bool m_stop_requested{};

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
  void handle_input(
      int64_t from, int64_t to, double pos0, bool taking, double rate = 1.,
      bool backwards = false);
  void record_note_on(int64_t pos, const libremidi::message& m);
  void record_note_off(int64_t pos, const libremidi::message& m);
  void play_range(double a, double b, double out0, double r);
  void play_range_reverse(double a, double b, double out0, double r);
  void update_reverse_controls();
  //! Controllers heard, by what they move then where they are, and for each
  //! the one before it: played backwards, the value a controller had before
  //! is the one it goes back to.
  std::array<uint16_t, Looper::midi_loop::max_controls> m_ctl_sorted{};
  std::array<int16_t, Looper::midi_loop::max_controls> m_ctl_prev{};
  std::array<int64_t, 6> m_ctl_signature{-1};
  void next_cycle_pass(int64_t start);

  // What an overdub last set each controller it moved to. Backwards, a value
  // holds for the positions under the one it was set at: the loop keeps, at
  // that position, the value from before, which is what it goes back to
  // going forwards; and where the playhead stops going backwards, the last
  // value set.
  struct last_control
  {
    uint16_t key;
    uint8_t status, data1, data2;
  };
  std::array<last_control, Looper::midi_loop::max_touched> m_last_ctl{};
  int m_last_ctl_count{};
  void record_control(int64_t pos, const libremidi::message& m, bool backwards);
  void close_backwards(int64_t boundary);
  int max_layers() const noexcept
  {
    return std::clamp(inputs.max_layers.value, 2, Looper::midi_loop::max_layers);
  }
  //! The playhead is m_phase, on the loop's own positions.
  bool m_was_reverse{};
  void advance(int64_t start, int64_t length, bool overdub);
  double rate() const noexcept;
  double m_phase{};
  void begin_pass_grace(int64_t pos);
  void begin_pass_carried(int64_t k);

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
  bool playing(int note) const noexcept;
  void start_playing(int note, int64_t end, int64_t ts);
  template <typename Frame>
  void end_playing(int64_t last, int64_t lo, Frame frame);
  void wrap_playing(int64_t len, int64_t ts);
  template <typename Frame>
  void play_notes(int count, int64_t lo, int64_t hi, Frame frame);
  void flush_input(int64_t ts);
  void push_outputs();

  void write_export_item(int item);
  void fill_export(
      const Looper::midi_loop& snap, int item, int64_t pos, Looper::midi_export& e) const;
  void write_stop_take(bool recording, double date, int64_t pos);
  //! The loop as it was when the take closed, which every file of the take
  //! is made from: the loop changes while they are written.
  Looper::midi_loop m_export_loop;
  int64_t m_export_pos{};
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
  m_playing_count = 0;
  if(m_loop_total == 0)
    return;
  for(int c = 0; c < 16; c++)
    for(int p = 0; p < 128; p++)
      while(m_loop_sounding[c][p] > 0)
        note_off(c + 1, p, ts, true);
}

inline bool Node::playing(int note) const noexcept
{
  const auto& n = loop.notes[note];
  for(int k = 0; k < m_playing_count; k++)
  {
    const auto& p = m_playing[k];
    if(p.note == note && p.start == n.start && p.note_pitch == n.pitch
       && p.note_channel == n.channel)
      return true;
  }
  return false;
}

//! Note `note` of the loop starts, unless it still sounds: it ends at `end`.
inline void Node::start_playing(int note, int64_t end, int64_t ts)
{
  if(m_playing_count >= max_playing || playing(note))
    return;
  const auto& n = loop.notes[note];
  const int ch = m_out_channel > 0 ? m_out_channel : n.channel;
  const int pitch = n.pitch + m_transpose;
  if(ch < 1 || ch > 16 || pitch < 0 || pitch > 127)
    return;

  auto it = std::upper_bound(
      m_playing.begin(), m_playing.begin() + m_playing_count, end,
      [](int64_t e, const playing_note& p) { return e < p.end; });
  std::move_backward(it, m_playing.begin() + m_playing_count, m_playing.begin() + m_playing_count + 1);
  *it = playing_note{
      .end = end,
      .start = n.start,
      .note = note,
      .note_channel = n.channel,
      .note_pitch = n.pitch,
      .channel = uint8_t(ch),
      .pitch = uint8_t(pitch)};
  m_playing_count++;
  note_on(ch, pitch, n.velocity, ts, true);
}

//! The notes of the loop ending at `last` or before end, at the frame of
//! where they end; one that should have ended before the range, at `lo`.
template <typename Frame>
inline void Node::end_playing(int64_t last, int64_t lo, Frame frame)
{
  int k = 0;
  for(; k < m_playing_count && m_playing[k].end <= last; k++)
  {
    const auto& p = m_playing[k];
    note_off(p.channel, p.pitch, frame(std::max(p.end, lo)), true);
  }
  if(k == 0)
    return;
  std::move(m_playing.begin() + k, m_playing.begin() + m_playing_count, m_playing.begin());
  m_playing_count -= k;
}

//! The playhead goes round: the notes still sounding end in the new round.
//! No note is as long as the loop, so one ends before the end of that round.
inline void Node::wrap_playing(int64_t len, int64_t ts)
{
  for(int k = 0; k < m_playing_count; k++)
    m_playing[k].end = std::min(m_playing[k].end - len, len - 1);
  // A note of the loop counted as sounding with none playing would mute its
  // key for good: every later note-on of it would be a second one.
  if(m_playing_count == 0 && m_loop_total != 0)
    flush_loop(ts);
}

//! The note-ons of [lo; hi[ gathered in m_range, applied in time order with
//! the note-offs of the notes playing: at one position, the note-offs first,
//! so that a note ending where it starts again is heard twice.
template <typename Frame>
inline void Node::play_notes(int count, int64_t lo, int64_t hi, Frame frame)
{
  std::sort(m_range.begin(), m_range.begin() + count, [](const range_event& a, const range_event& b) {
    if(a.pos != b.pos)
      return a.pos < b.pos;
    return a.note < b.note;
  });
  for(int i = 0; i < count; i++)
  {
    const auto& e = m_range[i];
    end_playing(e.pos, lo, frame);
    start_playing(e.note, e.end, frame(e.pos));
  }
  end_playing(hi - 1, lo, frame);
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

//! Capture: what was played over the `n` frames up to `end` becomes the loop.
inline void Node::capture_into(int64_t end, int64_t n)
{
  const int64_t from = end - n;

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
}

inline double Node::grid_frames() const noexcept
{
  // The loop's own beats: it keeps the speed it was recorded at.
  const double tempo = loop.empty() ? m_tempo : m_loop_tempo;
  return m_rate * 60. / std::max(1., tempo) * 4. * inputs.grid.value.value;
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
    int64_t from, int64_t to, double pos0, bool taking, double rate, bool backwards)
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
    // Backwards, pos0 counts from the loop's end, and a message falls on the
    // edge of the position the playhead has just gone over: a note played
    // there starts on it when the loop goes forwards again.
    int64_t pos = -1;
    if(pos0 >= 0)
    {
      pos = int64_t(pos0 + (std::max(ts, from) - from) * rate);
      if(backwards && loop.length > 0)
      {
        pos = (loop.length - pos) % loop.length;
        if(pos < 0)
          pos += loop.length;
      }
      else if(transport.actualMode != LoopMode::Record && loop.length > 0)
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
          record_control(pos, m, backwards);
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

inline void Node::record_control(int64_t pos, const libremidi::message& m, bool backwards)
{
  const Looper::midi_control c{
      .pos = pos,
      .status = m.bytes[0],
      .data1 = m.size() > 1 ? m.bytes[1] : uint8_t(0),
      .data2 = m.size() > 2 ? m.bytes[2] : uint8_t(0),
      .pass = 0,
      .replaced_by = 0,
      .merged = 0};
  const uint16_t key = c.key();
  last_control* last = nullptr;
  for(int i = 0; i < m_last_ctl_count; i++)
    if(m_last_ctl[i].key == key)
      last = &m_last_ctl[i];

  if(!backwards)
    loop.add_control(pos, c.status, c.data1, c.data2);
  else if(last)
    loop.add_control(pos, last->status, last->data1, last->data2);
  else
    loop.touch(key);

  if(!last && m_last_ctl_count < int(m_last_ctl.size()))
    last = &m_last_ctl[m_last_ctl_count++];
  if(last)
    *last = {key, c.status, c.data1, c.data2};
}

//! The playhead stops going backwards at `boundary` while taking in: the
//! values last set hold from there.
inline void Node::close_backwards(int64_t boundary)
{
  if(loop.length <= 0)
    return;
  boundary %= loop.length;
  for(int i = 0; i < m_last_ctl_count; i++)
  {
    const auto& c = m_last_ctl[i];
    loop.add_control(boundary, c.status, c.data1, c.data2);
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
  else if(m_reverse)
  {
    // Played while the playhead went backwards: the note-off is where the
    // note starts on the loop.
    const int64_t on = n->start;
    n->start = pos;
    n->length = loop.distance(pos, on);
  }
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

//! An overdub starts at position k. A recording that just closed on a grid
//! point it was cut back to hands it what was played since that point: when
//! the playhead went on from the grid point, the pass starts there and what
//! was played is where it was played; else it is played at the start of the
//! pass, as the grace window does.
inline void Node::begin_pass_carried(int64_t k)
{
  const int64_t len = loop.length;
  const bool from_point
      = m_carried_span > 0 && m_carried_span < len && k == m_carried_span;
  begin_pass(from_point ? 0 : k);
  begin_pass_grace(from_point ? 0 : k);
  for(int i = 0; i < m_carried_note_count; i++)
  {
    const auto& c = m_carried_notes[i];
    const int64_t pos = from_point ? c.start % len : k;
    if(auto* n = loop.add_note(pos, c.channel, c.pitch, c.velocity); n && c.length >= 0)
      n->length = std::max<int64_t>(1, c.length);
  }
  for(int i = 0; i < m_carried_control_count; i++)
  {
    const auto& c = m_carried_controls[i];
    loop.add_control(from_point ? c.pos % len : k, c.status, c.data1, c.data2);
  }
  if(from_point)
    loop.advance_pass(double(m_carried_span));
  m_carried_note_count = 0;
  m_carried_control_count = 0;
  m_carried_span = 0;
}

inline int64_t Node::looper_run(int64_t start, int64_t length)
{
  // The playhead jumped, e.g. a mode change read the loop from its start
  // again: the notes the loop was playing would end at the wrong time. So do
  // they when it turns round.
  if(!loop.empty() && transport.playbackPos % loop.length != m_expected_pos)
  {
    flush_loop(start);
    m_phase = double(transport.playbackPos % loop.length);
  }
  if(m_reverse != m_was_reverse)
  {
    flush_loop(start);
    if(m_was_reverse && loop.taking && !loop.empty())
      close_backwards(int64_t(std::ceil(m_phase - 1e-9)));
    m_was_reverse = m_reverse;
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
        m_switches.soloed.forget();
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
        begin_pass_carried(k);
        m_cycle_pending = false;
        m_last_ctl_count = 0;
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
    return m_speed;
  return std::clamp(m_tempo / m_loop_tempo, 0.05, 20.) * m_speed;
}

//! The overdub went round the loop with a pass per cycle: the pass is over,
//! and the next round is a pass of its own.
inline void Node::next_cycle_pass(int64_t start)
{
  close_pass(true);
  begin_pass(start);
}

//! Plays the loop over [start; start + length[ of the tick, and takes in what
//! is played when overdubbing. The loop's positions go `rate()` times as fast
//! as the tick's frames; at the loop's end, an overdub extends it or the
//! playhead goes round. Backwards, the same is done on the loop read from its
//! end: position y of it is position length - y of the loop.
inline void Node::advance(int64_t start, int64_t length, bool overdub)
{
  const double r = rate();
  const bool back = m_reverse;
  // Backwards, the playhead reaches the loop's start, not its end.
  const bool extend = overdub && inputs.past_end.value == Looper::PastEnd::Extend && !back;
  const bool per_cycle = overdub && inputs.passes.value == Looper::Passes::PerCycle;
  // Undoing an extension can leave the playhead past the end of the loop.
  const double ph = std::fmod(std::max(0., m_phase), double(loop.length));
  double y = back ? double(loop.length) - ph : ph;
  if(y >= double(loop.length))
    y -= double(loop.length);
  double out = double(start);
  const double out_end = double(start + length);

  while(out < out_end - 1e-9)
  {
    const double L = double(loop.length);
    const double b = std::min(L, y + (out_end - out) * r);
    const double seg_out = (b - y) / r;
    if(overdub && std::exchange(m_cycle_pending, false))
      next_cycle_pass(back ? 0 : int64_t(y));

    if(back)
      play_range_reverse(y, b, out, r);
    else
      play_range(y, b, out, r);
    // The input of the frames this part of the loop covers.
    const int64_t from = int64_t(std::ceil(out - 1e-9));
    const int64_t to = b >= L ? int64_t(std::ceil(out + seg_out - 1e-9))
                              : int64_t(std::ceil(out_end - 1e-9));
    handle_input(from, to, y + (from - out) * r, overdub, r, back);
    if(overdub)
      loop.advance_pass(b - y, back);

    out += seg_out;
    y = b;
    if(y >= L - 1e-9)
    {
      if(extend && loop.extend(m_extend_unit, int64_t(max_loop_seconds * m_rate)))
      {
        // A cycle is the loop's length when the overdub started: the loop
        // growing by one is a cycle gone round as well.
        m_extended = true;
        if(per_cycle)
          m_cycle_pending = true;
      }
      else
      {
        y -= L;
        if(y < 0.)
          y = 0.;
        wrap_playing(loop.length, int64_t(out));
        if(per_cycle)
        {
          // The pass is over at the loop's start.
          if(back)
            close_backwards(0);
          m_cycle_pending = true;
        }
      }
    }
  }
  // Input on the last frame, when the span ended on the loop's end.
  if(!overdub)
    handle_input(start + length, start + length, -1, false);
  if(back)
  {
    y = double(loop.length) - y;
    if(y >= double(loop.length))
      y -= double(loop.length);
  }
  m_phase = y;
}

//! The notes and controllers of the loop between positions a and b, sent at
//! the frames of the tick they fall on: out0 is the frame of position a.
inline void Node::play_range(double a, double b, double out0, double r)
{
  const int64_t len = loop.length;
  const auto mode = inputs.overdub_mode.value;
  const bool truncate = inputs.held.value == HeldNotes::Truncate;
  auto frame = [&](int64_t q) { return int64_t(out0 + (q - a) / r); };
  // The whole positions in [a; b[.
  const int64_t lo = int64_t(std::ceil(a));
  const int64_t hi = std::min(len, int64_t(std::ceil(b)));
  // The repeats first + i * period of the loop that fall in [from; to[: a
  // long loop over a short pass is not gone through repeat by repeat.
  auto repeats = [len](int64_t first, int64_t period, int64_t from, int64_t to, auto f) {
    from = std::max(from, first);
    to = std::min(to, len);
    if(from >= to)
      return;
    for(int64_t q = first + (from - first + period - 1) / period * period; q < to;
        q += period)
      f(q);
  };

  int count = 0;
  auto add = [&](int64_t pos, int64_t end, int note) {
    if(count < int(m_range.size()))
      m_range[count++] = {pos, end, note};
  };
  for(int i = 0; i < loop.note_count; i++)
  {
    const auto& n = loop.notes[i];
    // A note still held on the input is heard from the input: playing it
    // again each round would start it with nothing to end it. A note an
    // overdub replaces may be sounding when it is replaced: it ends where
    // it was going to.
    if(n.length < 0 || n.pass >= loop.count || !loop.heard(n, mode))
      continue;

    // A pass shorter than the loop plays again over the rest of it.
    const int64_t period = std::clamp<int64_t>(loop.period_of(n.pass), 1, len);
    const auto [first, length] = place(n, period);
    // A note held past the loop's end wraps to its start, or, truncated,
    // ends on the loop's end, which is where the next round starts.
    repeats(first, period, lo, hi, [&](int64_t on) {
      add(on, truncate ? std::min(on + length, len) : on + length, i);
    });
  }
  play_notes(count, lo, hi, frame);

  for(int i = 0; i < loop.control_count; i++)
  {
    const auto& c = loop.controls[i];
    if(!loop.heard(c))
      continue;
    const int64_t period = std::clamp<int64_t>(loop.period_of(c.pass), 1, len);
    repeats(c.pos % period, period, lo, hi, [&](int64_t q) {
      libremidi::message m;
      const uint8_t kind = c.status & 0xF0;
      const int ch = m_out_channel > 0 ? m_out_channel : (c.status & 0x0F) + 1;
      const uint8_t status = uint8_t(kind | ((ch - 1) & 0x0F));
      if(kind == 0xD0)
        m = libremidi::message{status, c.data1};
      else
        m = libremidi::message{status, c.data1, c.data2};
      emit(frame(q), m);
    });
  }
}

//! The notes and controllers of the loop between positions a and b of the
//! loop read from its end, sent at the frames of the tick they fall on. A
//! note [s; s + l[ plays over [length - s - l; length - s[; a note a pass
//! repeats every period plays as often, the pattern mirrored in each period.
//! A controller is crossed at length - p, and goes back to the value it had
//! before p.
inline void Node::play_range_reverse(double a, double b, double out0, double r)
{
  const int64_t len = loop.length;
  const auto mode = inputs.overdub_mode.value;
  const bool truncate = inputs.held.value == HeldNotes::Truncate;
  auto frame = [&](int64_t q) { return int64_t(out0 + (q - a) / r); };
  const int64_t lo = int64_t(std::ceil(a));
  const int64_t hi = std::min(len, int64_t(std::ceil(b)));
  auto repeats = [len](int64_t first, int64_t period, int64_t from, int64_t to, auto f) {
    from = std::max(from, first);
    to = std::min(to, len);
    if(from >= to)
      return;
    for(int64_t q = first + (from - first + period - 1) / period * period; q < to;
        q += period)
      f(q);
  };
  // A pattern repeats the same way backwards only over a whole number of its
  // periods; a pass that does not fit is played once.
  auto period_of = [&](int pass) {
    const int64_t p = std::clamp<int64_t>(loop.period_of(pass), 1, len);
    return len % p == 0 ? p : len;
  };
  auto mirrored = [](int64_t pos, int64_t period) {
    int64_t m = (-pos) % period;
    if(m < 0)
      m += period;
    return m;
  };

  int count = 0;
  auto add = [&](int64_t pos, int64_t end, int note) {
    if(count < int(m_range.size()))
      m_range[count++] = {pos, end, note};
  };
  for(int i = 0; i < loop.note_count; i++)
  {
    const auto& n = loop.notes[i];
    if(n.length < 0 || n.pass >= loop.count || !loop.heard(n, mode))
      continue;
    const int64_t period = period_of(n.pass);
    const auto [start, length] = place(n, period);
    const int64_t first = mirrored(start + length, period);
    if(!truncate)
    {
      repeats(first, period, lo, hi, [&](int64_t on) { add(on, on + length, i); });
    }
    else
    {
      // Forwards, a note held across the loop's end ends with it: what is
      // heard of it is its start. Backwards, that is from the loop's start
      // to where the note started.
      repeats(first, period, lo, std::min(hi, len - length + 1), [&](int64_t on) {
        add(on, on + length, i);
      });
      if(lo <= 0 && 0 < hi)
        repeats(first, period, len - length + 1, len, [&](int64_t on) {
          add(0, on + length - len, i);
        });
    }
  }
  play_notes(count, lo, hi, frame);

  update_reverse_controls();
  for(int i = 0; i < loop.control_count; i++)
  {
    const auto& c = loop.controls[i];
    if(m_ctl_prev[i] < 0)
      continue;
    const auto& v = loop.controls[m_ctl_prev[i]];
    const int64_t period = period_of(c.pass);
    repeats(mirrored(c.pos, period), period, lo, hi, [&](int64_t q) {
      libremidi::message m;
      const uint8_t kind = v.status & 0xF0;
      const int ch = m_out_channel > 0 ? m_out_channel : (v.status & 0x0F) + 1;
      const uint8_t status = uint8_t(kind | ((ch - 1) & 0x0F));
      if(kind == 0xD0)
        m = libremidi::message{status, v.data1};
      else
        m = libremidi::message{status, v.data1, v.data2};
      emit(frame(q), m);
    });
  }
}

//! The controller before each one heard, by what they move then where they
//! are, round the loop: worked out again when what is heard may have changed.
inline void Node::update_reverse_controls()
{
  const std::array<int64_t, 6> signature{
      int64_t(loop.edits),       loop.control_count, loop.count,
      loop.taking ? loop.pass_written : -1, loop.pass_start, loop.length};
  if(signature == m_ctl_signature)
    return;
  m_ctl_signature = signature;

  int n = 0;
  for(int i = 0; i < loop.control_count; i++)
  {
    m_ctl_prev[i] = -1;
    if(loop.heard(loop.controls[i]))
      m_ctl_sorted[n++] = uint16_t(i);
  }
  const auto& ctl = loop.controls;
  std::sort(m_ctl_sorted.begin(), m_ctl_sorted.begin() + n, [&](uint16_t a, uint16_t b) {
    const auto ka = ctl[a].key(), kb = ctl[b].key();
    if(ka != kb)
      return ka < kb;
    if(ctl[a].pos != ctl[b].pos)
      return ctl[a].pos < ctl[b].pos;
    return a < b;
  });
  for(int g = 0; g < n;)
  {
    int e = g + 1;
    while(e < n && ctl[m_ctl_sorted[e]].key() == ctl[m_ctl_sorted[g]].key())
      e++;
    for(int j = g; j < e; j++)
      m_ctl_prev[m_ctl_sorted[j]] = int16_t(m_ctl_sorted[j == g ? e - 1 : j - 1]);
    g = e;
  }
}

//! An extending pass cut back to `end`: what it played since is dropped.
inline void Node::trim_top_pass(int64_t end)
{
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
  loop.edits++;
  // The pass went from where it started to the old end, and no further.
  loop.pass_written = std::clamp<int64_t>(end - loop.pass_start, 0, loop.pass_written);
  loop.pass_progress = double(loop.pass_written);
}

//! A take of `passes` passes, made from the loop as it is now. A take of the
//! passes that closed leaves out the one taken in since.
inline void Node::start_export(int passes, bool open, bool)
{
  m_export_loop.assign(loop);
  if(!open && passes < loop.count)
  {
    m_export_loop.count = passes;
    m_export_loop.total = passes;
    m_export_loop.taking = false;
  }
  m_export_pos = loop.length > 0 ? transport.playbackPos % loop.length : 0;
  write_export_item(m_export_mix ? -1 : 0);
}

//! Writes one file of the take: what is heard of the loop (item -1), or what
//! is heard of one of its passes.
inline void Node::write_export_item(int item)
{
  m_export_item = item;
  m_writing = true;
  fill_export(m_export_loop, item, m_export_pos, *export_data);

  Looper::midi_take_job job;
  job.writer = writer;
  job.data = export_data;
  job.slot = m_export_slot;
  job.number = m_export_number;
  job.layer = item;
  worker.request(std::move(job));
}

//! The score stopped while a pass was taken in: the loop as it is heard is a
//! take of its own, the mix, whose job owns all it needs. It is skipped when
//! the one before it is still being written.
inline void Node::write_stop_take(bool recording, double date, int64_t pos)
{
  if(!writer || loop.empty() || stop_data->in_use)
    return;
  fill_export(loop, -1, pos, *stop_data);
  stop_data->tempo = recording ? m_tempo : m_loop_tempo;
  stop_data->meter_numerator = m_signature.upper;
  stop_data->meter_denominator = m_signature.lower;
  m_stop_date = date;

  Looper::midi_take_job job;
  job.writer = writer;
  job.data = stop_data;
  job.slot = claim_take_slot();
  job.number = 0;
  job.layer = -1;
  job.alone = true;
  stop_data->in_use = true;
  worker.request(std::move(job));
}

//! What is heard of `snap` (item -1), or of one of its passes, into `e`; a
//! note still held lasts until `pos`.
inline void Node::fill_export(
    const Looper::midi_loop& snap, int item, int64_t pos, Looper::midi_export& e) const
{
  e.note_count = 0;
  e.control_count = 0;
  const auto mode = inputs.overdub_mode.value;
  for(int i = 0; i < snap.note_count; i++)
  {
    // A muted pass has its file, as it would be heard.
    auto n = snap.notes[i];
    if(!snap.heard(n, mode, item < 0) || (item >= 0 && n.pass != item))
      continue;
    if(n.length < 0)
      n.length = snap.distance(n.start, pos);
    // A pass shorter than the loop is in the file as many times as it plays.
    const int64_t period = std::clamp<int64_t>(snap.period_of(n.pass), 1, snap.length);
    const auto placed = place(n, period);
    for(int64_t on = placed.start; on < snap.length; on += period)
    {
      if(e.note_count >= Looper::midi_loop::max_notes)
        break;
      n.start = on;
      n.length = placed.length;
      e.notes[e.note_count++] = n;
    }
  }
  for(int i = 0; i < snap.control_count; i++)
  {
    const auto& c = snap.controls[i];
    if(!snap.heard(c, item < 0) || (item >= 0 && c.pass != item))
      continue;
    const int64_t period = std::max<int64_t>(1, std::min<int64_t>(snap.period_of(c.pass), snap.length));
    for(int64_t q = c.pos % period; q < snap.length; q += period)
    {
      if(e.control_count >= Looper::midi_loop::max_controls)
        break;
      e.controls[e.control_count] = c;
      e.controls[e.control_count++].pos = q;
    }
  }
  e.length = snap.length;
  e.rate = m_rate;
  e.tempo = m_export_tempo;
  e.meter_numerator = m_export_signature.upper;
  e.meter_denominator = m_export_signature.lower;
  e.wrap = inputs.held.value == HeldNotes::Wrap;
}

inline void Node::send_view(int64_t frames)
{
  if(!view_due(frames))
    return;

  auto& v = m_view;
  fill_view(v);
  v.midi = true;
  v.lanes = std::min(loop.count, Looper::view_message::max_lanes);
  v.max_seconds = 0.f;

  // One lane per pass, the oldest passes in the first one when there are more
  // passes than lanes, as the audio looper's.
  constexpr int max_lanes = Looper::view_message::max_lanes;
  constexpr int max_shown = Looper::view_message::max_notes;
  const int lanes = v.lanes;
  const int merged = loop.count - lanes;
  const auto mode = inputs.overdub_mode.value;
  const int64_t len = std::max<int64_t>(1, loop.length);
  auto lane_of = [&](int pass) { return std::max(0, pass - merged); };
  // A pass shorter than the loop is drawn each time it plays.
  auto period_of = [&](const Looper::midi_note& n) {
    return std::max<int64_t>(1, std::min<int64_t>(loop.period_of(n.pass), len));
  };

  // Each lane gets as many of the notes as it has, up to an even share of
  // what the message holds; the share a lane does not use goes to the others.
  std::array<int64_t, max_lanes> wanted{};
  for(int i = 0; i < loop.note_count; i++)
  {
    const auto& n = loop.notes[i];
    if(n.pass >= loop.count)
      continue;
    const int64_t period = period_of(n);
    wanted[lane_of(n.pass)] += (len - 1 - n.start % period) / period + 1;
  }
  std::array<int, max_lanes> quota{};
  {
    std::array<bool, max_lanes> settled{};
    int left = max_shown;
    for(bool changed = true; changed;)
    {
      changed = false;
      int open = 0;
      for(int l = 0; l < lanes; l++)
        open += !settled[l];
      if(open == 0)
        break;
      const int share = left / open;
      for(int l = 0; l < lanes; l++)
      {
        if(!settled[l] && wanted[l] <= share)
        {
          quota[l] = int(wanted[l]);
          left -= quota[l];
          settled[l] = true;
          changed = true;
        }
      }
      if(!changed)
        for(int l = 0; l < lanes; l++)
          if(!settled[l])
            quota[l] = share;
    }
  }

  // The notes heard first, so that a lane over its share keeps those.
  int shown = 0;
  std::array<int, max_lanes> used{};
  for(int heard = 1; heard >= 0; heard--)
  {
    for(int i = 0; i < loop.note_count; i++)
    {
      const auto& n = loop.notes[i];
      if(n.pass >= loop.count || loop.heard(n, mode) != bool(heard))
        continue;
      const int lane = lane_of(n.pass);
      // A key still held goes from where it was pressed to the playhead, which
      // is before it when the loop plays backwards.
      const int64_t head = transport.playbackPos % len;
      int64_t first = n.start;
      int64_t l = std::min(n.length, len - 1);
      if(n.length < 0)
      {
        l = m_reverse ? loop.distance(head, n.start) : loop.distance(n.start, head);
        if(m_reverse)
          first = head;
      }
      const int64_t period = period_of(n);
      for(int64_t on = first % period; on < len && used[lane] < quota[lane];
          on += period)
      {
        auto& d = v.note[shown++];
        used[lane]++;
        d.start = float(on) / float(len);
        d.end = float((on + l) % len) / float(len);
        d.pitch = n.pitch;
        d.lane = uint8_t(lane);
        d.velocity = n.velocity;
        d.heard = uint8_t(heard);
      }
    }
  }
  v.notes = shown;
  send_message(v);
}
}
