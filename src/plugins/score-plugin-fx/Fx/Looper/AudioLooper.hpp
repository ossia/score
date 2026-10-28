#pragma once
#include <Fx/Looper/AudioLayers.hpp>
#include <Fx/Looper/LooperNode.hpp>
#include <Fx/Types.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/nodes/sound.hpp>
#include <ossia/dataflow/token_request.hpp>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/soundfile_port.hpp>

#include <functional>
#include <limits>
#include <memory>

namespace Nodes::Looper
{
inline ossia::audio_stretch_mode stretch_mode(FollowTempo f) noexcept
{
  switch(f)
  {
    case FollowTempo::Repitch:
      return ossia::audio_stretch_mode::Repitch;
    case FollowTempo::RepitchFast:
      return ossia::audio_stretch_mode::RepitchFastestQ;
    case FollowTempo::Timestretch:
      return ossia::audio_stretch_mode::RubberBandStandard;
    case FollowTempo::TimestretchPercussive:
      return ossia::audio_stretch_mode::RubberBandPercussive;
    case FollowTempo::Off:
    default:
      return ossia::audio_stretch_mode::None;
  }
}

//! How a take made with `f` asks the timeline to play it: as the loop was.
inline halp::clip_stretch clip_stretch(FollowTempo f) noexcept
{
  switch(f)
  {
    case FollowTempo::Repitch:
      return halp::clip_stretch::repitch;
    case FollowTempo::RepitchFast:
      return halp::clip_stretch::repitch_fast;
    case FollowTempo::Timestretch:
      return halp::clip_stretch::timestretch;
    case FollowTempo::TimestretchPercussive:
      return halp::clip_stretch::timestretch_percussive;
    case FollowTempo::Off:
    default:
      return halp::clip_stretch::none;
  }
}
}

namespace Nodes::AudioLooper
{
struct Node : Looper::looper_node<Node>
{
  halp_meta(name, "Looper (audio)")
  halp_meta(c_name, "Looper (audio)")
  halp_meta(category, "Audio/Utilities")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/audio-looper.html")
  halp_meta(
      description,
      "Loop audio in layers, and turn the loop into a clip of the timeline")
  halp_meta(uuid, "a0ad4227-ac3d-448b-a19b-19581ed4e2c6");
  halp_meta(recommended_height, 200);
  halp_flag(process_exec);

  //! Inlets that presets which do not name their controls count in their
  //! indices, and that the looper does not have: Sync.
  static constexpr std::array<int, 1> removed_inlets() noexcept { return {16}; }

  using LoopMode = Looper::LoopMode;
  using Passthrough = Looper::Passthrough;

  struct ins
  {
    halp::dynamic_audio_bus<"in", double> audio;

    struct : halp::enum_t<LoopMode, "Mode">
    {
      using halp::enum_t<LoopMode, "Mode">::operator=;
      static constexpr std::string_view former_name() noexcept { return "Loop"; }
      halp_meta(
          description,
          "What the looper is doing, or about to do on the grid: playing the "
          "loop, recording it, overdubbing on top of it, or nothing. It follows "
          "every change, by an action, a recording's end or a full buffer. "
          "Record on a loop overdubs it: Clear it first to record a new one. "
          "Overdub on an empty loop records it, then overdubs.")
      //! Shows the mode actions, a recording's end and a full loop change it to.
      std::function<void(std::string_view)> update_controller;
      //! A value came in, from the UI or a cable.
      void update(Node& n) noexcept { n.loop_arrived(); }
    } mode;

    Looper::quantif_selector quantif;

    struct : Looper::enum_combobox<"Passthrough", Passthrough, Passthrough::Record>
    {
      using enum_combobox::operator=;
      halp_meta(
          description,
          "Whether the input is heard as well as the loop. Record passthrough "
          "is heard only while recording or overdubbing; Full is heard always.")
    } passthrough;

    Looper::after_record_selector after_record;

    struct : halp::spinbox_i32<"Record bars", halp::irange{0, 64, 4}>
    {
      static constexpr std::string_view former_name() noexcept { return "Bars"; }
      halp_meta(
          description,
          "How long a recording runs before After record takes over, and so "
          "how long the loop it leaves is. Zero leaves recording to be ended by "
          "hand, and the loop is then as long as it was played for.")
    } postaction_bars;

    Looper::start_from_selector start_from;

    struct : halp::impulse_button<"Undo">
    {
      halp_meta(
          description,
          "Removes the last pass: the overdub in progress, else the last one "
          "made. Undoing the first recording empties the loop.")
    } undo;
    struct : halp::impulse_button<"Redo">
    {
      halp_meta(description, "Brings back the last pass undone")
    } redo;
    struct : halp::impulse_button<"Clear">
    {
      halp_meta(description, "Empties the loop and forgets every pass")
    } clear;

    struct : halp::knob_f32<"Feedback", halp::range{0., 1., 1.}>
    {
      halp_meta(
          description,
          "How much of the loop is kept each time an overdub goes round it: "
          "one keeps it all, lower values let older passes fade away.")
    } feedback;

    struct : halp::spinbox_i32<"Late stop (ms)", halp::irange{0, 500, 120}>
    {
      halp_meta(
          description,
          "A recording asked to stop this long after a point of the grid ends "
          "on that point, instead of a whole grid step later.")
    } late;

    struct : halp::spinbox_i32<"Latency (ms)", halp::irange{0, 500, 0}>
    {
      halp_meta(
          description,
          "How late the input reaches the looper: what was recorded is moved "
          "this much earlier in the loop.")
    } latency;

    struct : halp::spinbox_i32<"Max length (s)", halp::irange{1, 3600, 60}>
    {
      halp_meta(
          description,
          "Longest loop. The memory is reserved when playback starts, so a "
          "change applies from the next start.")
    } max_seconds;

    struct : halp::spinbox_i32<"Max layers", halp::irange{2, 32, 8}>
    {
      halp_meta(
          description,
          "Layers kept apart, for undo, mute, solo and erase. What a pass does "
          "past it is what At max layers says. Applies from the next start.")
    } max_layers;

    struct : halp::spinbox_i32<"Channels", halp::irange{1, 16, 2}>
    {
      halp_meta(
          description,
          "Channels recorded; the input's other channels are not. Applies from "
          "the next start.")
    } channels;

    struct : halp::spinbox_i32<"Keep last (s)", halp::irange{0, 600, 0}>
    {
      halp_meta(
          description,
          "Seconds of input kept at all times, for Capture. Zero keeps none. "
          "Applies from the next start.")
    } keep;

    struct : halp::spinbox_i32<"Capture bars", halp::irange{1, 64, 4}>
    {
      halp_meta(
          description,
          "How many bars Capture makes a loop of when there is none. Over a "
          "loop, Capture takes one round of it, whatever this says.")
    } capture_bars;

    struct : halp::impulse_button<"Capture">
    {
      halp_meta(
          description,
          "Takes the input of the last moments, recorded or not, without having "
          "to press Record first. With no loop, the last Capture bars become "
          "the loop, played at once. Over a loop, the input over its last round "
          "becomes a new layer, where it was played in the loop. Undo takes it "
          "out. Pressed just after a grid point, it ends there, as Late says. "
          "Needs Keep last.")
    } capture;

    // Controls added after the first presets go last: presets address the
    // inputs by their index.
    struct : halp::toggle<"Engine latency">
    {
      halp_meta(
          description,
          "Also moves what is recorded earlier by the latency the audio driver "
          "reports for its input and output, for input that comes from the "
          "sound card while the loop is heard. Leave off for input made inside "
          "score.")
    } engine_latency;

    struct : halp::soundfile_port<"Loop file">
    {
      halp_meta(
          description,
          "A sound file to loop: dropping one on the looper replaces the loop "
          "with it, up to Max length.")
    } file;

    struct : Looper::enum_combobox<"Export", Looper::TakeFiles, Looper::TakeFiles::Mix>
    {
      using enum_combobox::operator=;
      static constexpr std::string_view former_name() noexcept { return "Takes"; }
      halp_meta(
          description,
          "What Send, or a drag from it, places: the loop as it is heard "
          "(Mix), each pass in a file of its own (Layers), or both, each on a "
          "lane of its own. A take written as a pass closes has these files; "
          "one Send or a drag asks for, or the score's stop leaves, has them "
          "all, so that Export can still be changed once the score is "
          "stopped. Dragging a lane places its layer alone.")
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

