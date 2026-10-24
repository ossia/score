#pragma once
#include <Fx/Looper/AudioLayers.hpp>
#include <Fx/Looper/LooperView.hpp>
#include <Fx/Looper/TakeWriter.hpp>
#include <Fx/Looper/Sync.hpp>
#include <Fx/Looper/Transport.hpp>
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
}

namespace Nodes::AudioLooper
{
struct Node
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

  using LoopMode = Looper::LoopMode;
  using Postaction = Looper::Postaction;
  using Restart = Looper::Restart;
  using Passthrough = Looper::Passthrough;

  struct ins
  {
    halp::dynamic_audio_bus<"in", double> audio;

    struct : halp::enum_t<LoopMode, "Loop">
    {
      using halp::enum_t<LoopMode, "Loop">::operator=;
      halp_meta(
          description,
          "What the looper is doing: playing the loop, recording a new one over "
          "it, overdubbing on top of it, or nothing.")
    } mode;

    struct : quant_selector<"Quantif">
    {
      halp_meta(
          description,
          "Waits for the next point of this grid before changing mode, so a "
          "loop starts and ends in time. Whole is a bar. None changes at once.")
    } quantif;

    struct : Looper::enum_combobox<"Passthrough", Passthrough, Passthrough::Record>
    {
      using enum_combobox::operator=;
      halp_meta(
          description,
          "Whether the input is heard as well as the loop. Record passthrough "
          "is heard only while recording or overdubbing; Full is heard always.")
    } passthrough;

    struct : halp::enum_t<Postaction, "Post-action">
    {
      using halp::enum_t<Postaction, "Post-action">::operator=;
      halp_meta(
          description,
          "What recording gives way to once it has run for Bars bars, without "
          "the mode being touched.")
    } postaction;

    struct : halp::spinbox_i32<"Bars", halp::irange{0, 64, 4}>
    {
      halp_meta(
          description,
          "How long a recording runs before the post-action takes over, and so "
          "how long the loop it leaves is. Zero leaves recording to be ended by "
          "hand, and the loop is then as long as it was played for.")
    } postaction_bars;

