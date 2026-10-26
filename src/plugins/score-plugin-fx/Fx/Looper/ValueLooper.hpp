#pragma once
#include <Fx/Looper/LooperNode.hpp>
#include <Fx/Looper/ValueLoop.hpp>
#include <Fx/Looper/ValueTake.hpp>
#include <Fx/Types.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/dataflow/value_port.hpp>

#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>

#include <functional>
#include <limits>
#include <memory>

namespace Nodes::ValueLooper
{
struct Node : Looper::looper_node<Node>
{
  halp_meta(name, "Looper (values)")
  halp_meta(c_name, "value_looper")
  halp_meta(category, "Midi/Sequencers")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/value-looper.html")
  halp_meta(
      description,
      "Loop a stream of values in passes: numbers and vectors held as a "
      "curve, impulses fired where they came, and turn the loop into "
      "automations of the timeline")
  halp_meta(uuid, "399796c5-9e86-4a00-8671-3e759ad6fb33");
  halp_meta(recommended_height, 200);
  halp_flag(process_exec);

  using LoopMode = Looper::LoopMode;
  using Passthrough = Looper::Passthrough;

  //! Keys of the values looped: one stream for now.
  static constexpr int max_keys = 1;
  //! A value the input sends wins over the loop's for this long, when it is
  //! heard: while it moves, the input is what goes out.
  static constexpr double input_hold_seconds = 0.2;

  struct ins
  {
    ossia_port<"in", ossia::value_port> values;

    struct : halp::enum_t<LoopMode, "Mode">
    {
      using halp::enum_t<LoopMode, "Mode">::operator=;
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
          "Whether the values that come in go out as well as the loop: never, "
          "only while recording or overdubbing, or always. Always, a value "
          "that comes in wins over the loop while the input moves; once it "
          "stops, the loop takes over again from the value it holds there.")
    } passthrough;

    Looper::after_record_selector after_record;

    struct : halp::spinbox_i32<"Record bars", halp::irange{0, 64, 4}>
    {
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

    struct : halp::spinbox_i32<"Late stop (ms)", halp::irange{0, 500, 120}>
    {
      halp_meta(
          description,
          "A recording asked to stop this long after a point of the grid ends "
          "on that point.")
    } late;

    struct : halp::spinbox_i32<"Capture bars", halp::irange{1, 64, 4}>
    {
      halp_meta(description, "How many bars Capture takes")
    } capture_bars;

    struct : halp::impulse_button<"Capture">
    {
      halp_meta(
          description,
          "Makes a loop of the last bars of values that came in, recorded or "
          "not, starting from the value held before them, and plays it at "
          "once.")
    } capture;

    struct : Looper::enum_combobox<"Export", Looper::TakeFiles, Looper::TakeFiles::Mix>
    {
      using enum_combobox::operator=;
      halp_meta(
          description,
          "What a take writes: what is heard of the loop (Mix), each pass in a "
          "file of its own (Layers), or both. Sending or dragging a take of "
          "numbers or vectors places an automation for each number, a box "
          "for each file; a take with impulses, text, lists or maps is kept "
          "as a file only.")
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
          "Plays the loop backwards, from where the playhead is: a value goes "
          "back to the one it had before, impulses fire where they were. An "
          "overdub records where the playhead goes, as on tape: what is played "
          "over the reversed loop plays reversed once it goes forwards again.")
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
          "layer (Whole), each round writing over the one before, or a layer "
          "for each time round (Per cycle), cut on the loop's start, to keep "
          "the best one with Solo layer or Mute layer, erase the others, or "
          "splice them in the timeline from a take written as layers. With "
          "Past the end on Extend, a cut is made each time the loop grows.")
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

    struct
        : halp::spinbox_i32<"Max layers", halp::irange{2, Looper::value_loop::max_layers, 8}>
    {
      halp_meta(
          description,
          "Layers kept apart, for undo, mute, solo and erase. What a pass does "
          "past it is what At max layers says.")
    } max_layers;
  } inputs;

  struct
  {
    ossia_port<"out", ossia::value_port> values;
    struct : halp::val_port<"Phase", float>
    {
      halp_meta(description, "Position in the loop, from 0 to 1")
    } phase;
  } outputs;

  struct worker_t
  {
    std::function<void(Looper::value_take_job)> request;
    static std::function<void(Node&)> work(Looper::value_take_job&& job)
    {
      const bool ok = job.run();
      return [slot = job.slot, number = job.number, alone = job.alone,
              placeable = job.components > 0, ok](Node& self) {
        self.m_written_placeable = placeable;
        self.take_written(slot, number, ok, alone);
      };
    }
  } worker;