    struct : Looper::enum_combobox<"Follow tempo", Looper::FollowTempo, Looper::FollowTempo::Off>
    {
      using enum_combobox::operator=;
      struct range
      {
        std::string_view values[5]
            = {"Off", "Repitch", "Repitch fast", "Timestretch", "Timestretch percussive"};
        Looper::FollowTempo init = Looper::FollowTempo::Off;
      };
      halp_meta(
          description,
          "Plays the loop at the tempo of the score, as sound files are: "
          "resampled (Repitch: the pitch follows), or time-stretched (the "
          "pitch stays). A loop being recorded or overdubbed plays at its own "
          "speed, and a tempo change applies from the next round of the loop. "
          "Applies from the next start.")
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
          "Plays the loop backwards, from where the playhead is. An overdub "
          "writes where the playhead goes, as on tape: what is played over the "
          "reversed loop sounds reversed once it plays forwards again. "
          "Recording a new loop is always forwards.")
      std::function<void(bool)> update_controller;
    } reverse;

    struct : Looper::speed_selector<"Speed">
    {
      halp_meta(
          description,
          "Plays the loop slower or faster, as a tape: the pitch follows. An "
          "overdub writes where the playhead goes, so what is played at x1/2 "
          "sounds an octave up at x1. The grid, the bars and the loop's length "
          "stay in the score's time; combines with Follow tempo.")
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
  } inputs;

  struct
  {
    halp::mimic_audio_bus<"out", &ins::audio> audio;
    struct : halp::val_port<"Phase", float>
    {
      halp_meta(description, "Position in the loop, from 0 to 1")
    } phase;
  } outputs;

  // Writing takes to files
  struct worker_t
  {
    std::function<void(Looper::take_job)> request;
    //! The take the score's stop leaves is written once the looper is gone.
    static bool must_run(const Looper::take_job& job) noexcept { return job.alone; }
    static std::function<void(Node&)> work(Looper::take_job&& job)
    {
      const bool ok = job.run();
      return [slot = job.slot, number = job.number, alone = job.alone, ok](Node& self) {
        self.take_written(slot, number, ok, alone);
      };
    }
  } worker;

  std::function<void(Looper::view_message)> send_message;
  //! What the take the score's stop leaves is told to the views with, from
  //! its worker.
  std::function<void(Looper::view_message)> send_message_from_worker;

  //! From the view: applied at the start of the next tick.
  void process_message(const Looper::erase_request& r) { view_request(r); }

  const score::DocumentContext* ossia_document_context{};
  ossia::exec_state_facade ossia_state;

  Looper::audio_layers layers;
  std::shared_ptr<Looper::take_writer> writer;
  std::shared_ptr<Looper::export_buffer> export_audio;
  //! What the take the score's stop leaves is made of: the node may be gone
  //! before it is written.
  std::shared_ptr<Looper::audio_stop_take> stop_take;

  Node()
  {
    layers.allocate(2, 8, 60 * 48000);
    export_audio = Looper::export_buffer::make(layers.channels, layers.capacity);
    stop_take = std::make_shared<Looper::audio_stop_take>();
  }

  void prepare(halp::setup s)
  {
    const double rate = s.rate > 0 ? s.rate : 48000.;
    layers.allocate(
        inputs.channels.value, inputs.max_layers.value,
        int64_t(inputs.max_seconds.value * rate));
    if(!m_writing
       && (export_audio->channels != layers.channels
           || export_audio->capacity != layers.capacity))
      export_audio = Looper::export_buffer::make(layers.channels, layers.capacity);
    allocate_ring(layers.channels, int64_t(inputs.keep.value * rate));
    m_switches.seen_passes = layers.count;
    prepare_stretch(rate, s.frames);
    if(ossia_document_context)
      writer = Looper::take_writer::make(*ossia_document_context, "Looper", "wav");
    if(writer)
      writer->tell_views = send_message_from_worker;
  }

  //! Main thread only: the stretcher allocates. It is run once here over the
  //! longest span it will be given, on silence, so that the buffers it sizes
  //! on its first run are sized before the audio thread runs it.
  void prepare_stretch(double rate, int frames)
  {
    m_follow_tempo = inputs.follow_tempo.value;
    m_stretch_mode = Looper::stretch_mode(m_follow_tempo);
    const auto chans = std::size_t(layers.channels);
    m_stretcher.reset(0, m_stretch_mode, chans, std::size_t(rate), std::size_t(rate));
    const auto longest = std::size_t(std::max(frames, 8192));
    m_stretch_out.resize(chans);
    for(auto& c : m_stretch_out)
      c.assign(longest, 0.);
    m_stretch_span.clear();
    for(auto& c : m_stretch_out)
      m_stretch_span.emplace_back(c.data(), longest);
    m_stretch_ready = false;

    if(m_stretch_mode != ossia::audio_stretch_mode::None)
    {
      struct silence_source
      {
        void fetch_audio(int64_t, int64_t n, float** out) const noexcept
        {
          for(std::size_t c = 0; c < chans; c++)
            std::fill_n(out[c], n, 0.f);
        }
        void fetch_audio(int64_t, int64_t n, double** out) const noexcept
        {
          for(std::size_t c = 0; c < chans; c++)
            std::fill_n(out[c], n, 0.);
        }
        void fetch_audio_backward(int64_t s, int64_t n, float** out) const noexcept
        {
          fetch_audio(s, n, out);
        }
        void fetch_audio_backward(int64_t s, int64_t n, double** out) const noexcept
        {
          fetch_audio(s, n, out);
        }
        std::size_t chans;
      } source{chans};
      ossia::token_request tk;
      tk.prev_date = ossia::time_value{0};
      tk.date = ossia::time_value{int64_t(longest)};
      m_stretcher.run(
          source, tk, ossia_state, 1., 1., chans, int64_t(longest), int64_t(longest),
          int64_t(longest), 0, m_stretch_span);
      m_stretcher.transport(0);
      for(auto& c : m_stretch_out)
        std::fill(c.begin(), c.end(), 0.);
    }
  }

  //! Main thread only.
  void allocate_ring(int channels, int64_t frames)
  {
    if(channels == m_ring_channels && frames == m_ring_capacity)
      return;
    m_ring.clear();
    m_ring.shrink_to_fit();
    m_ring_channels = channels;
    m_ring_capacity = std::max<int64_t>(0, frames);
    m_ring.resize(
        std::size_t(m_ring_channels) * std::size_t(m_ring_capacity),
        boost::container::default_init);
  }

  // The transport's engine
  int64_t looper_run(int64_t start, int64_t length);
  void looper_trim_recording(int64_t frames)
  {
    // What was recorded past the grid point stays in the buffer, after the
    // loop's end, for an overdub that starts there.
    const int64_t end = std::max<int64_t>(0, layers.length - frames);
    m_carried_span += layers.length - end;
    layers.length = end;
    if(auto* l = layers.top())
      l->written = layers.length;
  }

  void operator()(const tick& tk)
  {
    if(tk.date == tk.prev_date)
      return;
    run_tick(tk);
    send_view(ossia_state.timings(tk).length);
  }