    struct : halp::enum_t<Restart, "Restart">
    {
      using halp::enum_t<Restart, "Restart">::operator=;
      halp_meta(
          description,
          "Where the loop is read from when the mode changes. Always reads it "
          "from the beginning again; Recording lets playing and overdubbing "
          "carry on from where they were.")
    } restart;

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
          "Passes kept apart for undo. Past it, the two oldest are merged. "
          "Applies from the next start.")
    } max_layers;

    struct : halp::spinbox_i32<"Channels", halp::irange{1, 16, 2}>
    {
      halp_meta(
          description,
          "Channels recorded; the input's other channels are not. Applies from "
          "the next start.")
    } channels;

    struct : Looper::enum_combobox<"Sync", Looper::SyncGroup, Looper::SyncGroup::None>
    {
      using enum_combobox::operator=;
      halp_meta(
          description,
          "Loopers of one group play together: the first to record sets the "
          "loop, the loops recorded after it are a whole number of it or a "
          "half, quarter or eighth of it, in phase with it.")
    } sync;

    struct : halp::spinbox_i32<"Keep last (s)", halp::irange{0, 600, 0}>
    {
      halp_meta(
          description,
          "Seconds of input kept at all times, for Capture. Zero keeps none. "
          "Applies from the next start.")
    } keep;

    struct : halp::spinbox_i32<"Capture bars", halp::irange{1, 64, 4}>
    {
      halp_meta(description, "How many bars Capture takes")
    } capture_bars;

    struct : halp::impulse_button<"Capture">
    {
      halp_meta(
          description,
          "Makes a loop of the last bars of the input, recorded or not, and plays "
          "it at once. Needs Keep last.")
    } capture;

    struct : halp::impulse_button<"Send to timeline">
    {
      halp_meta(
          description,
          "Places the last take in the timeline, where it was recorded, as a "
          "looping sound file.")
    } send;

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

    struct : Looper::enum_combobox<"Takes", Looper::TakeFiles, Looper::TakeFiles::Mix>
    {
      using enum_combobox::operator=;
      halp_meta(
          description,
          "What a take is written as: the loop as it is heard (Mix), each pass "
          "in a file of its own (Layers), or both. Sending or dragging a take "
          "places every file of it, each on a lane of its own.")
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

    struct : Looper::enum_combobox<"Follow tempo", Looper::FollowTempo, Looper::FollowTempo::Off>
    {
      using enum_combobox::operator=;
      halp_meta(
          description,
          "Plays the loop at the tempo of the score, as sound files are: "
          "resampled (Repitch: the pitch follows), or time-stretched (the "
          "pitch stays). A loop being recorded or overdubbed plays at its own "
          "speed, and a tempo change applies from the next round of the loop. "
          "Applies from the next start.")
    } follow_tempo;
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
    static std::function<void(Node&)> work(Looper::take_job&& job)
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
  Looper::audio_layers layers;
  std::shared_ptr<Looper::take_writer> writer;
  std::shared_ptr<Looper::export_buffer> export_audio;

  Node()
  {
    layers.allocate(2, 8, 60 * 48000);
    export_audio = Looper::export_buffer::make(layers.channels, layers.capacity);
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
    prepare_stretch(rate, s.frames);
    if(ossia_document_context)
      writer = Looper::take_writer::make(*ossia_document_context, "Looper", "wav");
  }

  //! Main thread only: the stretcher allocates.
  void prepare_stretch(double rate, int frames)
  {
    m_stretch_mode = Looper::stretch_mode(inputs.follow_tempo.value);
    m_stretcher.reset(0, m_stretch_mode, std::size_t(layers.channels), std::size_t(rate), std::size_t(rate));
    m_stretch_out.resize(std::size_t(layers.channels));
    for(auto& c : m_stretch_out)
      c.assign(std::size_t(std::max(frames, 8192)), 0.);
    m_stretch_ready = false;
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
  void looper_close_take(
      const ossia::token_request& tk, LoopMode mode, bool ended_by_postaction);
  void looper_trim_recording(int64_t frames)
  {
    layers.length = std::max<int64_t>(0, layers.length - frames);
    if(auto* l = layers.top())
      l->written = layers.length;
  }

  using tick = ossia::token_request;
  void operator()(const tick& tk)
  {
    if(tk.date == tk.prev_date)
      return;

    m_rate = ossia_state.sampleRate();
    m_tempo = tk.tempo;
    m_signature = tk.signature;
    m_in_chans = std::min<int>(inputs.audio.channels, layers.channels);

    sync.set_group(inputs.sync.value);
    apply_buttons();
    if(inputs.capture.value)
      capture(tk);
    load_file();

    m_mode = transport.effective(inputs.mode, layers.empty());
    const Looper::transport_settings settings{
        .mode = m_mode,
        .quantif = inputs.quantif.value,
        .postaction = inputs.postaction,
        .postaction_bars = inputs.postaction_bars,
        .restart = inputs.restart,
        .late_seconds = inputs.late.value / 1000.};
    transport(tk, ossia_state, settings, *this);
    keep_input(ossia_state.timings(tk).length);

    run_export();

    outputs.phase.value = layers.length > 0
                              ? float(transport.playbackPos % layers.length)
                                    / float(layers.length)
                              : 0.f;
    send_view(ossia_state.timings(tk).length);
  }

  //! Stopping the process keeps the loop: only the transport starts over.
  void stop()
  {
    transport.reset();
    m_view_countdown = 0;
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
          halp::item<&ins::sync> sync;
        } length;
        struct
        {
          halp_meta(name, "Play")
          halp_meta(layout, halp::layouts::hbox)
          halp::item<&ins::passthrough> passthrough;
          halp::item<&ins::restart> restart;
          halp::item<&ins::feedback> feedback;
          halp::item<&ins::past_end> past_end;
          halp::item<&ins::follow_tempo> follow_tempo;
        } play;
        struct
        {
          halp_meta(name, "Capture")
          halp_meta(layout, halp::layouts::hbox)
          halp::item<&ins::capture> capture;
          halp::item<&ins::capture_bars> capture_bars;
          halp::item<&ins::keep> keep;
        } capture;
        struct
        {
          halp_meta(name, "Memory")
          halp_meta(layout, halp::layouts::hbox)
          halp::item<&ins::max_seconds> max_seconds;
          halp::item<&ins::max_layers> max_layers;
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

  // Public for the tests: the audio of one pass.
  const float* layer_data(int layer, int chan) const noexcept
  {
    return layers.data(layers.layers[layer].slot, chan);
  }

private:
  double m_rate{48000.};
  double m_tempo{120.};
  ossia::time_signature m_signature{4, 4};
  int m_in_chans{};

  // Export of a take: the passes are flattened into the export buffer a
  // bounded number of frames per tick, then a worker writes it.
  bool m_export_pending{};
  bool m_flattening{};
  bool m_writing{};
  int64_t m_flatten_pos{};
  int64_t m_export_length{};
  int m_export_channels{};
  double m_export_tempo{120.};
  //! Tempo the loop was recorded at: the loop keeps its own speed.
  double m_loop_tempo{120.};
  ossia::time_signature m_export_signature{4, 4};
  double m_export_date{};
  //! The mode asked for, as the transport takes it up.
  LoopMode m_mode{LoopMode::Stop};
  double m_job_date{};
  int m_take_slot{};
  // The files of the take being written: the mix (item -1), then the layers,
  // one after the other under one take number.
  int m_export_item{};
  int m_export_layers{};
  bool m_export_mix{};
  bool m_export_cancelled{};
  int m_export_number{};
  std::array<Looper::audio_layer, Looper::audio_layers::max_layers> m_export_snapshot{};
  int m_takes_written{};
  double m_last_take_date{};
  bool m_send_requested{};
  bool m_full{};
  int64_t m_extend_unit{};
  bool m_extended{};

  // Playing the loop at the tempo of the score, with the stretchers of the
  // sound files reading the loop as their source.
  ossia::resampler m_stretcher;
  ossia::audio_stretch_mode m_stretch_mode{ossia::audio_stretch_mode::None};
  std::vector<std::vector<double>> m_stretch_out;
  double m_cycle_ratio{1.};
  bool m_stretch_ready{};
  int64_t m_expected_pos{};
  bool m_wrapped{};
  void play_stretched(int64_t first_pos, int64_t samples);

public:
  //! What the stretchers read: the loop as it is heard, round and round.
  template <typename T>
  void fetch_audio(int64_t start, int64_t frames, T** out) noexcept
  {
    const int64_t len = layers.length;
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

  int64_t m_view_countdown{};
  Looper::view_message m_view{};

  void apply_buttons();
  void capture(const tick& tk);
  void load_file();

  // A file being copied into the loop, a part per tick.
  const float** m_file_data{};
  int64_t m_file_frames{};
  int64_t m_file_pos{};
  int64_t m_file_target{};
  bool m_loading{};
  void keep_input(int64_t frames);
  void fade_edges(int slot);

  // The input of the last seconds, for Capture.
  boost::container::vector<float> m_ring;
  int64_t m_ring_capacity{};
  int m_ring_channels{};
  //! Frames of input seen since the looper was created.
  int64_t m_ring_written{};
  void run_export();
  void take_written(int slot, int number, bool ok);
  void start_export_item(int item);
  void send_view(int64_t frames);

  void play(int64_t first_pos, int64_t samples);
  void silence(int64_t first_pos, int64_t samples);
  void mix_input(int64_t first_pos, int64_t samples);
  void through(int64_t first_pos, int64_t samples);
  int64_t record(int64_t first_pos, int64_t samples, bool echo);
  void overdub(int64_t first_pos, int64_t samples, bool echo);
};

inline void Node::apply_buttons()
{
  if(inputs.clear.value)
  {
    sync.release();
    m_loading = false;
    layers.clear();
    transport.playbackPos = 0;
    // Whatever was being taken in is gone with the rest.
    if(transport.is_taking())
      transport.taking(LoopMode::Stop);
    m_full = false;
  }

  if(inputs.undo.value)
  {
    if(transport.is_taking())
    {
      // The pass in progress is dropped; taking in starts a new one.
      if(transport.actualMode == LoopMode::Record)
        layers.clear();
      else
        layers.undo(), layers.total = layers.count;
      transport.taking(LoopMode::Stop);
      if(transport.actualMode == LoopMode::Record)
        transport.playbackPos = 0;
    }
    else
    {
      layers.undo();
    }
  }

  if(inputs.redo.value && !transport.is_taking())
    layers.redo();

  if(inputs.send.value)
    m_send_requested = true;
}

inline int64_t Node::looper_run(int64_t start, int64_t length)
{
  // Stretching starts again from where the loop is, the next time it plays.
  if(transport.actualMode != LoopMode::Play)
    m_stretch_ready = false;

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

inline void Node::looper_close_take(
    const ossia::token_request& tk, LoopMode mode, bool ended_by_postaction)
{
  auto* top = layers.top();
  if(!top || layers.length <= 0)
    return;

  const int64_t latency
      = int64_t(inputs.latency.value * m_rate / 1000.)
        + (inputs.engine_latency.value
               ? int64_t(ossia_state.inputLatency()) + ossia_state.outputLatency()
               : 0);

  if(mode == LoopMode::Record)
  {
    m_loop_tempo = transport.loop_tempo(tk, layers.length);
    const double bar_samples = m_rate * 4.
                               * (double(tk.signature.upper) / tk.signature.lower)
                               * (60. / tk.tempo);
    const int64_t total_samples
        = int64_t(std::floor(transport.postaction_bars * bar_samples));

    // The bar count says when the post-action takes over, so it is the length
    // of a recording the post-action ended -- trimming the rounding off the
    // last bar. A recording stopped by hand is as long as it was played for,
    // and stretching it out to that count is how a loop ended early came back
    // with bars of silence at the end of it.
    const bool quantify_length = ended_by_postaction && transport.quantif > 0.f
                                 && transport.postaction_bars > 0;
    if(quantify_length && total_samples < layers.length)
      layers.length = total_samples;

    // In a sync group, the loop fits the group's.
    const auto synced
        = sync.close(layers.length, transport.engine_frame(tk), transport.playbackPos);
    if(synced.length < layers.length)
      layers.length = synced.length;
    const int64_t sync_extend = std::min(synced.length, layers.capacity);

    if(latency > 0)
      layers.shift_earlier(*top, latency);

    fade_edges(top->slot);

    // If there are less samples than expected we extend
    if(quantify_length && total_samples > layers.length)
    {
      const int64_t end = std::min(total_samples, layers.capacity);
      for(int c = 0; c < layers.channels; c++)
        std::fill(layers.data(top->slot, c) + layers.length, layers.data(top->slot, c) + end, 0.f);
      layers.length = end;
    }
    if(sync_extend > layers.length)
    {
      for(int c = 0; c < layers.channels; c++)
        std::fill(layers.data(top->slot, c) + layers.length, layers.data(top->slot, c) + sync_extend, 0.f);
      layers.length = sync_extend;
    }
    if(sync.current())
      transport.playbackPos = synced.position % std::max<int64_t>(1, layers.length);

    top->written = layers.length;
    m_export_date = transport.recordStartDate.impl / double(ossia::flicks_per_second<int64_t>);
  }
  else
  {
    // An extending pass asked to stop just after the loop grew ends on the
    // old end, as a late stop would.
    if(m_extended && m_extend_unit > 0)
    {
      const int64_t into_last = transport.playbackPos - (layers.length - m_extend_unit);
      if(into_last >= 0 && into_last <= int64_t(inputs.late.value * m_rate / 1000.)
         && layers.length - m_extend_unit >= m_extend_unit)
      {
        layers.length -= m_extend_unit;
        top->written
            = std::clamp<int64_t>(layers.length - top->start, 0, top->written);
        transport.playbackPos = into_last;
      }
    }
    m_extended = false;
    layers.top()->loop_after = layers.length;
    layers.fade_out_top();
    if(latency > 0)
      layers.shift_earlier(*top, latency);
  }

  m_export_tempo = m_loop_tempo;
  m_export_signature = tk.signature;
  m_export_pending = true;
}

//! A small fade on the first and last samples of a recorded pass.
inline void Node::fade_edges(int slot)
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

//! The last bars heard become the loop, ending on the last grid point when it
//! was just passed, as a late stop would.
inline void Node::capture(const tick& tk)
{
  if(transport.is_taking() || m_ring_capacity <= 0)
    return;

  const double bar = m_rate * 4. * (double(tk.signature.upper) / tk.signature.lower)
                     * (60. / std::max(1., tk.tempo));
  int64_t end = m_ring_written;
  if(transport.quantif > 0.f && transport.lastPointFrame)
  {
    const int64_t late = transport.frame - *transport.lastPointFrame;
    if(late >= 0 && late <= int64_t(inputs.late.value * m_rate / 1000.))
      end -= late;
  }
  const int64_t n = std::min<int64_t>(
      {int64_t(inputs.capture_bars.value * bar), end, m_ring_capacity, layers.capacity});
  if(n <= 0)
    return;

  layers.begin_recording();
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
  m_flattening = false;
  transport.play_captured((m_ring_written - end) % n);

  const double model_per_frame = ossia_state.samplesToModel();
  m_export_date = (tk.prev_date.impl - (m_ring_written - end + n) * model_per_frame)
                  / double(ossia::flicks_per_second<int64_t>);
  m_loop_tempo = tk.tempo;
  m_export_tempo = m_loop_tempo;
  m_export_signature = tk.signature;
  m_export_pending = true;
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
    layers.begin_recording();
    // Nothing is heard until the whole file is in.
    layers.count = 0;
    m_file_pos = 0;
    m_loading = true;
    m_full = false;
    m_flattening = false;
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
}

//! Plays the loop at the tempo of the score. The ratio is taken when the
//! loop starts a round, so that a tempo change is heard from the next round.
inline void Node::play_stretched(int64_t first_pos, int64_t samples)
{
  const int64_t len = layers.length;
  const int chans = layers.channels;
  if(std::ssize(m_stretch_out) < chans || samples > std::ssize(m_stretch_out[0]))
  {
    play(first_pos, samples);
    return;
  }
  if(!m_stretch_ready || transport.playbackPos % len != m_expected_pos)
  {
    m_stretcher.transport(transport.playbackPos % len);
    m_cycle_ratio = m_loop_tempo / std::max(1., m_tempo);
    m_stretch_ready = true;
  }

  ossia::mutable_audio_span<double> ap;
  for(int c = 0; c < chans; c++)
    ap.emplace_back(m_stretch_out[c].data(), std::size_t(samples));

  ossia::token_request tk;
  tk.prev_date = ossia::time_value{0};
  tk.date = ossia::time_value{samples};
  m_wrapped = false;
  m_stretcher.run(
      *this, tk, ossia_state, m_cycle_ratio, 1., std::size_t(chans), len, samples,
      samples, 0, ap);
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

  transport.playbackPos = m_stretcher.next_sample_to_read() % len;
  m_expected_pos = transport.playbackPos;
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
    layers.begin_recording();
    transport.playbackPos = 0;
    m_flattening = false;
    m_export_pending = false;
    m_export_cancelled = m_writing;
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
    transport.end_recording(inputs.postaction);
    transport.playbackPos = 0;
  }
  return n;
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

  int64_t len = layers.length;
  if(!transport.is_taking())
  {
    layers.begin_overdub(transport.playbackPos % len);
    // An extending pass grows the loop by the loop it started over.
    m_extend_unit = len;
    m_extended = false;
  }
  transport.taking(LoopMode::Overdub);
  const bool extend = inputs.past_end.value == Looper::PastEnd::Extend;

  auto* top = layers.top();
  if(!top)
    return;

  const int in_chans = std::min(p1.channels, layers.channels);
  const int out_chans = p2.channels;
  const float feedback = std::clamp(inputs.feedback.value, 0.f, 1.f);
  const int fade = Looper::audio_layers::fade_frames;

  int64_t k = transport.playbackPos % len;
  for(int64_t j = first_pos; j < last; j++)
  {
    const bool first_time = top->written < len;
    const float ramp = top->written < fade ? float(top->written) / fade : 1.f;
    for(int c = 0; c < layers.channels; c++)
    {
      const float in = c < in_chans ? float(p1.samples[c][j]) : 0.f;
      const float before = layers.mix(c, k);
      float& d = layers.data(top->slot, c)[k];
      // Later laps of the pass fade what it laid down on the earlier ones.
      d = (first_time ? 0.f : d * feedback) + in * ramp;
      layers.note_peak(top->slot, k, d);
      if(c < out_chans)
        p2.samples[c][j] = echo ? before + in : before;
    }
    for(int c = layers.channels; c < out_chans; c++)
      p2.samples[c][j] = 0.;

    top->written++;
    if(++k == len)
    {
      if(extend && layers.extend(m_extend_unit))
      {
        len = layers.length;
        m_extended = true;
      }
      else
      {
        k = 0;
        layers.decay(feedback);
      }
    }
  }
  transport.playbackPos = k;
}

inline void Node::start_export_item(int item)
{
  m_export_item = item;
  m_flattening = true;
  m_flatten_pos = 0;
}

inline void Node::run_export()
{
  if(m_export_pending && !m_flattening && !m_writing && !layers.empty() && writer
     && export_audio->capacity >= layers.length
     && export_audio->channels >= layers.channels)
  {
    m_export_pending = false;
    // A take closing while this one is written must not change its date.
    m_job_date = m_export_date;
    m_export_length = layers.length;
    m_export_channels = layers.channels;
    const auto files = inputs.takes.value;
    m_export_mix = files != Looper::TakeFiles::Layers;
    m_export_layers = files != Looper::TakeFiles::Mix ? layers.count : 0;
    // The passes as they are now: gains change with feedback while the files
    // are written.
    std::copy_n(layers.layers.begin(), layers.count, m_export_snapshot.begin());
    m_export_number = 0;
    start_export_item(m_export_mix ? -1 : 0);
  }

  if(!m_flattening)
    return;

  // About a second of audio per tick at most: a minute-long loop does not
  // make one tick a minute of work.
  constexpr int64_t chunk = 65536;
  const int64_t end = std::min(m_export_length, m_flatten_pos + chunk);
  for(int c = 0; c < m_export_channels; c++)
  {
    float* dst = export_audio->channel(c);
    if(m_export_item < 0)
    {
      for(int64_t k = m_flatten_pos; k < end; k++)
        dst[k] = layers.mix(c, k);
    }
    else
    {
      const auto& l = m_export_snapshot[m_export_item];
      const float* src = layers.data(l.slot, c);
      for(int64_t k = m_flatten_pos; k < end; k++)
        dst[k] = layers.valid(l, k) ? l.gain * src[layers.index(l, k)] : 0.f;
    }
  }
  m_flatten_pos = end;

  if(m_flatten_pos < m_export_length)
    return;

  m_flattening = false;
  m_writing = true;

  Looper::take_job job;
  job.writer = writer;
  job.audio = export_audio;
  job.slot = m_take_slot;
  job.number = m_export_number;
  job.layer = m_export_item;
  job.channels = m_export_channels;
  job.frames = m_export_length;
  job.rate = int(m_rate);
  job.tempo = m_export_tempo;
  job.meter_numerator = m_export_signature.upper;
  job.meter_denominator = m_export_signature.lower;
  worker.request(std::move(job));
}

inline void Node::take_written(int slot, int number, bool ok)
{
  m_writing = false;
  if(m_export_cancelled || !ok || !writer)
  {
    // A new recording replaced what was being written, or the file could not
    // be: the take stops there.
    m_export_cancelled = false;
    return;
  }

  m_export_number = number;
  if(m_export_item + 1 < m_export_layers)
  {
    start_export_item(m_export_item + 1);
    return;
  }

  m_takes_written++;
  m_last_take_date = m_job_date;
  m_view.set_take(writer->take_base(slot), "wav", m_export_mix, m_export_layers);
  m_view.take = m_takes_written;
  m_view.take_date = m_last_take_date;
  m_take_slot = (slot + 1) % Looper::take_writer::slots;
  // Force a view update with the new take.
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
  v.empty = layers.empty();
  v.mode = int(transport.actualMode);
  v.requested = int(transport.quantizedPlayMode);
  v.wanted = int(m_mode);
  v.layers = layers.count;
  v.redo = layers.total - layers.count;
  v.full = m_full;
  v.length_seconds = float(layers.length / m_rate);
  v.max_seconds = float(layers.capacity / m_rate);
  v.position = layers.length > 0
                   ? float(transport.playbackPos % layers.length) / float(layers.length)
                   : 0.f;
  v.tempo = float(m_tempo);
  v.writing = m_flattening || m_writing;

  // Lanes: one per pass, the oldest merged into the first when there are
  // more passes than lanes.
  const int lanes = std::min(layers.count, Looper::view_message::max_lanes);
  v.lanes = lanes;
  std::fill(v.peaks.begin(), v.peaks.end(), 0.f);
  const int64_t len = layers.length;
  if(len > 0)
  {
    const int64_t micro = (len + Looper::audio_layers::peak_frames - 1)
                          / Looper::audio_layers::peak_frames;
    for(int i = 0; i < layers.count; i++)
    {
      const int lane = std::max(0, i - (layers.count - lanes));
      const auto& l = layers.layers[i];
      const float* p = const_cast<Looper::audio_layers&>(layers).peaks_of(l.slot);
      float* dst = v.peaks.data() + lane * Looper::view_message::bins;
      // A pass shorter than the loop plays again over the rest of it.
      const int64_t own = std::max<int64_t>(
          1, (layers.period_of(l) + Looper::audio_layers::peak_frames - 1)
                 / Looper::audio_layers::peak_frames);
      for(int64_t m = 0; m < micro; m++)
      {
        const int b = int(m * Looper::view_message::bins / micro);
        dst[b] = std::max(dst[b], p[m % own] * l.gain);
      }
    }
  }

  send_message(v);
}
}
