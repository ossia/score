#pragma once
#include <Fx/Looper/LooperView.hpp>
#include <Fx/Looper/Switches.hpp>
#include <Fx/Looper/TakeWriter.hpp>
#include <Fx/Looper/Transport.hpp>

#include <ossia/dataflow/token_request.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace Nodes::Looper
{
/**
 * What the audio and the MIDI looper do the same way with the passes of
 * their loop and with their transport: the tick, the buttons, undo, redo,
 * erase, mute and the layers past Max layers, closing a take, Capture, the
 * score stopping, the takes written and what the view is sent of them. The
 * looper, `Node`, derives from looper_node<Node>, keeps its passes in a
 * pass_stack it returns from stack(), and says what is its own:
 *
 *   // The passes, and what is heard of them, are about to change: the MIDI
 *   // looper ends the notes it plays.
 *   void layers_changing();
 *   // A file is being loaded: the loop is not there yet.
 *   bool busy() const;
 *   int max_layers() const;
 *   void cleared();                       // Clear emptied the loop
 *   void pass_dropped();                  // the pass taken in is dropped
 *   void begin_overdub(int64_t start, bool oldest, int replaced);
 *   bool can_merge_newest() const;
 *   void merge_newest();                  // the newest two, for undo
 *   bool pass_closing();                  // the top pass is not closed yet
 *   void pass_closed();                   // the top pass is over
 *
 *   // The tick: before the buttons, after Capture, after the transport ran
 *   // it, and at its end.
 *   void tick_started(const tick&);
 *   void prepare_loop();
 *   void forget_carried();
 *   void tick_finished(const tick&);
 *
 *   // Closing a take. `bars`: the frames of the recording's bars when they
 *   // ended it, 0 else.
 *   void looper_trim_recording(int64_t frames);
 *   void close_recording(int64_t bars);
 *   void close_overdub();
 *
 *   // An extending pass cut back to the loop's end: whether the top pass
 *   // starts at `end` or after it, the pass before it having been closed
 *   // there; what is done once such a pass is dropped; the top pass ending at
 *   // `end`; the playhead put `pos` frames into the loop.
 *   bool cut_after(int64_t end) const;
 *   void cut_pass_dropped();
 *   void trim_top_pass(int64_t end);
 *   void head_moved(int64_t pos);
 *
 *   // The playhead, in the coordinates it goes forward in, how fast it goes
 *   // round in `mode`, whether the loop keeps to the bars rather than to the
 *   // timeline's frames, and putting it at `y`.
 *   double head_y(int64_t len) const;
 *   double rate_for(LoopMode mode) const;
 *   bool by_quarters() const;
 *   void set_head(double y, int64_t len);
 *
 *   // Capture: the count of input frames, the most it can take, making the
 *   // loop of the `n` frames up to `end` on that count, and after the loop
 *   // plays. Over a loop: whether what came in over the window makes a
 *   // layer, preparing for it, and writing it into the pass just begun.
 *   int64_t capture_clock() const;
 *   int64_t capture_limit() const;
 *   void capture_into(int64_t end, int64_t n);
 *   void captured();
 *   bool capture_layer_ready(const capture_window& w);
 *   void capture_layer_into(const capture_window& w);
 *
 *   // The score stops while a pass is taken in.
 *   void stopping();
 *   void close_on_stop(bool recording);
 *   void write_stop_take(bool recording, double date, int64_t pos);
 *
 *   // Takes: whether one can start, starting one of `passes` passes, the
 *   // work it does a tick, writing its file `item`, whether what it was made
 *   // of changed while it was written, its memory let go of, and the take
 *   // written on the score's stop.
 *   bool export_ready() const;
 *   void start_export(int passes, bool open, bool recording);
 *   void continue_export();
 *   void export_item(int item);
 *   bool take_cancelled();
 *   void take_released();
 *   void stop_take_written();
 *   static constexpr const char* take_extension;   // at most 7 characters
 *   bool take_placeable() const;    // optional: the host can place the take
 *   halp::clip_stretch take_stretch() const;
 *   bool view_full() const;
 *   bool view_writing() const;
 *
 * A looper may also say that an overdub pass is a layer only once something
 * comes in for it:
 *
 *   static constexpr bool passes_wait_for_material = true;
 *
 * Such a pass starts open but out of the stack: it is not a layer, makes no
 * room at Max layers, drops nothing redo would bring back, and the overdub
 * goes on over it. The looper calls material_arriving() before it records
 * the first thing of a pass, which makes the pass a layer starting where the
 * overdub began, as far as it went; and pass_advanced() for what the pass
 * goes over. A pass that never gets anything is no layer, no take, no merge.
 * A looper that does not say so makes its passes layers as they start.
 */
/**
 * What Capture takes over a loop: input frames [end - n; end[ of the capture
 * clock, as the pass an overdub over them would have been. Frame `end` is at
 * `y_end` in the coordinates the playhead goes forward in, which it goes
 * over `rate` positions a frame; `backwards`, they are the loop read from its
 * end.
 */
struct capture_window
{
  int64_t end{};
  int64_t n{};
  int64_t length{};
  double y_end{};
  double rate{1.};
  bool backwards{};

  static int64_t wrap(int64_t k, int64_t len) noexcept
  {
    k %= len;
    return k < 0 ? k + len : k;
  }
  //! Positions of the loop the window goes over, a round at most.
  int64_t positions() const noexcept
  {
    return std::clamp<int64_t>(std::llround(double(n) * rate), 0, length);
  }
  //! The first position of what it goes over, in the loop's own order.
  int64_t start() const noexcept
  {
    const auto y = int64_t(std::floor(y_end));
    return backwards ? wrap(length - y, length) : wrap(y - positions(), length);
  }
  //! The loop position input frame t falls on, as an overdub records it.
  int64_t position(int64_t t) const noexcept
  {
    double y = std::fmod(y_end - double(end - t) * rate, double(length));
    if(y < 0.)
      y += double(length);
    const int64_t k = std::min(int64_t(y), length - 1);
    return backwards ? wrap(length - k, length) : k;
  }
  //! The positions gone over from the window's start to input frame t.
  int64_t progress(int64_t t) const noexcept
  {
    return std::clamp<int64_t>(int64_t(double(t - (end - n)) * rate), 0, positions());
  }
  bool contains(int64_t t) const noexcept { return t >= end - n && t < end; }
};

template <typename Node>
struct looper_node
{
  using LoopMode = Looper::LoopMode;
  using tick = ossia::token_request;

  Looper::transport transport;

  //! A value came in on Mode.
  void loop_arrived() noexcept { m_loop.arrived = true; }

  // The transport's engine
  void looper_close_take(
      const ossia::token_request& tk, LoopMode mode, bool ended_by_postaction);
  int64_t looper_trim_extension(int64_t max_frames);
  void looper_rephase(const ossia::token_request& tk);
  int64_t looper_frames_to_end() const noexcept;
  Looper::loop_period looper_period(const ossia::token_request& tk) const noexcept;
  void looper_point()
  {
    m_switches.apply(self().inputs, self());
    m_speed = Looper::clamp_speed(self().inputs.speed.value);
    m_reverse = self().inputs.reverse.value;
  }

  // The switches' engine
  int looper_passes() const noexcept { return stack().count; }
  bool looper_muted(int pass) const noexcept { return stack().layers[pass].muted; }
  void looper_set_muted(int pass, bool muted)
  {
    if(stack().layers[pass].muted == muted)
      return;
    passes_changing();
    stack().set_muted(pass, muted);
  }
  void looper_mute(int pass) { mute_pass(pass); }
  void looper_erase(int pass) { erase_pass(pass); }

protected:
  static constexpr bool lazy_passes() noexcept
  {
    if constexpr(requires { Node::passes_wait_for_material; })
      return Node::passes_wait_for_material;
    else
      return false;
  }

  //! An overdub pass is open that nothing came in for yet: it is not in the
  //! stack.
  bool pass_pending() const noexcept { return m_pass_pending; }
  //! A pass is open in the stack.
  bool pass_open() const noexcept { return transport.is_taking() && !m_pass_pending; }

  //! Something comes in for the pass being taken in: a pass still waiting
  //! for it becomes a layer, from where the overdub began.
  void material_arriving()
  {
    if(!std::exchange(m_pass_pending, false))
      return;
    begin_pass_now(m_pending_start);
    // Undoing it gives back the loop as it was when the overdub began, before
    // it grew.
    if(auto* top = stack().top())
      top->loop_before = m_pending_length;
    if constexpr(lazy_passes())
      if(m_pending_progress > 0.)
        stack().advance_pass(m_pending_progress, m_pending_backwards);
  }
  //! The pass being taken in went over `frames` more frames of the loop.
  void pass_advanced(double frames, bool backwards)
  {
    if(m_pass_pending)
    {
      m_pending_progress += frames;
      m_pending_backwards = backwards;
    }
    else if constexpr(lazy_passes())
      stack().advance_pass(frames, backwards);
  }
  //! The frames the pass being taken in went over.
  double pass_progress() const noexcept
  {
    if(m_pass_pending)
      return m_pending_progress;
    if constexpr(lazy_passes())
      return stack().pass_progress;
    else
      return 0.;
  }

  //! The passes, or what is heard of them, are about to change.
  void passes_changing()
  {
    m_version++;
    self().layers_changing();
  }

  Node& self() noexcept { return static_cast<Node&>(*this); }
  const Node& self() const noexcept { return static_cast<const Node&>(*this); }
  auto& stack() noexcept { return self().stack(); }
  const auto& stack() const noexcept { return self().stack(); }

  //! From the view: applied at the start of the next tick.
  void view_request(const Looper::erase_request& r) noexcept
  {
    if(r.take > 0 && r.last >= 0)
    {
      m_part_request = r;
      return;
    }
    if(r.take > 0)
    {
      m_take_request = std::max(m_take_request, r.take);
      return;
    }
    m_erase_request = r;
    m_erase_requested = true;
  }

  void run_tick(const tick& tk);
  void apply_buttons();
  void follow_merges() { m_switches.follow_merges(self().inputs, stack().merged_passes); }

  void clear_loop();
  void drop_taken_pass();
  void undo_pass();
  void erase_pass(int pass);
  void mute_pass(int pass);
  int make_room();
  void begin_pass(int64_t start);
  void begin_pass_now(int64_t start);
  void close_pass(bool cycle = false);
  void merge_extra_pass();
  void capture(const tick& tk);
  void capture_layer(const tick& tk, int64_t now, int64_t end);
  void stop_score();

  //! Where the loop would be had it kept going since it was made: the rounds
  //! it would have gone, at its speed. Keeping to the bars, they come from the
  //! timeline's quarters, which a tempo change does not move; else from its
  //! date.
  double rounds_since_made(const ossia::token_request& tk) const noexcept
  {
    const double len = double(stack().length);
    const auto now = transport.timeline(tk);
    const double rounds
        = self().by_quarters()
              ? (now.quarters - m_origin.quarters) / (len / m_rate * m_loop_tempo / 60.)
              : (now.frames - m_origin.frames) / len;
    return rounds * m_speed;
  }

  void run_export();
  void take_written(int slot, int number, bool ok, bool alone);
  int claim_take_slot() noexcept
  {
    const int slot = m_next_take_slot;
    m_next_take_slot = (slot + 1) % Looper::take_writer::slots;
    return slot;
  }

  //! Whether the view is due a message: some 30 a second.
  bool view_due(int64_t frames) noexcept
  {
    if(!self().send_message)
      return false;
    m_view_countdown -= frames;
    if(m_view_countdown > 0)
      return false;
    m_view_countdown = int64_t(m_rate / 30.);
    return true;
  }
  void fill_view(Looper::view_message& v) const noexcept;
  //! Whether the host can place the take just written in the timeline: a
  //! looper says so with take_placeable(), else it can.
  bool placeable_take() const noexcept
  {
    if constexpr(requires(const Node& n) { n.take_placeable(); })
      return self().take_placeable();
    else
      return true;
  }

  double m_rate{48000.};
  double m_tempo{120.};
  ossia::time_signature m_signature{4, 4};
  //! The mode asked for, as the transport takes it up.
  LoopMode m_mode{LoopMode::Stop};
  //! What came in on Mode, and what it shows.
  Looper::loop_input m_loop;
  Looper::switches m_switches;

  //! The playhead goes at m_speed, backwards when m_reverse is set.
  double m_speed{1.};
  bool m_reverse{};
  //! Where position 0 of the loop was on the timeline: its date in frames and
  //! its musical position.
  Looper::transport::timeline_point m_origin{};
  //! Tempo the loop was recorded at: the loop keeps its own speed.
  double m_loop_tempo{120.};
  //! An extending pass grows the loop by the loop it started over; it grew
  //! last on its last cycle.
  int64_t m_extend_unit{};
  bool m_extended{};
  //! The overdub went round with a pass per cycle: going on starts a new
  //! pass. An overdub that stops there leaves none.
  bool m_cycle_pending{};

  // The takes: one is made each time a pass closes, and when the view asks
  // for one. Each file of it is written by the worker, one after the other
  // under one take number: the mix (item -1), then the layers.
  bool m_export_pending{};
  bool m_writing{};
  double m_export_date{};
  //! The date of the take being written: one closing meanwhile does not
  //! change it.
  double m_job_date{};
  double m_export_tempo{120.};
  ossia::time_signature m_export_signature{4, 4};
  int m_export_item{};
  int m_export_layers{};
  bool m_export_mix{};
  int m_export_number{};
  int m_export_slot{};
  double m_stop_date{};
  //! How the take being written, and the one written on the score's stop,
  //! ask the timeline to follow the tempo.
  halp::clip_stretch m_job_stretch{};
  halp::clip_stretch m_stop_stretch{};
  int m_takes_written{};
  //! The last take the view asked for, and the one the take being written
  //! answers.
  int m_take_request{};
  int m_job_request{};
  //! The files of some passes alone the view asked for, the take being
  //! written being those, and the first file of the take.
  Looper::erase_request m_part_request{.pass = -1, .layers = -1};
  bool m_job_part{};
  int m_export_first{};
  //! Counts the changes of the passes, and what it was when the take being
  //! written started.
  int m_version{};
  int m_job_version{};
  //! The overdub pass waiting for something to come in: where it began, the
  //! frames it went over and which way.
  bool m_pass_pending{};
  int64_t m_pending_start{};
  int64_t m_pending_length{};
  double m_pending_progress{};
  bool m_pending_backwards{};
  int m_next_take_slot{};

  int64_t m_view_countdown{};
  Looper::view_message m_view{};
  Looper::erase_request m_erase_request{};
  bool m_erase_requested{};
};

template <typename Node>
void looper_node<Node>::run_tick(const tick& tk)
{
  auto& in = self().inputs;
  auto& st = stack();
  const auto state = self().ossia_state;
  m_rate = state.sampleRate();
  m_tempo = tk.tempo;
  m_signature = tk.signature;
  m_speed = Looper::clamp_speed(in.speed.value);
  m_reverse = in.reverse.value;
  self().tick_started(tk);

  apply_buttons();
  follow_merges();
  if(in.capture.value)
    capture(tk);
  self().prepare_loop();

  const bool empty = st.empty() || self().busy();
  Looper::read_loop(in.mode, transport, m_loop, empty);
  m_switches.read(in, transport, self(), empty);
  transport.settle(empty);

  m_mode = transport.wanted;
  const Looper::transport_settings settings{
      .mode = m_mode,
      .quantif = in.quantif.value,
      .after_record = in.after_record,
      .postaction_bars = in.postaction_bars,
      .start_from = in.start_from,
      .late_seconds = in.late.value / 1000.,
      .stop_at = in.stop_at.value};
  transport(tk, state, settings, self());
  // Only a pass that starts with the take closing gets what it cut off.
  self().forget_carried();
  Looper::show_loop(in.mode, transport, m_loop);
  follow_merges();
  m_switches.follow(in, st.count);

  self().tick_finished(tk);
  follow_merges();
  run_export();

  self().outputs.phase.value
      = st.length > 0 ? float(transport.playbackPos % st.length) / float(st.length) : 0.f;
}

template <typename Node>
void looper_node<Node>::apply_buttons()
{
  auto& in = self().inputs;
  if(in.clear.value)
    clear_loop();

  // The m and x of a lane are layer actions as any: on the grid.
  if(std::exchange(m_erase_requested, false))
    m_switches.add(
        {m_erase_request.mute ? Looper::layer_action::mute : Looper::layer_action::erase,
         m_erase_request.pass, m_erase_request.layers},
        transport);

  if(in.undo.value || in.redo.value)
    m_switches.soloed.forget();
  if(in.undo.value)
  {
    if(pass_open())
      drop_taken_pass();
    else
    {
      passes_changing();
      undo_pass();
    }
  }

  if(in.redo.value && !pass_open())
  {
    passes_changing();
    const int out = stack().leaves_on_redo();
    if(stack().redo())
    {
      if(out >= 0)
        m_switches.removed(in, out);
      merge_extra_pass();
    }
  }
}

//! A recording going on starts again on the next frame, its bars and tempo
//! counted from there.
template <typename Node>
void looper_node<Node>::clear_loop()
{
  m_switches.soloed.forget();
  passes_changing();
  self().cleared();
  stack().clear();
  m_pass_pending = false;
  transport.playbackPos = 0;
  // Whatever was being taken in is gone with the rest.
  if(transport.is_taking())
    transport.taking(LoopMode::Stop);
  transport.restart_recording();
}

//! The pass being taken in is dropped; taking in starts a new one. A
//! recording starts again on the next frame, its bars and tempo counted from
//! there.
template <typename Node>
void looper_node<Node>::drop_taken_pass()
{
  passes_changing();
  if(transport.actualMode == LoopMode::Record)
  {
    stack().clear();
    transport.playbackPos = 0;
    transport.restart_recording();
  }
  else
  {
    self().pass_dropped();
    undo_pass();
    stack().drop_redo();
  }
  transport.taking(LoopMode::Stop);
}

//! Undoes the top pass: a layer it replaced comes back to its place, the one
//! Layer says keeping its own.
template <typename Node>
void looper_node<Node>::undo_pass()
{
  const int back = stack().returns_on_undo();
  if(stack().undo() && back >= 0)
    m_switches.inserted(self().inputs, back, stack().count);
}

//! Erasing the pass being taken in is undoing it, and erasing the only pass
//! heard is clearing the loop.
template <typename Node>
void looper_node<Node>::erase_pass(int pass)
{
  auto& st = stack();
  if(pass < 0 || pass >= st.count || self().busy())
    return;
  m_switches.soloed.forget();
  if(st.count == 1)
    clear_loop();
  else if(pass_open() && pass == st.count - 1)
    drop_taken_pass();
  else
  {
    passes_changing();
    if(st.erase(pass))
      m_switches.removed(self().inputs, pass);
  }
}

//! A muted pass is kept as it is; muting it again lets it be heard.
template <typename Node>
void looper_node<Node>::mute_pass(int pass)
{
  auto& st = stack();
  if(pass < 0 || pass >= st.count || self().busy())
    return;
  m_switches.soloed.forget();
  passes_changing();
  st.set_muted(pass, !st.layers[pass].muted);
}

//! There are Max layers, and a pass starts: with Replace a muted layer, the
//! oldest muted layer goes for it, else the newest. The layer that goes, or
//! -1 for none.
template <typename Node>
int looper_node<Node>::make_room()
{
  auto& st = stack();
  if(self().inputs.at_max_layers.value != Looper::AtMaxLayers::ReplaceMuted
     || st.count < self().max_layers() || st.count < 2)
    return -1;
  int pass = st.count - 1;
  for(int i = 0; i < st.count; i++)
    if(st.layers[i].muted)
    {
      pass = i;
      break;
    }
  m_switches.soloed.forget();
  passes_changing();
  m_switches.removed(self().inputs, pass);
  return pass;
}

//! Starts an overdub pass at `start`, room made for it as At max layers says:
//! a layer it replaces is kept for undo until the next pass starts.
template <typename Node>
void looper_node<Node>::begin_pass(int64_t start)
{
  if constexpr(lazy_passes())
  {
    m_pass_pending = true;
    m_pending_start = start;
    m_pending_length = stack().length;
    m_pending_progress = 0.;
    m_pending_backwards = false;
    return;
  }
  begin_pass_now(start);
}

template <typename Node>
void looper_node<Node>::begin_pass_now(int64_t start)
{
  m_version++;
  const int replaced = make_room();
  self().begin_overdub(
      start, self().inputs.at_max_layers.value == Looper::AtMaxLayers::MergeOldest,
      replaced);
}

//! The overdub pass is over, at the end of the take or of a `cycle`: it is
//! written in a take, and with Merge into newest, past Max layers, merged
//! into the newest layer.
template <typename Node>
void looper_node<Node>::close_pass(bool cycle)
{
  // Nothing came in: there is no pass to close.
  if(std::exchange(m_pass_pending, false))
  {
    if(!cycle)
      m_extended = false;
    return;
  }
  auto* top = stack().top();
  if(!top)
    return;
  // A loop grown by the cycle that ended can still be cut back by a late
  // stop.
  if(!cycle)
    m_extended = false;
  m_version++;
  m_export_pending = true;
  if(!self().pass_closing())
    return;
  top->loop_after = stack().length;
  self().pass_closed();
  merge_extra_pass();
}

template <typename Node>
void looper_node<Node>::merge_extra_pass()
{
  if(self().inputs.at_max_layers.value == Looper::AtMaxLayers::MergeIntoNewest
     && stack().count > self().max_layers() && self().can_merge_newest())
  {
    passes_changing();
    self().merge_newest();
  }
}

template <typename Node>
void looper_node<Node>::looper_close_take(
    const ossia::token_request& tk, LoopMode mode, bool ended_by_postaction)
{
  auto& st = stack();
  if(st.count == 0)
    return;

  if(mode == LoopMode::Record)
  {
    m_loop_tempo = transport.loop_tempo(tk, st.length);
    const double bar_frames = m_rate * 4. * (double(tk.signature.upper) / tk.signature.lower)
                              * (60. / tk.tempo);
    const auto bars = int64_t(std::floor(transport.postaction_bars * bar_frames));

    // The bar count says when the post-action takes over, so it is the length
    // of a recording the post-action ended: what was recorded past the bars,
    // e.g. the tempo having dropped while it went on, is cut off as a late
    // stop cuts it, for the overdub that follows. A recording stopped by hand
    // is as long as it was played for.
    const bool to_bars = ended_by_postaction && transport.quantized()
                         && transport.postaction_bars > 0 && bars > 0;
    if(to_bars && bars < st.length)
      self().looper_trim_recording(st.length - bars);
    self().close_recording(to_bars ? bars : 0);
    m_export_date = transport.recordStartDate.impl / double(ossia::flicks_per_second<int64_t>);

    // Position 0 of the loop on the timeline: the loop plays on from
    // playbackPos at the point the take closes.
    const auto at = double(transport.playbackPos % std::max<int64_t>(1, st.length));
    m_origin = transport.timeline(tk);
    m_origin.frames -= at;
    m_origin.quarters -= at / m_rate * m_loop_tempo / 60.;
  }
  else
  {
    // An overdub that got nothing leaves no take.
    if(m_pass_pending)
    {
      self().close_overdub();
      m_extended = false;
      return;
    }
    self().close_overdub();
  }
  m_version++;
  m_extended = false;
  m_export_pending = true;
}

//! An extending pass asked to stop just after the loop grew ends on the old
//! end, as a late stop would.
template <typename Node>
int64_t looper_node<Node>::looper_trim_extension(int64_t max_frames)
{
  auto& st = stack();
  if(!m_extended || m_extend_unit <= 0 || st.count == 0)
    return -1;
  const int64_t end = st.length - m_extend_unit;
  const int64_t into_last = transport.playbackPos - end;
  if(into_last < 0 || into_last > max_frames || end < m_extend_unit)
    return -1;
  if(m_pass_pending)
  {
    // The pass after the cut got nothing: it was never a layer.
    m_pass_pending = false;
    self().cut_pass_dropped();
    st.length = end;
    if(auto* t = st.top())
      t->loop_after = end;
  }
  else if(st.count > 1 && self().cut_after(end))
  {
    // A pass per cycle cut the overdub on the old end: the pass after the cut
    // has nothing left, and goes as undo takes a pass out. The one before it
    // ends on the old end, closed already.
    self().pass_dropped();
    undo_pass();
    st.drop_redo();
    self().cut_pass_dropped();
    st.length = end;
    if(auto* t = st.top())
      t->loop_after = end;
  }
  else
  {
    self().trim_top_pass(end);
    st.length = end;
  }
  m_version++;
  m_extended = false;
  self().head_moved(into_last);
  return into_last;
}

//! An extending overdub grows the loop at its end instead of reaching it.
template <typename Node>
int64_t looper_node<Node>::looper_frames_to_end() const noexcept
{
  const LoopMode m = transport.actualMode;
  const auto& st = stack();
  if(st.empty() || self().busy() || (m != LoopMode::Play && m != LoopMode::Overdub))
    return -1;
  if(m == LoopMode::Overdub && self().inputs.past_end.value == Looper::PastEnd::Extend
     && !m_reverse)
    return -1;
  return Looper::frames_to_loop_end(self().head_y(st.length), st.length, self().rate_for(m));
}

//! The loop as Quantize's Loop: the frames a round of it takes at the rate it
//! goes, and where the playhead is in it, the way it goes. An extending
//! overdub's round is the loop as long as it is now. A stopped loop is where
//! it would be had it kept going since it was made, as In phase has it.
template <typename Node>
Looper::loop_period
looper_node<Node>::looper_period(const ossia::token_request& tk) const noexcept
{
  const LoopMode m = transport.actualMode;
  const auto& st = stack();
  if(st.empty() || self().busy() || st.length <= 0 || m == LoopMode::Record)
    return {};
  const double rate = self().rate_for(m);
  const int64_t round = Looper::loop_round(st.length, rate);
  if(m == LoopMode::Stop)
    return {
        round, int64_t(std::floor(rounds_since_made(tk) * double(round) + 1e-6)), false};
  const int64_t to_end = Looper::frames_to_loop_end(self().head_y(st.length), st.length, rate);
  return {round, std::clamp<int64_t>(round - to_end, 0, round), true};
}

//! The loop is heard again: the playhead goes where it would be had the loop
//! kept playing at its speed since it was made.
template <typename Node>
void looper_node<Node>::looper_rephase(const ossia::token_request& tk)
{
  const auto& st = stack();
  if(st.empty())
    return;
  const double rounds = rounds_since_made(tk);
  // A whole position: what is left of a frame is rounding.
  self().set_head(
      std::floor((rounds - std::floor(rounds)) * double(st.length) + 1e-6), st.length);
}

//! What was heard lately, ending on the last grid point when it was just
//! passed, as a late stop would: the last bars of it become the loop, or,
//! over a loop, its last round becomes a layer.
template <typename Node>
void looper_node<Node>::capture(const tick& tk)
{
  if(transport.is_taking() || self().busy())
    return;
  auto& in = self().inputs;
  const double bar = m_rate * 4. * (double(tk.signature.upper) / tk.signature.lower)
                     * (60. / std::max(1., tk.tempo));
  const int64_t now = self().capture_clock();
  int64_t end = now;
  if(transport.quantized() && transport.lastPointFrame)
  {
    const int64_t late = transport.frame - *transport.lastPointFrame;
    if(late >= 0 && late <= int64_t(in.late.value * m_rate / 1000.))
      end -= late;
  }
  if(!stack().empty())
  {
    capture_layer(tk, now, end);
    return;
  }
  const int64_t n = std::min<int64_t>(
      {int64_t(in.capture_bars.value * bar), end, self().capture_limit()});
  if(n <= 0)
    return;

  passes_changing();
  m_switches.soloed.forget();
  self().capture_into(end, n);

  transport.play_captured((now - end) % n);
  m_loop_tempo = tk.tempo;
  m_origin = Looper::timeline_start(tk, self().ossia_state);
  m_origin.frames -= double(transport.playbackPos);
  m_origin.quarters -= double(transport.playbackPos) / m_rate * tk.tempo / 60.;
  const double model_per_frame = self().ossia_state.samplesToModel();
  m_export_date = (tk.prev_date.impl - (now - end + n) * model_per_frame)
                  / double(ossia::flicks_per_second<int64_t>);
  m_export_pending = true;
  self().captured();
}

//! The last round of the loop up to `end`, played over it, becomes a layer
//! on top, as an overdub pass of that round would have: where it was played
//! in the loop, room made for it as for any pass, undone and redone as one,
//! written in a take when it closes. The loop keeps its length. A stopped
//! loop goes round where it would be had it kept going, as In phase has it,
//! and plays from there.
template <typename Node>
void looper_node<Node>::capture_layer(const tick& tk, int64_t now, int64_t end)
{
  auto& st = stack();
  const int64_t len = st.length;
  const LoopMode m = transport.actualMode;
  const bool stopped = m != LoopMode::Play;
  const double rate = self().rate_for(m);
  if(len <= 0 || !(rate > 0.))
    return;
  double y_now{};
  if(stopped)
  {
    const double rounds = rounds_since_made(tk);
    y_now = std::floor((rounds - std::floor(rounds)) * double(len) + 1e-6);
  }
  else
    y_now = self().head_y(len);

  const auto round = int64_t(std::ceil(double(len) / rate - 1e-9));
  double y_end = std::fmod(y_now - double(now - end) * rate, double(len));
  if(y_end < 0.)
    y_end += double(len);
  Looper::capture_window w{
      .end = end,
      .n = std::min<int64_t>({round, end, self().capture_limit()}),
      .length = len,
      .y_end = y_end,
      .rate = rate,
      .backwards = m_reverse};
  if(w.n <= 0 || w.positions() <= 0 || !self().capture_layer_ready(w))
    return;

  passes_changing();
  const auto pushes = st.pushes;
  begin_pass_now(w.start());
  if(st.pushes == pushes)
    return;
  self().capture_layer_into(w);
  close_pass();

  if(stopped)
  {
    self().set_head(y_now, len);
    transport.play_captured(transport.playbackPos);
    self().captured();
  }
}

//! The score stops: what was being taken in is a pass like any other, written
//! in a take of its own, and Mode goes from Record or Overdub to Play. The
//! loop is kept: only the transport starts over.
template <typename Node>
void looper_node<Node>::stop_score()
{
  auto& st = stack();
  // A pass that got nothing is no take.
  const bool taking = pass_open();
  m_pass_pending = false;
  const bool recording = taking && transport.actualMode == LoopMode::Record;
  const double date
      = recording ? transport.recordStartDate.impl / double(ossia::flicks_per_second<int64_t>)
                  : m_export_date;
  const int64_t pos = st.length > 0 ? transport.playbackPos % st.length : 0;
  self().stopping();
  if(taking)
  {
    self().close_on_stop(recording);
    m_stop_stretch = self().take_stretch();
    self().write_stop_take(recording, date, pos);
  }
  transport.reset();
  m_switches.reset();
  transport.stopped(st.empty());
  Looper::show_loop(self().inputs.mode, transport, m_loop);
  m_view_countdown = 0;
}

//! A take starts once the last one is written, when a pass closed or the view
//! asked for one. Asked for while a pass is taken in, the take has every
//! pass, that one as far as it went, and a recording is placed where it
//! started, at the tempo it is played at. Else it is the passes that closed:
//! one started since is not in it.
template <typename Node>
void looper_node<Node>::run_export()
{
  auto& st = stack();
  const bool asked = m_take_request > m_view.answered;
  if(asked && st.empty())
  {
    // Nothing to take: the last take is what there is.
    m_view.answered = m_take_request;
    m_view_countdown = 0;
  }
  // Passes asked for alone, of the loop the view showed: one that changed
  // since has nothing to give.
  auto& part = m_part_request;
  const bool part_asked = part.take > m_view.part_answered;
  if(part_asked
     && (st.empty() || part.layers != st.count || part.pass < 0 || part.last < part.pass
         || part.last >= st.count))
  {
    m_view.part_answered = part.take;
    part.take = 0;
    m_view_countdown = 0;
  }
  else if(
      part_asked && !m_writing && self().writer && self().export_ready())
  {
    // The files of the passes as they are now, the one taken in as far as it
    // went, under a take number of their own.
    m_job_part = true;
    m_job_request = part.take;
    part.take = 0;
    const bool open = pass_open();
    const bool recording = open && transport.actualMode == LoopMode::Record;
    m_job_stretch = self().take_stretch();
    m_export_tempo = recording ? m_tempo : m_loop_tempo;
    m_export_signature = m_signature;
    m_export_mix = false;
    m_export_first = part.pass;
    m_export_layers = part.last + 1;
    m_export_number = 0;
    m_export_slot = claim_take_slot();
    self().start_export(st.count, open, recording);
  }
  else if(
      (m_export_pending || asked) && !m_writing && self().writer && !st.empty()
      && self().export_ready())
  {
    m_export_pending = false;
    m_job_part = false;
    m_job_request = m_take_request;
    m_job_version = m_version;
    const bool open = asked && pass_open();
    const bool recording = open && transport.actualMode == LoopMode::Record;
    if(recording)
      m_export_date = transport.recordStartDate.impl / double(ossia::flicks_per_second<int64_t>);
    m_job_date = m_export_date;
    m_job_stretch = self().take_stretch();
    m_export_tempo = recording ? m_tempo : m_loop_tempo;
    m_export_signature = m_signature;
    const int passes
        = open || !pass_open() ? st.count : std::max(1, st.count - 1);
    const auto files = self().inputs.exported.value;
    m_export_mix = files != Looper::TakeFiles::Layers;
    m_export_layers = files != Looper::TakeFiles::Mix ? passes : 0;
    m_export_first = m_export_mix ? -1 : 0;
    m_export_number = 0;
    m_export_slot = claim_take_slot();
    self().start_export(passes, open, recording);
  }
  self().continue_export();
}

template <typename Node>
void looper_node<Node>::take_written(int slot, int number, bool ok, bool alone)
{
  auto& writer = self().writer;
  if(alone)
  {
    self().stop_take_written();
    if(!ok || !writer)
      return;
    m_takes_written++;
    m_view.set_take(
        writer->take_base(slot), Node::take_extension, true, 0, m_stop_stretch,
        placeable_take());
    m_view.take = m_takes_written;
    m_view.take_version = -1;
    m_view.take_date = m_stop_date;
    m_view_countdown = 0;
    return;
  }
  m_writing = false;
  if(self().take_cancelled() || !ok || !writer)
  {
    // What was being written changed, or the file could not be: the take
    // stops there.
    self().take_released();
    auto& answered = m_job_part ? m_view.part_answered : m_view.answered;
    answered = std::max(answered, m_job_request);
    m_view_countdown = 0;
    return;
  }

  m_export_number = number;
  if(m_export_item + 1 < m_export_layers)
  {
    self().export_item(m_export_item + 1);
    return;
  }

  self().take_released();
  if(m_job_part)
  {
    m_view.set_part(
        writer->take_base(slot), m_export_first, m_export_layers - 1, m_job_request,
        m_job_stretch, placeable_take());
    m_view.part_answered = std::max(m_view.part_answered, m_job_request);
    m_view_countdown = 0;
    return;
  }
  m_takes_written++;
  m_view.take_version = m_job_version;
  m_view.set_take(
      writer->take_base(slot), Node::take_extension, m_export_mix, m_export_layers,
      m_job_stretch, placeable_take());
  m_view.take = m_takes_written;
  m_view.take_date = m_job_date;
  m_view.answered = std::max(m_view.answered, m_job_request);
  // Force a view update with the new take.
  m_view_countdown = 0;
}

//! What the view shows of the transport and of the passes, the oldest of
//! which share the first lane when there are more of them than lanes.
template <typename Node>
void looper_node<Node>::fill_view(Looper::view_message& v) const noexcept
{
  const auto& st = stack();
  v.empty = st.empty();
  v.mode = int(transport.actualMode);
  v.requested = int(transport.quantizedPlayMode);
  v.wanted = int(m_mode);
  v.layers = st.count;
  v.redo = st.total - st.count;
  v.full = self().view_full();
  v.length_seconds = float(st.length / m_rate);
  v.position
      = st.length > 0 ? float(transport.playbackPos % st.length) / float(st.length) : 0.f;
  v.tempo = float(m_tempo);
  v.writing = self().view_writing();
  v.version = m_version;
  v.reverse = m_reverse;
  v.speed = float(m_speed);
  v.selected = self().inputs.layer.value - 1;
  v.restart_pending = transport.restart_pending();
  v.new_pass_pending = transport.new_pass_pending();
  const int lanes = std::min(st.count, Looper::view_message::max_lanes);
  const int merged = st.count - lanes;
  v.muted.fill(0);
  for(int l = merged > 0 ? 1 : 0; l < lanes; l++)
    v.muted[l] = st.layers[l + merged].muted;
}
}