  //! Stopping the process keeps the loop: only the transport starts over.
  void stop() { stop_score(); }

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
          halp::item<&ins::capture> capture;
          halp::item<&ins::capture_bars> capture_bars;
        } length;
        struct
        {
          halp_meta(name, "Play")
          halp_meta(layout, halp::layouts::grid)
          halp_meta(alignment, halp::alignments::left)
          halp_meta(spacing, 6)
          halp_meta(columns, 3)
          halp::item<&ins::passthrough> passthrough;
          halp::item<&ins::start_from> start_from;
          halp::item<&ins::feedback> feedback;
          halp::item<&ins::past_end> past_end;
          halp::item<&ins::passes> passes;
        } play;
        Looper::switches_tab<ins> switches;
        Looper::layers_tab<ins> layers;
        Looper::speed_tab<ins> speed;
        struct
        {
          halp_meta(name, "Memory")
          halp_meta(layout, halp::layouts::grid)
          halp_meta(alignment, halp::alignments::left)
          halp_meta(spacing, 6)
          halp_meta(columns, 3)
          halp::item<&ins::max_seconds> max_seconds;
          halp::item<&ins::keep> keep;
          halp::item<&ins::max_layers> max_layers;
          halp::item<&ins::at_max_layers> at_max_layers;
          halp::item<&ins::channels> channels;
          halp::item<&ins::latency> latency;
          halp::item<&ins::engine_latency> engine_latency;
        } memory;
      } settings;
    } controls;

    struct
    {
      halp_meta(layout, halp::layouts::container)
      halp_meta(background, halp::colors::background_mid)
      Looper::looper_view_item<ins> view;
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

  // Public for the tests: the audio of one pass.
  const float* layer_data(int layer, int chan) const noexcept
  {
    return layers.data(layers.layers[layer].slot, chan);
  }

  //! Samples the looper's background work -- merging passes, flattening a
  //! take, drawing the overview -- goes through in one tick at most.
  static constexpr int64_t work_per_tick = 262144;

private:
  friend struct Looper::looper_node<Node>;

  // looper_node's hooks
  Looper::audio_layers& stack() noexcept { return layers; }
  const Looper::audio_layers& stack() const noexcept { return layers; }
  int max_layers() const noexcept { return layers.slots; }
  void layers_changing() noexcept { }
  bool busy() const noexcept { return m_loading; }
  void cleared() noexcept
  {
    m_loading = false;
    m_seam.base_slot = -1;
    m_full = false;
  }
  void pass_dropped() noexcept { }
  void begin_overdub(int64_t start, bool oldest, int replaced)
  {
    layers.begin_overdub(start, oldest, replaced);
    m_pass_closed = false;
  }
  bool can_merge_newest() const noexcept { return !layers.merging.active; }
  void merge_newest() noexcept { layers.start_merge(layers.count - 2, true); }
  //! The top pass was closed on a cut already, the pass after it dropped.
  bool pass_closing() noexcept { return !std::exchange(m_pass_closed, true); }
  //! Its end faded, moved by the latency.
  void pass_closed()
  {
    layers.fade_out_top();
    shift_pass(*layers.top());
  }
  void tick_started(const tick& tk)
  {
    m_in_chans = std::min<int>(inputs.audio.channels, layers.channels);
    m_tick_start = Looper::timeline_start(tk, ossia_state);
  }
  void prepare_loop() { load_file(); }
  void forget_carried() noexcept { m_carried_span = 0; }
  void tick_finished(const tick& tk)
  {
    keep_input(ossia_state.timings(tk).length);
    // About a quarter of a million samples of merging per tick, whatever the
    // number of channels.
    layers.merge_step(std::max<int64_t>(1024, work_per_tick / std::max(1, layers.channels)));
  }
  void close_recording(int64_t bars);
  void close_overdub() { close_pass(); }
  bool cut_after(int64_t end) const noexcept
  {
    return layers.layers[layers.count - 1].start >= end;
  }
  void cut_pass_dropped() noexcept { m_pass_closed = true; }
  void trim_top_pass(int64_t end) noexcept
  {
    auto& top = *layers.top();
    top.written = std::clamp<int64_t>(end - top.start, 0, top.written);
  }
  void head_moved(int64_t) noexcept { }
  //! Following the tempo, the loop goes at the score's tempo over its own,
  //! but not while it is overdubbed.
  double rate_for(LoopMode m) const noexcept
  {
    return by_quarters() && m != LoopMode::Overdub
               ? m_speed * std::max(1., m_tempo) / m_loop_tempo
               : m_speed;
  }
  bool by_quarters() const noexcept
  {
    return m_stretch_mode != ossia::audio_stretch_mode::None && m_loop_tempo > 0.;
  }
  void set_head(double y, int64_t len) noexcept
  {
    set_head_y(y, len);
    m_stretch_ready = false;
  }
  int64_t capture_clock() const noexcept { return m_ring_written; }
  int64_t capture_limit() const noexcept
  {
    return m_ring_capacity > 0 ? std::min(m_ring_capacity, layers.capacity) : 0;
  }
  void capture_into(int64_t end, int64_t n);
  void captured() noexcept { }
  //! A layer is captured from the input kept, in a buffer that may be one a
  //! take being written still reads.
  bool capture_layer_ready(const Looper::capture_window&) noexcept
  {
    if(m_ring_capacity <= 0)
      return false;
    layers.reserved_overwritten = false;
    update_seam(true);
    return true;
  }
  void capture_layer_into(const Looper::capture_window& w);
  void stopping() noexcept { }
  void close_on_stop(bool recording)
  {
    if(!recording)
      close_pass();
  }
  bool export_ready() const noexcept
  {
    return !m_flattening && !export_audio->in_use && export_audio->capacity >= layers.length
           && export_audio->channels >= layers.channels;
  }
  void start_export(int passes, bool open, bool recording);
  void continue_export();
  void export_item(int item) { start_export_item(item); }
  bool take_cancelled() noexcept { return std::exchange(m_export_cancelled, false); }
  void take_released() noexcept { layers.reserved = 0; }
  void stop_take_written() noexcept { layers.held = 0; }
  const Looper::stop_take_notice& stop_notice() const noexcept { return stop_take->notice; }
  static constexpr const char* take_extension = "wav";
  //! The Follow tempo the loop plays with, which takes keep.
  halp::clip_stretch take_stretch() const noexcept
  {
    return Looper::clip_stretch(m_follow_tempo);
  }
  bool view_full() const noexcept { return m_full; }
  bool view_writing() const noexcept { return m_flattening || m_writing; }

  int m_in_chans{};
  //! Where the timeline is at the start of this tick.
  Looper::transport::timeline_point m_tick_start{};

  // Export of a take: the passes are flattened into the export buffer a
  // bounded number of frames per tick, then a worker writes it.
  bool m_flattening{};
  int64_t m_flatten_pos{};
  int64_t m_export_length{};
  int m_export_channels{};
  bool m_export_cancelled{};
  //! The passes of the take as they were when it closed, which every file of
  //! it is flattened from: their buffers are reserved until it is written.
  std::array<Looper::audio_layer, Looper::audio_layers::max_passes> m_export_snapshot{};
  int m_export_count{};
  void cancel_export() noexcept;
  bool m_full{};

  // Playing the loop at the tempo of the score, with the stretchers of the
  // sound files reading the loop as their source.
  ossia::resampler m_stretcher;
  Looper::FollowTempo m_follow_tempo{Looper::FollowTempo::Off};
  ossia::audio_stretch_mode m_stretch_mode{ossia::audio_stretch_mode::None};
  std::vector<std::vector<double>> m_stretch_out;
  //! Views on m_stretch_out, made once: more channels than the container
  //! holds inline would allocate.
  ossia::mutable_audio_span<double> m_stretch_span;
  //! Position of the loop being heard: the stretchers read ahead of it.
  double m_heard{};
  double m_cycle_ratio{1.};
  bool m_stretch_ready{};
  int64_t m_expected_pos{};
  bool m_wrapped{};
  //! Speed and direction the stretchers were last given the loop at.
  double m_stretch_speed{1.};
  bool m_stretch_reverse{};
  void play_stretched(int64_t first_pos, int64_t samples);

  // The playhead as a tape's: it reads the loop at m_speed, backwards when
  // m_reverse is set. Its position is transport.playbackPos and the part of a
  // frame m_frac it is past it, which a move of the playhead by anything else
  // than playing drops.
  double m_frac{};
  int64_t m_head_pos{-1};
  //! The input of the last frame, and what came in since the last position
  //! an overdub slower than x1 wrote.
  std::array<float, Looper::audio_layers::max_channels> m_prev_in{};
  std::array<float, Looper::audio_layers::max_channels> m_acc{};
  int m_acc_n{};
  //! Positions the pass has written, its later rounds included: the first
  //! ones fade it in.
  int64_t m_pass_frames{};
  //! Frames a recording cut back to a grid point had recorded past it.
  int64_t m_carried_span{};
  void begin_pass_carried(int64_t len);
  double head_y(int64_t len) const noexcept;
  void set_head_y(double y, int64_t len) noexcept;
  float tape(int c, int64_t k, int64_t len) const noexcept
  {
    return layers.mix(c, m_reverse ? len - 1 - k : k);
  }
  float read_span(int c, double y, double s, int64_t len) const noexcept;
  void write_tape(int64_t y, const float* v, float feedback);
  bool cycle_end(int64_t& len, bool extend, float feedback);
  int64_t latency_frames() const noexcept;
  void shift_pass(Looper::audio_layer& l);
  //! The top pass is closed: closing the take does not close it again.
  bool m_pass_closed{};

public:
  //! What the stretchers read: the loop as it is heard, round and round.
  template <typename T>
  void fetch_audio(int64_t start, int64_t frames, T** out) noexcept
  {
    const int64_t len = layers.length;
    if(len > 0 && (m_stretch_speed != 1. || m_stretch_reverse))
    {
      // Frame u of what the stretchers read is the tape at u * speed.
      const double sp = m_stretch_speed;
      for(int c = 0; c < layers.channels; c++)
        for(int64_t j = 0; j < frames; j++)
          out[c][j] = T(read_span(c, std::fmod(double(start + j) * sp, double(len)), sp, len));
      if(std::floor(double(start + frames) * sp / double(len))
         > std::floor(double(start) * sp / double(len)))
        m_wrapped = true;
      return;
    }
    for(int c = 0; c < layers.channels; c++)
    {
      int64_t k = len > 0 ? start % len : 0;
      for(int64_t j = 0; j < frames; j++)
      {
        out[c][j] = len > 0 ? T(layers.mix(c, k)) : T(0);
        if(++k >= len)
        {
          k = 0;
          m_wrapped = true;
        }
      }
    }
  }
  template <typename T>
  void fetch_audio_backward(int64_t, int64_t frames, T** out) noexcept
  {
    for(int c = 0; c < layers.channels; c++)
      std::fill_n(out[c], frames, T(0));
  }

private:
  // The overview of the passes, worked out over as many messages as a long
  // loop needs.
  std::array<float, Looper::view_message::max_lanes * Looper::view_message::bins>
      m_scan_peaks{};
  int m_scan_layer{};
  int64_t m_scan_pos{};
  int m_scan_count{-1};
  uint64_t m_scan_changes{};
  int64_t m_scan_length{-1};
  void scan_peaks(bool whole = false);

  void load_file();

  // A file being copied into the loop, a part per tick.
  const float** m_file_data{};
  int64_t m_file_frames{};
  int64_t m_file_pos{};
  int64_t m_file_target{};
  bool m_loading{};
  void keep_input(int64_t frames);
  void fade_edges(int slot, bool end = true);
  //! A recording closed into an overdub on the same frame: the pass carries
  //! on what it was taking in, so neither is faded where they meet.
  bool m_seamless{};
  //! The end of such a recording, faded while the overdub that went on from
  //! it is not heard, as it is when a recording simply ends. The samples the
  //! fade changes are kept to give them back.
  struct seam
  {
    //! Buffers of the recording and of the overdub, -1 for none.
    int base_slot{-1};
    int pass_slot{-1};
    //! Frames of the recording.
    int64_t end{};
    bool faded{};
    std::array<
        float, Looper::audio_layers::max_channels * Looper::audio_layers::fade_frames>
        kept{};
  } m_seam;
  void update_seam(bool new_pass);
  void fade_seam(bool faded);

  // The input of the last seconds, for Capture.
  boost::container::vector<float> m_ring;
  int64_t m_ring_capacity{};
  int m_ring_channels{};
  //! Frames of input seen since the looper was created.
  int64_t m_ring_written{};
  void start_export_item(int item);
  void flatten(
      const Looper::audio_layer* passes, int count, int item, int64_t len, int64_t from,
      int64_t to);
  void copy_open_pass();
  void write_stop_take(bool recording, double date, int64_t pos);
  //! A take asked for while an overdub goes on: the pass is copied into a
  //! free buffer before the take is flattened, ahead of where the pass
  //! writes, so that the take is the loop of one moment.
  int m_copy_from{-1};
  int m_copy_to{-1};
  int64_t m_copy_pos{};
  int64_t m_copy_left{};
  int64_t m_copy_period{};
  int m_copy_dir{1};
  void send_view(int64_t frames);
  void fill_message(Looper::view_message& v);

  void play(int64_t first_pos, int64_t samples);
  void play_tape(int64_t first_pos, int64_t samples);
  void silence(int64_t first_pos, int64_t samples);
  void mix_input(int64_t first_pos, int64_t samples);
  void through(int64_t first_pos, int64_t samples);
  int64_t record(int64_t first_pos, int64_t samples, bool echo);
  void overdub(int64_t first_pos, int64_t samples, bool echo);
};