  std::function<void(Looper::view_message)> send_message;

  //! From the view: applied at the start of the next tick.
  void process_message(const Looper::erase_request& r) { view_request(r); }

  const score::DocumentContext* ossia_document_context{};
  ossia::exec_state_facade ossia_state;

  Looper::value_loop loop;
  std::shared_ptr<Looper::take_writer> writer;
  std::shared_ptr<Looper::value_export> export_data;
  //! What the take written when the score stops is made of: the node may be
  //! gone before it is written.
  std::shared_ptr<Looper::value_export> stop_data;

  Node()
  {
    export_data = std::make_shared<Looper::value_export>();
    stop_data = std::make_shared<Looper::value_export>();
  }

  void prepare(halp::setup)
  {
    if(ossia_document_context)
      writer = Looper::take_writer::make(*ossia_document_context, "Value looper", "values");
  }

  //! The score stops: the take of a pass being taken in is written on the
  //! next tick, which the score stopping still runs.
  void stop() { m_stop_requested = true; }

  int64_t looper_run(int64_t start, int64_t length);
  void looper_trim_recording(int64_t frames);
  //! Longest loop an extending overdub makes, as the audio looper's longest.
  static constexpr double max_loop_seconds = 3600.;

  void operator()(const tick& tk)
  {
    m_out_count = 0;
    const auto timings = ossia_state.timings(tk);
    m_tick_frames = timings.length;
    m_tick_start = timings.start_sample;
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
          halp::item<&ins::passthrough> passthrough;
        } play;
        Looper::switches_tab<ins> switches;
        struct
        {
          halp_meta(name, "Overdub")
          halp_meta(layout, halp::layouts::grid)
          halp_meta(alignment, halp::alignments::left)
          halp_meta(spacing, 6)
          halp_meta(columns, 2)
          halp::item<&ins::past_end> past_end;
          halp::item<&ins::passes> passes;
          halp::item<&ins::max_layers> max_layers;
          halp::item<&ins::at_max_layers> at_max_layers;
        } overdub;
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
  Looper::value_loop& stack() noexcept { return loop; }
  const Looper::value_loop& stack() const noexcept { return loop; }
  //! What is heard changes: the playhead sends the value held then, at the
  //! next span it plays.
  void layers_changing() noexcept { }
  bool busy() const noexcept { return false; }
  void cleared() noexcept { }
  void pass_dropped() noexcept
  {
    loop.taking = false;
    loop.touch_count = 0;
  }
  void begin_overdub(int64_t start, bool oldest, int replaced)
  {
    loop.begin_overdub(start, max_layers(), oldest, replaced);
    forget_recorded();
  }
  bool can_merge_newest() const noexcept { return !loop.taking; }
  void merge_newest() noexcept { loop.merge(loop.count - 2, true); }
  bool pass_closing() noexcept { return true; }
  //! It holds each key it moved from its first new value of it to here.
  void pass_closed() { loop.end_pass(); }
  void tick_started(const tick&);
  void prepare_loop() noexcept { }
  void forget_carried() noexcept
  {
    for(int i = 0; i < m_carried_count; i++)
      m_carried[i].value.reset();
    m_carried_count = 0;
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
    loop.end_pass();
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
  //! What goes out is sent again once the score starts again.
  void stopping() noexcept
  {
    for(auto& k : m_keys)
    {
      k.out.reset();
      k.owned_until = std::numeric_limits<int64_t>::min();
    }
  }
  void close_on_stop(bool recording)
  {
    if(recording)
      loop.end_pass();
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
  //! An automation can play the take written last.
  bool take_placeable() const noexcept { return m_written_placeable; }
  static constexpr const char* take_extension = "values";
  //! An automation is placed in the score's time.
  static constexpr halp::clip_stretch take_stretch() noexcept
  {
    return halp::clip_stretch::none;
  }
  bool view_full() const noexcept { return loop.full; }
  bool view_writing() const noexcept { return m_writing; }

  int max_layers() const noexcept
  {
    return std::clamp(inputs.max_layers.value, 2, Looper::value_loop::max_layers);
  }

  //! What the looper knows of a key: the last value that came in, the last
  //! that went out, until when the input wins over the loop, and the last
  //! value the pass being taken in recorded.
  struct key_state
  {
    Looper::value_slot in;
    Looper::value_slot out;
    int64_t owned_until{std::numeric_limits<int64_t>::min()};
    Looper::value_slot recorded;
  };
  std::array<key_state, max_keys> m_keys{};
  void forget_recorded() noexcept
  {
    for(auto& k : m_keys)
      k.recorded.reset();
  }

  // What goes out this tick.
  static constexpr int max_out = 1024;
  struct out_value
  {
    int64_t ts;
    Looper::value_slot value;
    int order;
  };
  std::array<out_value, max_out> m_out{};
  int m_out_count{};
  void emit(int64_t ts, const ossia::value& v)
  {
    if(m_out_count < max_out)
    {
      auto& o = m_out[m_out_count++];
      o.ts = ts;
      o.value.set(v);
    }
  }
  //! The loop sends `v`, held from frame `ts` of the tick: unless it is what
  //! went out already, or the input has the key.
  void send_held(uint16_t key, const Looper::value_slot& v, int64_t ts)
  {
    auto& k = m_keys[key];
    if(v.empty() || m_now + ts < k.owned_until)
      return;
    if(!k.out.empty() && k.out.same(v))
      return;
    k.out = v;
    emit(ts, v.get());
  }
  void push_outputs();

  // The input of the tick, by time.
  static constexpr int max_in = 1024;
  std::array<int, max_in> m_in_order{};
  int m_in_count{};
  int m_in_index{};
  int64_t input_ts(int i) const noexcept;

  // What the loop sends over the span being played, by time: a held value
  // changing, from point `ref` of the timeline, or an impulse, event `ref`.
  struct due
  {
    int64_t ts;
    int ref;
    bool impulse;
    int order;
  };
  static constexpr int max_due = 2048;
  std::array<due, max_due> m_due{};
  int m_due_count{};
  int m_due_index{};
  void add_due(int64_t ts, int ref, bool impulse) noexcept
  {
    if(m_due_count < max_due)
    {
      m_due[m_due_count] = {ts, ref, impulse, m_due_count};
      m_due_count++;
    }
  }
  void send_due(int64_t until);

  Looper::held_timeline m_timeline;
  Looper::held_scratch m_scratch;
  void update_timeline() { m_timeline.update(loop, 0, m_scratch); }
  //! The loop takes up the value it holds where the playhead is.
  void chase(int64_t ts);

  // What a recording cut back to a grid point had recorded past it, from the
  // grid point on, and how many frames were cut.
  struct carried_value
  {
    int64_t pos;
    Looper::value_slot value;
    uint16_t key;
  };
  std::array<carried_value, 1024> m_carried{};
  int m_carried_count{};
  int64_t m_carried_span{};

  // What came in lately, recorded or not, for Capture, and for each key the
  // value it held before the oldest of it.
  struct played
  {
    int64_t frame;
    Looper::value_slot value;
    uint16_t key;
  };
  static constexpr int history_size = 4096;
  std::array<played, history_size> m_history{};
  int64_t m_history_count{};
  std::array<Looper::value_slot, max_keys> m_history_floor{};

  int64_t m_now{};
  int64_t m_tick_frames{};
  int64_t m_tick_start{};
  int64_t m_expected_pos{};
  bool m_stop_requested{};
  bool m_written_placeable{true};

  void handle_input(
      int64_t from, int64_t to, double pos0, bool taking, double rate = 1.,
      bool backwards = false);
  void record_value(
      int64_t pos, int64_t written, uint16_t key, const ossia::value& v, bool backwards);
  void close_backwards(int64_t boundary);
  void next_cycle_pass(int64_t start);
  void begin_pass_carried(int64_t k);
  void begin_recording();

  //! The playhead is m_phase, on the loop's own positions.
  bool m_was_reverse{};
  double m_phase{};
  void advance(int64_t start, int64_t length, bool overdub);
  double rate() const noexcept;
  void play_range(double a, double b, double out0, double r);
  void play_range_reverse(double a, double b, double out0, double r);

  void write_export_item(int item);
  void fill_export(
      const Looper::value_loop& snap, int item, Looper::value_export& e);
  void write_stop_take(bool recording, double date, int64_t pos);
  //! The loop as it was when the take closed, which every file of the take
  //! is made from: the loop changes while they are written.
  Looper::value_loop m_export_loop;
  Looper::held_timeline m_export_timeline;
  void send_view(int64_t frames);
};

inline int64_t Node::input_ts(int i) const noexcept
{
  const auto& data = inputs.values.value->get_data();
  return data[std::size_t(m_in_order[i])].timestamp - m_tick_start;
}

//! The input of the tick, by time: values sent together keep their order.
inline void Node::tick_started(const tick&)
{
  m_in_count = 0;
  m_in_index = 0;
  if(!inputs.values.value)
    return;
  const auto& data = inputs.values.value->get_data();
  for(std::size_t i = 0; i < data.size() && m_in_count < max_in; i++)
    if(data[i].value.valid())
      m_in_order[m_in_count++] = int(i);
  std::sort(m_in_order.begin(), m_in_order.begin() + m_in_count, [&](int a, int b) {
    const auto ta = data[std::size_t(a)].timestamp, tb = data[std::size_t(b)].timestamp;
    return ta != tb ? ta < tb : a < b;
  });
}

inline void Node::push_outputs()
{
  for(int i = 0; i < m_out_count; i++)
    m_out[i].order = i;
  std::sort(m_out.begin(), m_out.begin() + m_out_count, [](const out_value& a, const out_value& b) {
    return a.ts != b.ts ? a.ts < b.ts : a.order < b.order;
  });
  auto* port = outputs.values.value;
  for(int i = 0; i < m_out_count; i++)
  {
    if(port)
      port->write_value(
          m_out[i].value.get(),
          m_tick_start + std::clamp<int64_t>(m_out[i].ts, 0, std::max<int64_t>(0, m_tick_frames - 1)));
    m_out[i].value.reset();
  }
  m_out_count = 0;
}

//! What the loop sends over the span up to frame `until`, in time.
inline void Node::send_due(int64_t until)
{
  for(; m_due_index < m_due_count && m_due[m_due_index].ts <= until; m_due_index++)
  {
    const auto& d = m_due[m_due_index];
    if(d.impulse)
    {
      if(d.ref < loop.event_count)
        emit(d.ts, loop.events[d.ref].value.get());
      continue;
    }
    const auto& p = m_timeline.points[d.ref];
    if(p.event >= 0 && p.event < loop.event_count)
      send_held(loop.events[p.event].key, loop.events[p.event].value, d.ts);
  }
}

inline void Node::chase(int64_t ts)
{
  if(loop.empty())
    return;
  update_timeline();
  const int64_t x = std::clamp<int64_t>(int64_t(m_phase), 0, loop.length - 1);
  const int i = m_reverse ? m_timeline.before(x) : m_timeline.at(x);
  if(i < 0)
    return;
  const int e = m_timeline.points[i].event;
  if(e >= 0 && e < loop.event_count)
    send_held(loop.events[e].key, loop.events[e].value, ts);
}

//! The input values of [from; to[ of the tick: recorded at pos0 and on when
//! taking in, sent on when the passthrough says so, after what the loop
//! sends before them.
inline void Node::handle_input(
    int64_t from, int64_t to, double pos0, bool taking, double rate, bool backwards)
{
  const Passthrough pt = inputs.passthrough;
  const bool echo = taking ? pt != Passthrough::None : pt == Passthrough::Full;
  const auto hold = int64_t(input_hold_seconds * m_rate);

  for(; m_in_index < m_in_count; m_in_index++)
  {
    const int64_t ts = input_ts(m_in_index);
    if(ts >= to)
      break;
    send_due(ts);
    const auto& v = inputs.values.value->get_data()[std::size_t(m_in_order[m_in_index])].value;
    const uint16_t key = 0;
    auto& k = m_keys[key];
    const bool impulse = v.get_type() == ossia::val_type::IMPULSE;

    {
      auto& h = m_history[m_history_count++ % history_size];
      if(m_history_count > history_size && !h.value.impulse())
        m_history_floor[h.key] = h.value;
      h.frame = m_now + ts;
      h.key = key;
      h.value.set(v);
    }

    // Where the value falls in the loop. A recording's loop is as long as
    // what was recorded so far, so its positions are not wrapped.
    // Backwards, pos0 counts from the loop's end, and a value falls on the
    // edge of the position the playhead has just gone over.
    int64_t pos = -1;
    int64_t written = 0;
    if(pos0 >= 0)
    {
      const double into = (std::max(ts, from) - from) * rate;
      pos = int64_t(pos0 + into);
      written = int64_t(loop.pass_progress + into);
      if(backwards && loop.length > 0)
      {
        pos = (loop.length - pos) % loop.length;
        if(pos < 0)
          pos += loop.length;
      }
      else if(transport.actualMode != LoopMode::Record && loop.length > 0)
        pos %= loop.length;
      else
        written = pos;
    }

    // A value is new unless it is the one the key held: only a new value
    // is recorded.
    const bool fresh = impulse || k.in.empty() || !k.in.same(v);
    if(!impulse)
      k.in.set(v);
    if(taking && pos >= 0 && fresh)
      record_value(pos, written, key, v, backwards);

    if(echo)
    {
      emit(ts, v);
      if(!impulse)
      {
        k.out.set(v);
        k.owned_until = m_now + ts + hold;
      }
    }
  }
  send_due(to - 1);
}

inline void Node::record_value(
    int64_t pos, int64_t written, uint16_t key, const ossia::value& v, bool backwards)
{
  if(v.get_type() == ossia::val_type::IMPULSE)
  {
    loop.add_value(pos, written, key, v);
    return;
  }
  auto& k = m_keys[key];
  if(!loop.touched(key))
    loop.touch_key(key, pos, written, backwards);
  // Backwards, a value holds for the positions under the one it came at:
  // the loop keeps, at that position, the value from before, which is what
  // it goes back to going forwards; and where the playhead stops going
  // backwards, the last value that came.
  if(!backwards)
    loop.add_value(pos, written, key, v);
  else if(!k.recorded.empty())
    loop.add_value(pos, written, key, k.recorded.get());
  k.recorded.set(v);
}

//! The playhead stops going backwards at `boundary` while taking in: the
//! values last recorded hold from there.
inline void Node::close_backwards(int64_t boundary)
{
  if(loop.length <= 0)
    return;
  boundary %= loop.length;
  for(int key = 0; key < max_keys; key++)
    if(!m_keys[key].recorded.empty())
      loop.add_value(
          boundary, int64_t(loop.pass_progress), uint16_t(key), m_keys[key].recorded.get());
}

//! A recording starts with the value each key holds: one that does not move
//! during its first beat is in the loop all the same.
inline void Node::begin_recording()
{
  loop.begin_recording();
  forget_recorded();
  for(int key = 0; key < max_keys; key++)
  {
    const auto& in = m_keys[key].in;
    if(in.empty())
      continue;
    loop.touch_key(uint16_t(key), 0, 0, false);
    loop.add_value(0, 0, uint16_t(key), in.get());
  }
}

inline void Node::looper_trim_recording(int64_t frames)
{
  const int64_t end = std::max<int64_t>(0, loop.length - frames);
  // What came after the grid point belongs to the next pass: it is kept
  // aside, from the grid point on, for an overdub that starts there.
  const int64_t cut = loop.length - end;
  for(int i = 0; i < m_carried_count; i++)
    m_carried[i].pos += cut;
  m_carried_span += cut;
  for(int i = 0; i < loop.event_count; i++)
  {
    const auto& e = loop.events[i];
    if(e.pos < end || e.impulse)
      continue;
    if(m_carried_count < int(m_carried.size()))
    {
      auto& c = m_carried[m_carried_count++];
      c.pos = e.pos - end;
      c.value = e.value;
      c.key = e.key;
    }
  }
  loop.remove_events([end](const Looper::value_event& e) { return e.pos >= end; });
  loop.length = end;
  loop.pass_written = end;
  loop.edits++;
}

//! An overdub starts at position k. A recording that just closed on a grid
//! point it was cut back to hands it what came in since that point: when
//! the playhead went on from the grid point, the pass starts there and the
//! values are where they came; else they come at the start of the pass.
inline void Node::begin_pass_carried(int64_t k)
{
  const int64_t len = loop.length;
  const bool from_point = m_carried_span > 0 && m_carried_span < len && k == m_carried_span;
  begin_pass(from_point ? 0 : k);
  for(int i = 0; i < m_carried_count; i++)
  {
    const auto& c = m_carried[i];
    const int64_t pos = from_point ? c.pos % len : k;
    record_value(pos, from_point ? c.pos : 0, c.key, c.value.get(), false);
  }
  if(from_point)
    loop.advance_pass(double(m_carried_span));
  forget_carried();
}

inline int64_t Node::looper_run(int64_t start, int64_t length)
{
  if(!loop.empty() && transport.playbackPos % loop.length != m_expected_pos)
    m_phase = double(transport.playbackPos % loop.length);
  if(m_reverse != m_was_reverse)
  {
    if(m_was_reverse && loop.taking && !loop.empty())
      close_backwards(int64_t(std::ceil(m_phase - 1e-9)));
    m_was_reverse = m_reverse;
  }

  const LoopMode mode = transport.actualMode;
  switch(mode)
  {
    case LoopMode::Stop:
      handle_input(start, start + length, -1, false);
      break;

    case LoopMode::Play:
      advance(start, length, false);
      break;

    case LoopMode::Record: {
      if(!transport.is_taking())
      {
        begin_recording();
        m_switches.soloed.forget();
        transport.playbackPos = 0;
        m_export_pending = false;
      }
      transport.taking(LoopMode::Record);
      const int64_t pos0 = loop.length;
      handle_input(start, start + length, double(pos0), true);
      loop.length += length;
      loop.pass_written = loop.length;
      loop.pass_progress = double(loop.length);
      loop.update_open_stretches();
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
//! comes in when overdubbing. The loop's positions go `rate()` times as fast
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
  m_phase = ph;
  double y = back ? double(loop.length) - ph : ph;
  if(y >= double(loop.length))
    y -= double(loop.length);
  double out = double(start);
  const double out_end = double(start + length);

  chase(start);
  while(out < out_end - 1e-9)
  {
    const double L = double(loop.length);
    const double b = std::min(L, y + (out_end - out) * r);
    const double seg_out = (b - y) / r;
    if(overdub && std::exchange(m_cycle_pending, false))
      next_cycle_pass(back ? 0 : int64_t(y));

    // Gone round again, the pass writes over what it recorded a round
    // before.
    if(overdub && !back)
      loop.write_over(int64_t(std::ceil(y)), int64_t(std::ceil(b)), loop.pass_progress);
    if(back)
      play_range_reverse(y, b, out, r);
    else
      play_range(y, b, out, r);
    // The input of the frames this part of the loop covers.
    const int64_t from = int64_t(std::ceil(out - 1e-9));
    const int64_t to = b >= L ? int64_t(std::ceil(out + seg_out - 1e-9))
                              : int64_t(std::ceil(out_end - 1e-9));
    handle_input(from, to, y + (from - out) * r, overdub, r, back);
    send_due(std::numeric_limits<int64_t>::max());
    if(overdub)
    {
      loop.advance_pass(b - y, back);
      loop.update_open_stretches();
    }

    out += seg_out;
    y = b;
    if(y >= L - 1e-9)
    {
      if(extend && loop.extend(m_extend_unit, int64_t(max_loop_seconds * m_rate)))
      {
        // A cycle is the loop's length when the overdub started: the loop
        // growing by one is a cycle gone round as well.
        m_extended = true;
        loop.edits++;
        if(per_cycle)
          m_cycle_pending = true;
      }
      else
      {
        y -= L;
        if(y < 0.)
          y = 0.;
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

//! What the loop sends between positions a and b, at the frames of the tick
//! they fall on: out0 is the frame of position a. A held value changes where
//! the timeline says; an impulse fires where it is, as often as its pass
//! repeats.
inline void Node::play_range(double a, double b, double out0, double r)
{
  m_due_count = 0;
  m_due_index = 0;
  update_timeline();
  const int64_t len = loop.length;
  auto frame = [&](int64_t q) { return int64_t(out0 + (q - a) / r); };
  const int64_t lo = int64_t(std::ceil(a));
  const int64_t hi = std::min(len, int64_t(std::ceil(b)));
  if(lo >= hi)
    return;

  for(int i = m_timeline.at(lo); i >= 0 && i < m_timeline.count; i++)
  {
    const auto& p = m_timeline.points[i];
    if(p.pos < lo)
      continue;
    if(p.pos >= hi)
      break;
    add_due(frame(p.pos), i, false);
  }
  for(int i = 0; i < loop.event_count; i++)
  {
    const auto& e = loop.events[i];
    if(!e.impulse || !loop.heard(e))
      continue;
    const int64_t period = loop.period_for(e.pass);
    const int64_t first = e.pos % period;
    for(int64_t q = first + std::max<int64_t>(0, (lo - first + period - 1) / period) * period;
        q < hi; q += period)
      add_due(frame(q), i, true);
  }
  std::sort(m_due.begin(), m_due.begin() + m_due_count, [](const due& x, const due& y) {
    return x.ts != y.ts ? x.ts < y.ts : x.order < y.order;
  });
}

//! The same, between positions a and b of the loop read from its end: a
//! change of the held value at p is crossed at length - p, and goes back to
//! the value before it; an impulse at p fires at length - p, a pass that
//! repeats a whole number of times in the loop mirrored in each repeat.
inline void Node::play_range_reverse(double a, double b, double out0, double r)
{
  m_due_count = 0;
  m_due_index = 0;
  update_timeline();
  const int64_t len = loop.length;
  auto frame = [&](int64_t q) { return int64_t(out0 + (q - a) / r); };
  const int64_t lo = int64_t(std::ceil(a));
  const int64_t hi = std::min(len, int64_t(std::ceil(b)));
  if(lo >= hi)
    return;
  auto mirrored = [](int64_t pos, int64_t period) {
    int64_t m = (-pos) % period;
    if(m < 0)
      m += period;
    return m;
  };

  for(int i = 0; i < m_timeline.count; i++)
  {
    const int64_t q = mirrored(m_timeline.points[i].pos, len);
    if(q >= lo && q < hi)
      add_due(frame(q), m_timeline.previous(i), false);
  }
  for(int i = 0; i < loop.event_count; i++)
  {
    const auto& e = loop.events[i];
    if(!e.impulse || !loop.heard(e))
      continue;
    int64_t period = loop.period_for(e.pass);
    if(len % period != 0)
      period = len;
    const int64_t first = mirrored(e.pos % period, period);
    for(int64_t q = first + std::max<int64_t>(0, (lo - first + period - 1) / period) * period;
        q < hi; q += period)
      add_due(frame(q), i, true);
  }
  std::sort(m_due.begin(), m_due.begin() + m_due_count, [](const due& x, const due& y) {
    return x.ts != y.ts ? x.ts < y.ts : x.order < y.order;
  });
}

//! Capture: what came in over the `n` frames up to `end` becomes the loop,
//! from the value each key held before them.
inline void Node::capture_into(int64_t end, int64_t n)
{
  const int64_t from = end - n;
  loop.begin_recording();
  loop.length = n;
  forget_recorded();

  const int64_t count = std::min<int64_t>(m_history_count, history_size);
  // The value held before the window.
  for(int key = 0; key < max_keys; key++)
  {
    const Looper::value_slot* before = m_history_floor[key].empty() ? nullptr : &m_history_floor[key];
    for(int64_t i = m_history_count - count; i < m_history_count; i++)
    {
      const auto& h = m_history[i % history_size];
      if(h.key == key && h.frame < from && !h.value.impulse())
        before = &h.value;
    }
    if(before)
      record_value(0, 0, uint16_t(key), before->get(), false);
  }
  for(int64_t i = m_history_count - count; i < m_history_count; i++)
  {
    const auto& h = m_history[i % history_size];
    if(h.frame < from || h.frame >= end)
      continue;
    const int64_t pos = h.frame - from;
    const auto& last = m_keys[h.key].recorded;
    if(!h.value.impulse() && !last.empty() && last.same(h.value))
      continue;
    record_value(pos, pos, h.key, h.value.get(), false);
  }
  loop.pass_written = n;
  loop.pass_progress = double(n);
  loop.end_pass();
}

//! An extending pass cut back to `end`: what it recorded since is dropped.
inline void Node::trim_top_pass(int64_t end)
{
  const int pass = loop.count - 1;
  loop.remove_events(
      [=](const Looper::value_event& e) { return e.pass == pass && e.pos >= end; });
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
    m_export_loop.edits++;
  }
  write_export_item(m_export_mix ? -1 : 0);
}

//! Writes one file of the take: what is heard of the loop (item -1), or what
//! is heard of one of its passes.
inline void Node::write_export_item(int item)
{
  m_export_item = item;
  m_writing = true;
  fill_export(m_export_loop, item, *export_data);

  Looper::value_take_job job;
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
inline void Node::write_stop_take(bool recording, double date, int64_t)
{
  if(!writer || loop.empty() || stop_data->in_use)
    return;
  m_export_tempo = recording ? m_tempo : m_loop_tempo;
  m_export_signature = m_signature;
  fill_export(loop, -1, *stop_data);
  m_stop_date = date;

  Looper::value_take_job job;
  job.writer = writer;
  job.data = stop_data;
  job.slot = claim_take_slot();
  job.number = 0;
  job.layer = -1;
  job.alone = true;
  stop_data->in_use = true;
  worker.request(std::move(job));
}

//! What is heard of `snap` (item -1), or of one of its passes, into `e`: the
//! held values where they change, and the impulses.
inline void Node::fill_export(
    const Looper::value_loop& snap, int item, Looper::value_export& e)
{
  e.clear();
  auto& tl = m_export_timeline;
  tl.update(snap, 0, m_scratch, item, item, item < 0, true);
  // The held values, then the impulses, by position.
  for(int i = 0; i < tl.count; i++)
    if(tl.points[i].event >= 0)
      e.add(tl.points[i].pos, snap.events[tl.points[i].event].value, false);
  for(int i = 0; i < snap.event_count; i++)
  {
    const auto& ev = snap.events[i];
    if(!ev.impulse || !snap.heard(ev, item < 0) || (item >= 0 && ev.pass != item))
      continue;
    const int64_t period = snap.period_for(ev.pass);
    for(int64_t q = ev.pos % period; q < snap.length; q += period)
      e.add(q, ev.value, true);
  }
  std::sort(e.items.begin(), e.items.begin() + e.count, [](const auto& a, const auto& b) {
    return a.pos != b.pos ? a.pos < b.pos : a.impulse < b.impulse;
  });
  e.length = snap.length;
  e.rate = m_rate;
  e.tempo = m_export_tempo;
  e.meter_numerator = m_export_signature.upper;
  e.meter_denominator = m_export_signature.lower;
}

inline void Node::send_view(int64_t frames)
{
  if(!view_due(frames))
    return;

  auto& v = m_view;
  fill_view(v);
  v.values = true;
  v.lanes = std::min(loop.count, Looper::view_message::max_lanes);
  v.max_seconds = 0.f;
  v.notes = 0;

  constexpr int bins = Looper::view_message::bins;
  const int lanes = v.lanes;
  const int merged = loop.count - lanes;
  const int64_t len = std::max<int64_t>(1, loop.length);
  v.peaks.fill(-1.f);
  v.value_flags.fill(0);
  if(loop.empty())
  {
    send_message(v);
    return;
  }
  update_timeline();

  // What each lane holds, sampled at the start of each bin: its passes
  // alone, muted or not, and whether that is what is heard there.
  m_scratch.prepare(loop, 0, false);
  for(int l = 0; l < lanes; l++)
  {
    const int first = l == 0 ? 0 : l + merged;
    const int last = l + merged;
    float* y = v.peaks.data() + l * bins;
    uint8_t* f = v.value_flags.data() + l * bins;
    double lo = 0., hi = 0.;
    bool any = false;
    for(int b = 0; b < bins; b++)
    {
      const int64_t x = std::min<int64_t>(len - 1, int64_t(double(b) * double(len) / bins));
      const int e = m_scratch.value_at(loop, 0, x, first, last);
      if(e < 0)
        continue;
      const auto& val = loop.events[e].value.get();
      const int at = m_timeline.at(x);
      if(at >= 0 && m_timeline.points[at].event == e)
        f[b] |= Looper::view_message::flag_heard;
      if(Looper::value_export::numbers_of(val) > 0)
      {
        const double n = Looper::value_export::number(val, 0);
        y[b] = float(n);
        f[b] |= Looper::view_message::flag_number;
        lo = any ? std::min(lo, n) : n;
        hi = any ? std::max(hi, n) : n;
        any = true;
      }
      else
      {
        f[b] |= Looper::view_message::flag_other;
      }
    }
    // Scaled to the values seen in the lane.
    for(int b = 0; b < bins; b++)
      if(f[b] & Looper::view_message::flag_number)
        y[b] = hi > lo ? float((y[b] - lo) / (hi - lo)) : 0.5f;
  }

  // The impulses, and the values that are not numbers, as marks where they
  // are.
  for(int i = 0; i < loop.event_count; i++)
  {
    const auto& e = loop.events[i];
    if(e.pass >= loop.count)
      continue;
    const bool number = !e.impulse && Looper::value_export::numbers_of(e.value.get()) > 0;
    if(number)
      continue;
    const int l = std::clamp(e.pass - merged, 0, lanes - 1);
    const int64_t period = loop.period_for(e.pass);
    for(int64_t q = e.pos % period; q < len; q += period)
    {
      const int b = std::clamp(int(q * bins / len), 0, bins - 1);
      auto& f = v.value_flags[l * bins + b];
      f |= e.impulse ? Looper::view_message::flag_impulse : Looper::view_message::flag_mark;
      if(e.impulse && loop.heard(e))
        f |= Looper::view_message::flag_heard_mark;
    }
  }
  send_message(v);
}
}