inline int64_t Node::looper_run(int64_t start, int64_t length)
{
  update_seam(false);
  // Stretching starts again from where the loop is, the next time it plays.
  if(transport.actualMode != LoopMode::Play)
    m_stretch_ready = false;
  if(transport.playbackPos != m_head_pos)
    m_frac = 0.;

  // Record passthrough is what its name says: the input while something is
  // being taken in, and nothing else. Only Full is heard the rest of the time.
  const Passthrough pt = inputs.passthrough;
  const bool echo_while_taking_in = pt != Passthrough::None;
  const bool echo_otherwise = pt == Passthrough::Full;
  switch(transport.actualMode)
  {
    case LoopMode::Play:
      if(layers.empty())
        echo_otherwise ? through(start, length) : silence(start, length);
      else
      {
        if(m_stretch_mode != ossia::audio_stretch_mode::None)
          play_stretched(start, length);
        else if(m_speed != 1. || m_reverse || m_frac != 0.)
          play_tape(start, length);
        else
          play(start, length);
        if(echo_otherwise)
          mix_input(start, length);
      }
      return length;
    case LoopMode::Stop:
      echo_otherwise ? through(start, length) : silence(start, length);
      break;
    case LoopMode::Record:
      return record(start, length, echo_while_taking_in);
    case LoopMode::Overdub:
      overdub(start, length, echo_while_taking_in);
      break;
  }
  return length;
}

//! The recording is over: moved by the latency, faded, and padded with
//! silence up to its `bars` when they are longer.
inline void Node::close_recording(int64_t bars)
{
  auto* top = layers.top();
  if(!top || layers.length <= 0)
    return;

  if(const int64_t latency = latency_frames(); latency > 0)
    layers.shift_earlier(*top, latency);

  // Overdubbing from the frame the recording ends on carries on what was
  // played: the end of the loop goes on into the first pass over it.
  m_seamless = transport.actualMode == LoopMode::Overdub;
  fade_edges(top->slot, !m_seamless);
  if(m_seamless)
  {
    const int64_t n = std::min<int64_t>(layers.length, Looper::audio_layers::fade_frames);
    m_seam.base_slot = top->slot;
    m_seam.pass_slot = -1;
    m_seam.end = layers.length;
    m_seam.faded = false;
    for(int c = 0; c < layers.channels; c++)
      std::copy_n(
          layers.data(top->slot, c) + layers.length - n, n,
          m_seam.kept.data() + c * Looper::audio_layers::fade_frames);
  }

  if(bars > layers.length)
  {
    const int64_t end = std::min(bars, layers.capacity);
    for(int c = 0; c < layers.channels; c++)
      std::fill(layers.data(top->slot, c) + layers.length, layers.data(top->slot, c) + end, 0.f);
    layers.length = end;
  }
  top->written = layers.length;
}

inline int64_t Node::latency_frames() const noexcept
{
  return int64_t(inputs.latency.value * m_rate / 1000.)
         + (inputs.engine_latency.value
                ? int64_t(ossia_state.inputLatency()) + ossia_state.outputLatency()
                : 0);
}

//! An overdub pass is moved by the latency, in positions of the loop: the
//! playhead went `speed` of them a frame, and backwards it moves the pass
//! later.
inline void Node::shift_pass(Looper::audio_layer& l)
{
  const int64_t latency = latency_frames();
  if(latency <= 0)
    return;
  const int64_t positions = std::llround(double(latency) * m_speed);
  layers.shift_earlier(l, l.backwards ? -positions : positions);
}

//! What was taken for a take being written is about to change: the take
//! stops where it is, and is not offered for sending.
inline void Node::cancel_export() noexcept
{
  m_flattening = false;
  m_copy_left = 0;
  if(m_writing)
    m_export_cancelled = true;
  layers.reserved = 0;
}

//! The recording's end is faded unless the overdub that went on from it is
//! heard over it. Once that overdub is gone for good, redo or not, the fade
//! stays. `new_pass`: a pass is about to start, which drops what redo would
//! bring back.
inline void Node::update_seam(bool new_pass)
{
  auto& s = m_seam;
  if(s.base_slot < 0 || s.pass_slot < 0)
    return;
  // Merged into another pass, or erased: what is in that buffer is not the
  // recording any more.
  if(layers.count == 0 || layers.layers[0].slot != s.base_slot)
  {
    s.base_slot = -1;
    return;
  }
  const int kept = new_pass ? layers.count : layers.total;
  bool there = false;
  for(int i = 1; i < kept; i++)
    there = there || layers.layers[i].slot == s.pass_slot;
  if(!there)
  {
    fade_seam(true);
    s.base_slot = -1;
    return;
  }
  const auto& over = layers.layers[1];
  fade_seam(!(layers.count > 1 && over.slot == s.pass_slot && !over.muted));
}

inline void Node::fade_seam(bool faded)
{
  auto& s = m_seam;
  if(s.faded == faded)
    return;
  s.faded = faded;
  constexpr int fade = Looper::audio_layers::fade_frames;
  const int64_t n = std::min<int64_t>(s.end, fade);
  if(n <= 0 || s.end > layers.capacity)
    return;
  for(int c = 0; c < layers.channels; c++)
  {
    float* d = layers.data(s.base_slot, c) + s.end - n;
    const float* k = s.kept.data() + c * fade;
    for(int64_t i = 0; i < n; i++)
      d[i] = faded ? k[i] * (1.f - float(i) / float(n)) : k[i];
  }
}

//! A small fade on the first and last samples of a recorded pass.
inline void Node::fade_edges(int slot, bool end)
{
  for(int c = 0; c < layers.channels; c++)
  {
    float* chan = layers.data(slot, c);
    const int64_t samples = layers.length;
    if(int64_t min_n = std::min(samples, int64_t(Looper::audio_layers::fade_frames));
       min_n > 0)
    {
      float f = 1.f / min_n;
      float ff = 0.f;
      for(int64_t i = 0; i < min_n; i++)
      {
        chan[i] *= ff;
        ff += f;
      }
      if(!end)
        continue;
      for(int64_t i = samples - min_n; i < samples; i++)
      {
        chan[i] *= ff;
        ff -= f;
      }
    }
  }
}

inline void Node::keep_input(int64_t frames)
{
  if(m_ring_capacity > 0)
  {
    auto& in = inputs.audio;
    for(int c = 0; c < m_ring_channels; c++)
    {
      float* ring = m_ring.data() + std::size_t(c) * std::size_t(m_ring_capacity);
      int64_t k = m_ring_written % m_ring_capacity;
      for(int64_t j = 0; j < frames; j++)
      {
        ring[k] = c < in.channels ? float(in.samples[c][j]) : 0.f;
        if(++k == m_ring_capacity)
          k = 0;
      }
    }
  }
  m_ring_written += frames;
}

//! Capture: the `n` frames of input up to `end` become the loop.
inline void Node::capture_into(int64_t end, int64_t n)
{
  cancel_export();
  layers.begin_recording();
  m_seam.base_slot = -1;
  const int slot = layers.layers[0].slot;
  for(int c = 0; c < layers.channels; c++)
  {
    float* dst = layers.data(slot, c);
    if(c >= m_ring_channels)
    {
      std::fill_n(dst, n, 0.f);
      continue;
    }
    const float* ring = m_ring.data() + std::size_t(c) * std::size_t(m_ring_capacity);
    for(int64_t i = 0; i < n; i++)
    {
      const float v = ring[(end - n + i) % m_ring_capacity];
      dst[i] = v;
      layers.note_peak(slot, i, v);
    }
  }
  layers.length = n;
  layers.layers[0].written = n;
  fade_edges(slot);
  m_full = false;
}

//! The input over the window, written into the pass just begun as an overdub
//! would have written it then: each position of the loop takes what came in
//! when the playhead went over it, faded in as a pass starts; its end is
//! faded and moved by the latency as the pass closes. Going over the loop's
//! end, the passes under it fade by the feedback.
inline void Node::capture_layer_into(const Looper::capture_window& w)
{
  if(layers.reserved_overwritten)
    cancel_export();
  auto* top = layers.top();
  if(!top)
    return;
  const int64_t len = layers.length;
  const int64_t positions = w.positions();
  const auto y_last = int64_t(std::floor(w.y_end));
  const int64_t first = w.end - w.n;
  const int fade = Looper::audio_layers::fade_frames;
  const auto input = [&](int c, double t) {
    if(c >= m_ring_channels)
      return 0.f;
    const float* ring = m_ring.data() + std::size_t(c) * std::size_t(m_ring_capacity);
    const auto at = [&](int64_t f) {
      return ring[std::clamp<int64_t>(f, first, w.end - 1) % m_ring_capacity];
    };
    const auto f = int64_t(std::floor(t));
    const float a = float(t - double(f));
    return at(f) + a * (at(f + 1) - at(f));
  };
  for(int64_t i = 0; i < positions; i++)
  {
    const int64_t y = y_last - positions + i;
    const double t = double(w.end) - (w.y_end - double(y)) / w.rate;
    const int64_t yw = Looper::capture_window::wrap(y, len);
    const int64_t k = w.backwards ? len - 1 - yw : yw;
    const float ramp = i < fade ? float(i) / float(fade) : 1.f;
    for(int c = 0; c < layers.channels; c++)
    {
      const float v = input(c, t) * ramp;
      layers.data(top->slot, c)[k] = v;
      layers.note_peak(top->slot, k, v);
    }
  }
  top->start = w.start();
  top->written = positions;
  top->backwards = w.backwards;
  if(y_last - positions < 0)
    layers.decay(std::clamp(inputs.feedback.value, 0.f, 1.f));
}

//! A new file replaces the loop. It is copied about a second per tick so that
//! a long file does not make one tick long, at the looper's rate: a file at
//! another rate is resampled linearly.
inline void Node::load_file()
{
  const auto& f = inputs.file.soundfile;
  if(f.data != m_file_data || f.frames != m_file_frames)
  {
    m_file_data = f.data;
    m_file_frames = f.frames;
    m_loading = false;

    if(!inputs.file || transport.is_taking())
      return;

    const double ratio = f.rate > 0 ? m_rate / f.rate : 1.;
    m_file_target = std::min<int64_t>(int64_t(f.frames * ratio), layers.capacity);
    if(m_file_target <= 0)
      return;
    cancel_export();
    layers.begin_recording();
    m_seam.base_slot = -1;
    m_switches.soloed.forget();
    // Nothing is heard until the whole file is in.
    layers.count = 0;
    m_file_pos = 0;
    m_loading = true;
    m_full = false;
  }

  if(!m_loading)
    return;

  const double step = f.rate > 0 ? double(f.rate) / m_rate : 1.;
  const int slot = layers.layers[0].slot;
  const int64_t end = std::min(m_file_target, m_file_pos + 65536);
  for(int c = 0; c < layers.channels; c++)
  {
    float* dst = layers.data(slot, c);
    // A mono file goes to every channel.
    const float* src = f.channels > 0 ? f.data[std::min(c, f.channels - 1)] : nullptr;
    for(int64_t k = m_file_pos; k < end; k++)
    {
      const double x = k * step;
      const int64_t i = int64_t(x);
      const float a = i < f.frames ? src[i] : 0.f;
      const float b = i + 1 < f.frames ? src[i + 1] : a;
      const float v = a + float(x - i) * (b - a);
      dst[k] = v;
      layers.note_peak(slot, k, v);
    }
  }
  m_file_pos = end;
  if(m_file_pos < m_file_target)
    return;

  m_loading = false;
  m_loop_tempo = m_tempo;
  layers.length = m_file_target;
  layers.layers[0].written = m_file_target;
  layers.count = 1;
  layers.total = 1;
  fade_edges(slot);
  transport.play_captured(0);
  m_origin = m_tick_start;
}

//! Plays the loop at the tempo of the score. The ratio is taken when the
//! loop starts a round, so that a tempo change is heard from the next round.
inline void Node::play_stretched(int64_t first_pos, int64_t samples)
{
  const int64_t len = layers.length;
  const int chans = layers.channels;
  if(std::ssize(m_stretch_out) < chans || std::ssize(m_stretch_span) < chans
     || samples > std::ssize(m_stretch_out[0]))
  {
    play(first_pos, samples);
    return;
  }
  if(!m_stretch_ready || transport.playbackPos % len != m_expected_pos
     || m_stretch_speed != m_speed || m_stretch_reverse != m_reverse)
  {
    // The stretchers read the tape at m_speed: their frame u is position
    // u * m_speed of it, which goes round every len / m_speed frames.
    m_stretch_speed = m_speed;
    m_stretch_reverse = m_reverse;
    const auto u = int64_t(head_y(len) / m_speed);
    m_stretcher.transport(u);
    m_heard = double(u);
    m_cycle_ratio = m_loop_tempo / std::max(1., m_tempo);
    m_stretch_ready = true;
  }

  for(int c = 0; c < chans; c++)
    m_stretch_span[c] = std::span<double>(m_stretch_out[c].data(), std::size_t(samples));

  ossia::token_request tk;
  tk.prev_date = ossia::time_value{0};
  tk.date = ossia::time_value{samples};
  m_wrapped = false;
  // Frames of output per frame of the loop, for this run.
  const double ratio = m_stretcher.stretch() ? m_cycle_ratio : 1.;
  m_stretcher.run(
      *this, tk, ossia_state, m_cycle_ratio, 1., std::size_t(chans), len, samples,
      samples, 0, m_stretch_span);
  if(m_wrapped)
    m_cycle_ratio = m_loop_tempo / std::max(1., m_tempo);

  auto& p2 = outputs.audio;
  const int loop_chans = std::min(chans, p2.channels);
  for(int c = 0; c < loop_chans; c++)
    for(int64_t j = 0; j < samples; j++)
      p2.samples[c][first_pos + j] = m_stretch_out[c][j];
  for(int c = loop_chans; c < p2.channels; c++)
    for(int64_t j = 0; j < samples; j++)
      p2.samples[c][first_pos + j] = 0.;

  // The stretcher reads ahead of what it outputs, by what it keeps buffered
  // and its own latency: the position is the one of what was heard, which an
  // overdub that carries on from here writes at.
  m_heard = std::fmod(
      m_heard + double(samples) / std::max(1e-9, ratio), double(len) / m_stretch_speed);
  if(m_stretch_speed == 1. && !m_stretch_reverse)
  {
    transport.playbackPos = int64_t(m_heard) % len;
    m_frac = 0.;
    m_head_pos = transport.playbackPos;
  }
  else
  {
    set_head_y(std::fmod(m_heard * m_stretch_speed, double(len)), len);
  }
  m_expected_pos = transport.playbackPos;
}

//! The playhead in the coordinates it goes forward in: the loop's own, or
//! the loop read from its end when it plays backwards.
inline double Node::head_y(int64_t len) const noexcept
{
  const double h = double(transport.playbackPos % len) + m_frac;
  double y = m_reverse ? double(len) - h : h;
  if(y >= double(len))
    y -= double(len);
  if(y < 0.)
    y += double(len);
  return y;
}

inline void Node::set_head_y(double y, int64_t len) noexcept
{
  double h = m_reverse ? double(len) - y : y;
  h = std::fmod(h, double(len));
  if(h < 0.)
    h += double(len);
  const double whole = std::floor(h);
  transport.playbackPos = int64_t(whole) % len;
  m_frac = h - whole;
  // A position that rounds to the loop's end is its start.
  if(m_frac >= 1. - 1e-9)
  {
    transport.playbackPos = (transport.playbackPos + 1) % len;
    m_frac = 0.;
  }
  m_head_pos = transport.playbackPos;
}

//! What a frame of output hears of the tape it goes over, [y; y + s[: faster
//! than x1, the positions of it averaged; slower, the two nearest positions
//! to its middle, interpolated. At x1 on a whole position, that position.
inline float Node::read_span(int c, double y, double s, int64_t len) const noexcept
{
  if(s > 1.)
  {
    const int n = int(std::lround(s));
    const auto k0 = int64_t(y);
    float sum = 0.f;
    for(int i = 0; i < n; i++)
      sum += tape(c, (k0 + i) % len, len);
    return sum / float(n);
  }
  const double centre = y + 0.5 * s - 0.5;
  const double whole = std::floor(centre);
  const double f = centre - whole;
  int64_t k = int64_t(whole) % len;
  if(k < 0)
    k += len;
  const float a = tape(c, k, len);
  if(f < 1e-9)
    return a;
  return a + float(f) * (tape(c, (k + 1) % len, len) - a);
}

//! Playback at another speed than x1, or backwards.
inline void Node::play_tape(int64_t first_pos, int64_t samples)
{
  auto& p2 = outputs.audio;
  const int64_t last = first_pos + samples;
  const int out_chans = p2.channels;
  const int loop_chans = std::min(layers.channels, out_chans);
  const int64_t len = layers.length;
  const double s = m_speed;
  double y = head_y(len);
  // Faster than x1, whole positions are gone over.
  if(s > 1.)
    y = std::floor(y);

  for(int64_t j = first_pos; j < last; j++)
  {
    for(int i = 0; i < loop_chans; i++)
      p2.samples[i][j] = read_span(i, y, s, len);
    y += s;
    while(y >= double(len))
      y -= double(len);
  }
  for(int i = loop_chans; i < out_chans; i++)
    for(int64_t j = first_pos; j < last; j++)
      p2.samples[i][j] = 0.;

  set_head_y(y, len);
}

inline void Node::play(int64_t first_pos, int64_t samples)
{
  auto& p2 = outputs.audio;
  const int64_t last = first_pos + samples;
  const int out_chans = p2.channels;
  const int loop_chans = std::min(layers.channels, out_chans);
  const int64_t len = layers.length;
  const int64_t start = transport.playbackPos % len;

  for(int i = 0; i < loop_chans; i++)
  {
    auto& out = p2.samples[i];
    // The loop is free to be shorter than the tick, and to end anywhere
    // inside it.
    int64_t k = start;
    for(int64_t j = first_pos; j < last; j++)
    {
      out[j] = layers.mix(i, k);
      if(++k == len)
        k = 0;
    }
  }

  // The bus can carry more channels than the loop was recorded with.
  for(int i = loop_chans; i < out_chans; i++)
  {
    auto& out = p2.samples[i];
    for(int64_t j = first_pos; j < last; j++)
      out[j] = 0.;
  }

  // The loop goes on with no output to play it to, e.g. nothing patched in.
  transport.playbackPos = (start + samples) % len;
}

//! Outlet buffers come from a pool and are resized, not cleared: writing
//! nothing leaves the previous tick's audio in them.
inline void Node::silence(int64_t first_pos, int64_t samples)
{
  auto& p2 = outputs.audio;
  const int64_t last = first_pos + samples;
  for(int i = 0; i < p2.channels; i++)
  {
    auto& out = p2.samples[i];
    for(int64_t j = first_pos; j < last; j++)
      out[j] = 0.;
  }
}

inline void Node::mix_input(int64_t first_pos, int64_t samples)
{
  auto& p1 = inputs.audio;
  auto& p2 = outputs.audio;
  const int chans = std::min(p1.channels, p2.channels);
  const int64_t last = first_pos + samples;
  for(int i = 0; i < chans; i++)
  {
    auto& in = p1.samples[i];
    auto& out = p2.samples[i];
    for(int64_t j = first_pos; j < last; j++)
      out[j] += in[j];
  }
}

inline void Node::through(int64_t first_pos, int64_t samples)
{
  auto& p1 = inputs.audio;
  auto& p2 = outputs.audio;
  const int chans = std::min(p1.channels, p2.channels);
  const int64_t last = first_pos + samples;
  for(int i = 0; i < chans; i++)
  {
    auto& in = p1.samples[i];
    auto& out = p2.samples[i];
    for(int64_t j = first_pos; j < last; j++)
      out[j] = in[j];
  }
  for(int i = chans; i < p2.channels; i++)
  {
    auto& out = p2.samples[i];
    for(int64_t j = first_pos; j < last; j++)
      out[j] = 0.;
  }
}

inline int64_t Node::record(int64_t first_pos, int64_t samples, bool echo)
{
  auto& p1 = inputs.audio;
  if(!transport.is_taking())
  {
    // A new recording: the passes go, and an export of them that was still
    // being flattened would read the new one.
    cancel_export();
    layers.begin_recording();
    m_seam.base_slot = -1;
    m_switches.soloed.forget();
    m_seamless = false;
    transport.playbackPos = 0;
    m_export_pending = false;
    m_full = false;
    m_loading = false;
  }
  transport.taking(LoopMode::Record);

  auto& base = layers.layers[0];
  const int64_t pos = layers.length;
  const int64_t room = std::max<int64_t>(0, layers.capacity - pos);
  const int64_t n = std::min(samples, room);
  const int chans = std::min(p1.channels, layers.channels);

  for(int c = 0; c < layers.channels; c++)
  {
    float* dst = layers.data(base.slot, c) + pos;
    if(c < chans)
    {
      auto& in = p1.samples[c];
      for(int64_t j = 0; j < n; j++)
      {
        const float v = float(in[first_pos + j]);
        dst[j] = v;
        layers.note_peak(base.slot, pos + j, v);
      }
    }
    else
    {
      std::fill_n(dst, n, 0.f);
    }
  }
  layers.length += n;
  base.written = layers.length;
  transport.playbackPos = layers.length;

  echo ? through(first_pos, n) : silence(first_pos, n);

  if(n < samples)
  {
    // The buffer is full: the loop is what fitted, and the post-action takes
    // over for the rest of the span.
    m_full = true;
    transport.end_recording(inputs.after_record);
    transport.playbackPos = 0;
  }
  return n;
}

//! Position y of the tape, going the way the playhead goes, is written with
//! v: replacing what the pass had not written yet, over what it wrote on its
//! earlier rounds, faded by the feedback.
inline void Node::write_tape(int64_t y, const float* v, float feedback)
{
  if(std::exchange(m_cycle_pending, false))
  {
    // The pass of the cycle that ended is closed as any pass is.
    close_pass(true);
    layers.reserved_overwritten = false;
    update_seam(true);
    begin_pass(m_reverse ? layers.length - 1 - y : y);
    if(layers.reserved_overwritten)
      cancel_export();
    // Faded in as any pass.
    m_pass_frames = 0;
  }
  auto* t = layers.top();
  if(!t)
    return;
  auto& top = *t;
  const int64_t len = layers.length;
  const int64_t k = m_reverse ? len - 1 - y : y;
  const bool first_time = !layers.valid(top, k);
  const int fade = Looper::audio_layers::fade_frames;
  const float ramp
      = m_pass_frames < fade ? float(m_pass_frames) / fade : 1.f;
  m_pass_frames++;
  for(int c = 0; c < layers.channels; c++)
  {
    float& d = layers.data(top.slot, c)[k];
    // Later laps of the pass fade what it laid down on the earlier ones.
    d = (first_time ? 0.f : d * feedback) + v[c] * ramp;
    layers.note_peak(top.slot, k, d);
  }
  layers.cover(top, k);
  top.backwards = m_reverse;
}

//! The overdub went past the last position of the loop. An extending pass
//! makes the loop longer, else the playhead goes round, the passes under it
//! fade by the feedback, and with a pass per cycle the next round is a pass
//! of its own. True when the playhead goes round.
inline bool Node::cycle_end(int64_t& len, bool extend, float feedback)
{
  // A cycle is the loop's length when the overdub started: the loop growing
  // by one is a cycle gone round as well.
  const bool per_cycle = inputs.passes.value == Looper::Passes::PerCycle;
  if(extend && layers.extend(m_extend_unit))
  {
    len = layers.length;
    m_extended = true;
    if(per_cycle)
      m_cycle_pending = true;
    return false;
  }
  layers.decay(feedback);
  if(per_cycle)
    m_cycle_pending = true;
  return true;
}

//! An overdub starts where the playhead is. A recording that just closed on
//! a grid point it was cut back to, the playhead going on from that point,
//! hands it what was recorded since: the pass starts on the grid point with
//! it, faded in as a pass starts.
inline void Node::begin_pass_carried(int64_t len)
{
  const int64_t span = m_carried_span;
  m_carried_span = 0;
  // A pass that carries on a recording starts at full level: it is not faded
  // in, as if it had already written its fade.
  const bool seamless = std::exchange(m_seamless, false);
  const int fade = seamless ? 0 : Looper::audio_layers::fade_frames;
  m_pass_frames = seamless ? Looper::audio_layers::fade_frames : 0;
  const int64_t k = transport.playbackPos % len;
  const bool carried = span > 0 && span < len && k == span && layers.count == 1
                       && m_speed == 1. && !m_reverse
                       && len + span <= layers.capacity;
  update_seam(true);
  begin_pass(carried ? 0 : k);
  auto* top = layers.top();
  if(seamless && top)
    m_seam.pass_slot = top->slot;
  if(!carried || !top || layers.count != 2)
    return;
  const auto& base = layers.layers[0];
  for(int64_t i = 0; i < span; i++)
  {
    const float ramp = i < fade ? float(i) / fade : 1.f;
    for(int c = 0; c < layers.channels; c++)
    {
      const float v = layers.data(base.slot, c)[len + i] * ramp;
      layers.data(top->slot, c)[i] = v;
      layers.note_peak(top->slot, i, v);
    }
    layers.cover(*top, i);
  }
  m_pass_frames = std::max<int64_t>(m_pass_frames, span);
}

inline void Node::overdub(int64_t first_pos, int64_t samples, bool echo)
{
  auto& p1 = inputs.audio;
  auto& p2 = outputs.audio;
  const int64_t last = first_pos + samples;

  // Nothing to layer onto: an overdub of an empty loop is an echo.
  if(layers.empty())
  {
    echo ? through(first_pos, samples) : silence(first_pos, samples);
    return;
  }

  const int in_chans = std::min(p1.channels, layers.channels);
  const int out_chans = p2.channels;
  auto input = [&](int c, int64_t j) {
    return c < in_chans ? float(p1.samples[c][j]) : 0.f;
  };

  int64_t len = layers.length;
  if(!transport.is_taking())
  {
    layers.reserved_overwritten = false;
    begin_pass_carried(len);
    // No buffer was left but one a take being written still reads.
    if(layers.reserved_overwritten)
      cancel_export();
    // An extending pass grows the loop by the loop it started over.
    m_extend_unit = len;
    m_extended = false;
    m_cycle_pending = false;
    m_acc_n = 0;
    for(int c = 0; c < layers.channels; c++)
    {
      m_acc[c] = 0.f;
      m_prev_in[c] = input(c, first_pos);
    }
  }
  transport.taking(LoopMode::Overdub);
  // Backwards, the playhead reaches the loop's start, not its end.
  const bool extend = inputs.past_end.value == Looper::PastEnd::Extend && !m_reverse;

  if(!layers.top())
    return;

  const float feedback = std::clamp(inputs.feedback.value, 0.f, 1.f);
  const double s = m_speed;
  double y = head_y(len);
  if(s >= 1.)
    y = std::floor(y);
  const int steps = s >= 1. ? int(std::lround(s)) : 1;
  std::array<float, Looper::audio_layers::max_channels> v{};

  for(int64_t j = first_pos; j < last; j++)
  {
    for(int c = 0; c < layers.channels; c++)
    {
      const float in = input(c, j);
      const float before = read_span(c, y, s, len);
      if(c < out_chans)
        p2.samples[c][j] = echo ? before + in : before;
    }
    for(int c = layers.channels; c < out_chans; c++)
      p2.samples[c][j] = 0.;

    if(s >= 1.)
    {
      // Each frame goes over `steps` positions: the input between the last
      // frame and this one, interpolated, the last of them this frame's.
      auto w = int64_t(y);
      for(int i = 0; i < steps; i++)
      {
        const float t = float(i + 1) / float(steps);
        for(int c = 0; c < layers.channels; c++)
          v[c] = steps == 1 ? input(c, j) : m_prev_in[c] + t * (input(c, j) - m_prev_in[c]);
        write_tape(w, v.data(), feedback);
        if(++w == len && cycle_end(len, extend, feedback))
          w = 0;
      }
      y = double(w);
    }
    else
    {
      // A position takes several frames: what came in over them, averaged,
      // once the playhead leaves it.
      for(int c = 0; c < layers.channels; c++)
        m_acc[c] += input(c, j);
      m_acc_n++;
      const auto k = int64_t(y);
      y += s;
      if(y >= double(k + 1) - 1e-9)
      {
        for(int c = 0; c < layers.channels; c++)
        {
          v[c] = m_acc[c] / float(m_acc_n);
          m_acc[c] = 0.f;
        }
        m_acc_n = 0;
        write_tape(k, v.data(), feedback);
        if(k + 1 == len && cycle_end(len, extend, feedback))
          y -= double(len);
      }
    }
    for(int c = 0; c < layers.channels; c++)
      m_prev_in[c] = input(c, j);
  }
  set_head_y(y, len);
}

inline void Node::start_export_item(int item)
{
  m_export_item = item;
  m_flattening = true;
  m_flatten_pos = 0;
}

//! A take of `passes` passes: their buffers are reserved until it is written,
//! and a pass still taken in, `open`, is copied as it goes on.
inline void Node::start_export(int passes, bool open, bool recording)
{
  m_export_length = layers.length;
  m_export_channels = layers.channels;
  // The passes as they are now: gains change with feedback, the loop's
  // length with undo, and new passes come while the files are written.
  m_export_count = passes;
  std::copy_n(layers.layers.begin(), layers.count, m_export_snapshot.begin());
  layers.reserved = 0;
  for(int i = 0; i < m_export_count; i++)
    layers.reserved |= uint64_t(1) << m_export_snapshot[i].slot;
  start_export_item(m_export_first);
  if(open && !recording)
    copy_open_pass();
}

inline void Node::continue_export()
{
  if(!m_flattening)
    return;

  if(m_copy_left > 0)
  {
    const int64_t n = std::min(
        m_copy_left,
        std::max<int64_t>(65536, work_per_tick / std::max(1, m_export_channels)));
    for(int c = 0; c < m_export_channels; c++)
    {
      const float* src = layers.data(m_copy_from, c);
      float* dst = layers.data(m_copy_to, c);
      int64_t q = m_copy_pos;
      for(int64_t i = 0; i < n; i++)
      {
        dst[q] = src[q];
        q += m_copy_dir;
        if(q >= m_copy_period)
          q = 0;
        else if(q < 0)
          q = m_copy_period - 1;
      }
    }
    m_copy_pos = (m_copy_pos + m_copy_dir * n) % m_copy_period;
    if(m_copy_pos < 0)
      m_copy_pos += m_copy_period;
    m_copy_left -= n;
    if(m_copy_left > 0)
      return;
  }

  // A bounded amount of work per tick: a minute-long loop of many passes and
  // channels does not make one tick a minute of work.
  const int64_t per_frame
      = int64_t(m_export_channels) * (m_export_item < 0 ? std::max(1, m_export_count) : 1);
  const int64_t chunk = std::max<int64_t>(1024, work_per_tick / std::max<int64_t>(1, per_frame));
  const int64_t end = std::min(m_export_length, m_flatten_pos + chunk);
  flatten(
      m_export_snapshot.data(), m_export_count, m_export_item, m_export_length,
      m_flatten_pos, end);
  m_flatten_pos = end;

  if(m_flatten_pos < m_export_length)
    return;

  m_flattening = false;
  m_writing = true;

  Looper::take_job job;
  job.writer = writer;
  job.audio = export_audio;
  job.slot = m_export_slot;
  job.number = m_export_number;
  job.layer = m_export_item;
  job.channels = m_export_channels;
  job.frames = m_export_length;
  job.rate = int(m_rate);
  job.tempo = m_export_tempo;
  job.meter_numerator = m_export_signature.upper;
  job.meter_denominator = m_export_signature.lower;
  export_audio->in_use = true;
  worker.request(std::move(job));
}

//! Frames [from; to[ of a take of `count` passes into the export buffer: what
//! is heard of them (item -1), or of one of them.
inline void Node::flatten(
    const Looper::audio_layer* passes, int count, int item, int64_t len, int64_t from,
    int64_t to)
{
  const auto data = [this](int slot, int c) -> const float* { return layers.data(slot, c); };
  for(int c = 0; c < m_export_channels; c++)
  {
    float* dst = export_audio->channel(c);
    if(item < 0)
    {
      Looper::mix_passes(data, passes, count, c, len, from, to, dst);
    }
    else
    {
      // A muted pass has its file, as it would be heard.
      const auto& l = passes[item];
      const float* src = layers.data(l.slot, c);
      for(int64_t k = from; k < to; k++)
        dst[k] = Looper::audio_layers::valid_in(l, k, len)
                     ? l.unmuted_gain * src[Looper::audio_layers::index_in(l, k, len)]
                     : 0.f;
    }
  }
}

//! The take has the overdub going on: its pass is copied into a free buffer,
//! from a little behind where it writes and on the way the playhead goes,
//! faster than the playhead: every position is copied before the pass writes
//! it again. Without a free buffer, the take reads the pass as it goes on.
inline void Node::copy_open_pass()
{
  auto& l = m_export_snapshot[m_export_count - 1];
  int spare = -1;
  for(int s = 0; s < layers.buffers() && spare < 0; s++)
    if(!layers.slot_used(s) && !layers.is_reserved(s))
      spare = s;
  const int64_t period = Looper::audio_layers::period_in(l, m_export_length);
  if(spare < 0 || period <= 0)
    return;
  layers.reserved |= uint64_t(1) << spare;
  m_copy_from = l.slot;
  m_copy_to = spare;
  l.slot = spare;
  m_copy_period = period;
  m_copy_left = period;
  m_copy_dir = m_reverse ? -1 : 1;
  const int64_t head = transport.playbackPos % std::max<int64_t>(1, m_export_length) % period;
  const int64_t behind = std::min<int64_t>(period / 2, 8192);
  m_copy_pos = ((head - m_copy_dir * behind) % period + period) % period;
}

//! The take the score's stop leaves: the mix and each pass, which its job
//! flattens from the passes as they are now and the memory they are in,
//! which it keeps: the looper may be gone by then. Their buffers are kept
//! from new passes until it is written. It is skipped when the one before it
//! is still being written.
inline void Node::write_stop_take(bool recording, double date, int64_t)
{
  auto& s = *stop_take;
  if(!writer || layers.empty() || s.in_use)
    return;

  auto& src = s.passes;
  src.pool = layers.pool;
  std::copy_n(layers.layers.begin(), layers.count, src.passes.begin());
  src.count = layers.count;
  src.channels = layers.channels;
  src.capacity = layers.capacity;
  src.length = layers.length;
  for(int i = 0; i < layers.count; i++)
    layers.held |= uint64_t(1) << layers.layers[i].slot;
  // The views are shown every pass, as the take has them.
  scan_peaks(true);
  prepare_stop_notice(s.notice, date);

  Looper::take_job job;
  job.writer = writer;
  job.stop = stop_take;
  job.slot = claim_take_slot();
  job.alone = true;
  job.channels = layers.channels;
  job.frames = layers.length;
  job.rate = int(m_rate);
  job.tempo = recording ? m_tempo : m_loop_tempo;
  job.meter_numerator = m_signature.upper;
  job.meter_denominator = m_signature.lower;
  s.in_use = true;
  worker.request(std::move(job));
}

inline void Node::send_view(int64_t frames)
{
  if(!view_due(frames))
    return;
  fill_message(m_view);
  send_message(m_view);
}

//! What the view is sent of the loop: the peaks of its passes, in a lane
//! per pass, from the last complete scan of them.
inline void Node::fill_message(Looper::view_message& v)
{
  fill_view(v);
  v.max_seconds = float(layers.capacity / m_rate);
  scan_peaks();
  if(&v != &m_view)
  {
    v.lanes = m_view.lanes;
    v.peaks = m_view.peaks;
  }
}

//! The overview: one lane per pass, the oldest merged into the first when
//! there are more passes than lanes. A long loop of many passes is gone
//! through over several messages, the view keeping the last complete one,
//! unless the scan is `whole`.
inline void Node::scan_peaks(bool whole)
{
  using layers_t = Looper::audio_layers;
  constexpr int bins = Looper::view_message::bins;
  const int count = layers.count;
  const int64_t len = layers.length;
  const int lanes = std::min(count, Looper::view_message::max_lanes);
  if(count != m_scan_count || len != m_scan_length || layers.changes != m_scan_changes)
  {
    m_scan_count = count;
    m_scan_length = len;
    m_scan_changes = layers.changes;
    m_scan_layer = 0;
    m_scan_pos = 0;
    std::fill(m_scan_peaks.begin(), m_scan_peaks.end(), 0.f);
  }

  const int64_t micro = (len + layers_t::peak_frames - 1) / layers_t::peak_frames;
  int64_t budget = whole ? std::numeric_limits<int64_t>::max() / 2 : work_per_tick / 8;
  while(m_scan_layer < count && budget > 0)
  {
    const int i = m_scan_layer;
    const int lane = std::max(0, i - (count - lanes));
    const auto& l = layers.layers[i];
    const float* p = layers.peaks_of(l.slot);
    float* dst = m_scan_peaks.data() + lane * bins;
    // A pass shorter than the loop plays again over the rest of it.
    const int64_t own = std::max<int64_t>(
        1, (layers.period_of(l) + layers_t::peak_frames - 1) / layers_t::peak_frames);
    const int64_t end = std::min(micro, m_scan_pos + budget);
    // A muted pass of a lane of its own is drawn as it would be heard, the
    // view dims it; in the lane of the oldest passes, it is left out.
    const float gain = count > lanes && i <= count - lanes ? l.gain : l.unmuted_gain;
    for(int64_t m = m_scan_pos; m < end; m++)
    {
      const int b = int(m * bins / micro);
      dst[b] = std::max(dst[b], p[m % own] * gain);
    }
    budget -= end - m_scan_pos;
    m_scan_pos = end;
    if(m_scan_pos >= micro)
    {
      m_scan_layer++;
      m_scan_pos = 0;
    }
  }

  if(m_scan_layer < count)
    return;
  m_view.lanes = lanes;
  m_view.peaks = m_scan_peaks;
  // The next message goes through the passes again: gains change as an
  // overdub goes round, and what it records with them.
  m_scan_layer = 0;
  m_scan_pos = 0;
  std::fill(m_scan_peaks.begin(), m_scan_peaks.end(), 0.f);
}
}
