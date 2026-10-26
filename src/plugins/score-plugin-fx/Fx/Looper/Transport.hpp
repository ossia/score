#pragma once
#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/token_request.hpp>

#include <halp/grid.hpp>
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

/**
 * What the audio and the MIDI looper share: when they record, overdub, play
 * or stop, sample by sample.
 *
 * The loopers ask for a mode; the transport waits for the next point of the
 * quantification grid before taking it up, splits the tick there, and hands
 * the looper the spans of the tick with the mode each of them is in. A
 * recording can end on its own after a number of bars, into After record.
 *
 * The looper (the Engine below) provides:
 *
 *   // Does what transport.actualMode says over [start; start + length[ of
 *   // the tick, relative to the tick's first sample. Returns the frames it
 *   // did: fewer when it ended a recording itself, with end_recording(),
 *   // and the rest is run again once the take is closed.
 *   int64_t looper_run(int64_t start, int64_t length);
 *
 *   // Something was recorded or overdubbed since the last call, and the
 *   // mode is changing: the take is complete. `mode` is what was taking in
 *   // (Record or Overdub).
 *   void looper_close_take(
 *       const ossia::token_request& tk, Looper::LoopMode mode,
 *       bool ended_by_postaction);
 *
 *   // A recording stopped this many frames after a grid point ends on that
 *   // point: what was recorded since is dropped.
 *   void looper_trim_recording(int64_t frames);
 *
 *   // An overdub that made the loop longer is stopped: when the playhead is
 *   // at most `max_frames` into the part the loop last grew by, that part is
 *   // dropped and the frames the playhead is into it are returned, else -1.
 *   int64_t looper_trim_extension(int64_t max_frames);
 *
 *   // StartFrom::InPhase: the loop starts being heard again, at the start of
 *   // the span of `tk`; the looper puts the playhead where the loop would be
 *   // had it kept playing since it was made, by the timeline's clock.
 *   void looper_rephase(const ossia::token_request& tk);
 *
 *   // What the looper asked request_point() for is done now, on the grid
 *   // point the transport takes its requests up on.
 *   void looper_point();
 *
 *   // Frames until the playhead reaches the end of the loop, the way it goes,
 *   // or -1 when it is not going round a loop.
 *   int64_t looper_frames_to_end() const;
 *
 *   // The loop as the period of Quantif's Loop grid, at the start of the
 *   // span of `tk`; see loop_period.
 *   Looper::loop_period looper_period(const ossia::token_request& tk) const;
 *
 * The looper calls transport.taking(mode) whenever it records or overdubs.
 */
namespace Nodes::Looper
{
// Order matters: saved values are indices.
enum class LoopMode
{
  Play,
  Record,
  Overdub,
  Stop
};

//! What a recording gives way to when it ends: its bars running out, a full
//! buffer, Rec / Play / Dub. An overdub starts on the frame the recording ends
//! on.
//!
//! Order matters: this is the Post-action of older scores.
enum class AfterRecord
{
  Play,
  Overdub
};

//! When a stop is taken up.
enum class StopAt
{
  Grid,    ///< On the next point of the grid, as any change of mode.
  LoopEnd, ///< When the loop has played to its end.
  Now      ///< At once.
};

//! What Rec / Play / Dub does while overdubbing.
enum class AfterOverdub
{
  Play,    ///< Plays the loop.
  NewLayer ///< Goes on overdubbing in a new layer, as New layer does.
};

//! The actions that are not "set Loop to a mode". Each is taken up on the
//! next grid point, as a change of Loop is.
enum class TransportAction
{
  RecPlayDub, ///< Records an empty loop, then plays, overdubs, plays again.
  PlayStop,   ///< Plays a stopped loop, stops anything else.
  NewLayer,   ///< While overdubbing, ends the layer and starts another.
  Retrigger   ///< Back to the loop's start, in the same mode.
};

//! Where the loop is read from when the mode changes.
//!
//! Order matters: this is the Restart of older scores, whose Always,
//! Recording and In phase these are.
enum class StartFrom
{
  LoopStart, ///< Every change of mode reads the loop from its beginning again.
  WhereItWas, ///< Only a recording does; playing and overdubbing carry straight on.
  InPhase    ///< As WhereItWas, and a loop heard again after a stop, a transport
             ///< start or a jump of the timeline is where it would be had it
             ///< kept playing.
};

//! What an overdub that goes round the loop several times leaves.
enum class Passes
{
  Whole,   ///< One pass, from when the overdub starts to when it stops.
  PerCycle ///< A pass for each time it goes round, cut on the loop's start.
};

//! What a pass that would make more layers than Max layers does.
enum class AtMaxLayers
{
  MergeOldest,    ///< The two oldest layers are merged into one.
  MergeIntoNewest, ///< The pass is merged into the newest layer once it ends.
  ReplaceMuted    ///< The pass takes the place of the oldest muted layer, else
                  ///< of the newest one.
};

//! The merges of a loop's passes since the looper last read them: each one
//! made pass `at[i]` and the one over it a single pass, in that order.
struct merge_log
{
  static constexpr int max_merges = 64;
  std::array<int8_t, max_merges> at{};
  int count{};

  void add(int pass) noexcept
  {
    if(count < max_merges)
      at[std::size_t(count++)] = int8_t(pass);
  }
};

//! The speeds a loop is played at, as a tape: the pitch follows.
inline constexpr std::array<std::pair<const char*, float>, 5> speeds{
    {{"x1/4", 0.25f}, {"x1/2", 0.5f}, {"x1", 1.f}, {"x2", 2.f}, {"x4", 4.f}}};

template <halp::static_string Name>
struct speed_selector
{
  static clang_buggy_consteval auto name() { return std::string_view{Name.value}; }
  enum widget
  {
    combobox
  };
  struct range
  {
    decltype(speeds) values = speeds;
    float init = 1.f;
  };
  float value = 1.f;
};

//! A speed the loopers can play at, whatever a port sends.
inline double clamp_speed(float s) noexcept
{
  return std::isfinite(s) ? std::clamp(double(s), 0.25, 4.) : 1.;
}

// Order matters: this replaced a toggle, and a stored 0/1 has to keep
// meaning what it did.
enum class Passthrough
{
  None,   ///< The input is never heard, in any mode.
  Record, ///< Heard while recording and overdubbing; playback is the loop alone.
  Full    ///< Always heard, mixed over the loop during playback.
};

//! What an overdub does at the end of the loop.
enum class PastEnd
{
  Wrap,  ///< Goes round the loop again.
  Extend ///< Makes the loop longer by the loop's length, the passes under it repeating.
};

//! How an audio loop follows the tempo of the score, as sound files do.
enum class FollowTempo
{
  Off,
  Repitch,
  RepitchFast,
  Timestretch,
  TimestretchPercussive
};

//! The units Quantif offers: a time, a note value, bars, or the loop itself.
inline constexpr halp::grid_setup quantif_grid{
    .units = halp::grid_units::free | halp::grid_units::note | halp::grid_units::bars
             | halp::grid_units::period,
    .init = halp::grid_value::note(0.25),
    .max_bars = 16};

//! A grid that never waits: none, or nothing the transport can find points of.
inline bool no_grid(const halp::grid_value& g) noexcept
{
  return g.none() || !(g.value > 0.) || g.unit == halp::grid_unit::parent;
}

//! The loop as the period of a grid, in frames of the tick: how long the
//! playhead takes to go round it at the speed it goes, and where it is.
struct loop_period
{
  //! 0: no loop yet (empty, or being recorded).
  int64_t length{};
  //! Going round: frames into the current round, in [0; length]. Stopped:
  //! frames since the loop was made, by the timeline, over all the rounds it
  //! would have gone.
  int64_t position{};
  bool going_round{};
};

struct transport_settings
{
  LoopMode mode{LoopMode::Stop};
  halp::grid_value quantif{};
  AfterRecord after_record{};
  int postaction_bars{};
  StartFrom start_from{};
  //! A recording asked to stop at most this long after a grid point ends on
  //! that point instead of waiting for the next one.
  double late_seconds{};
  StopAt stop_at{};
};

struct transport
{
  //! The mode asked for: Loop, or what an action, a recording's end or a full
  //! buffer changed it to. The looper shows it in Loop, so that what Loop
  //! says is what the looper does.
  LoopMode wanted{LoopMode::Stop};
  LoopMode quantizedPlayMode{LoopMode::Stop};
  LoopMode actualMode{LoopMode::Stop};
  int64_t playbackPos{};
  ossia::quarter_note recordStartBar{-1.};
  ossia::quarter_note recordEndBar{-1.};
  //! The grid of this tick; no_grid() when changes are taken up at once.
  halp::grid_value quantif{};
  bool quantized() const noexcept { return !no_grid(quantif); }
  std::optional<ossia::time_value> this_buffer_quantif_time;
  int64_t tickStartSample{};
  int postaction_bars{};
  double sampleRate{48000.};
  //! Whether its bars running out is what ended the recording, rather than the
  //! mode being changed by hand.
  bool endedByPostaction{false};

  //! Frames run since the transport was created, at the start of this tick.
  int64_t frame{};
  //! Where the last grid point fell, on the same count.
  std::optional<int64_t> lastPointFrame;
  //! Model date at which the current recording started.
  ossia::time_value recordStartDate{};
  //! Musical position at which the current recording started.
  ossia::quarter_note recordStartPosition{};

  //! The engine's frame count at the start of the span of `tk`: the same
  //! clock for every looper, which is what keeps loops of a sync group in
  //! phase.
  int64_t engine_frame(const ossia::token_request& tk) const noexcept
  {
    // The engine's count is at the end of the buffer while the graph runs.
    return m_facade.samplesSinceStart() - m_facade.bufferSize()
           + m_facade.timings(tk).start_sample + m_close_offset;
  }

  //! The tempo of a recording that is closing: the quarters it went over for
  //! the frames it kept. A tempo change during the recording is accounted
  //! for; the tempo of the moment is used where there are no quarters.
  double loop_tempo(const ossia::token_request& tk, int64_t frames) const noexcept
  {
    double quarters = tk.musical_start_position - recordStartPosition;
    // A late stop dropped the frames after the grid point, not their quarters.
    if(m_closing_late)
      quarters -= m_late_frames / sampleRate * tk.tempo / 60.;
    if(quarters > 1e-3 && frames > 0 && sampleRate > 0)
      return 60. * quarters / (frames / sampleRate);
    return tk.tempo;
  }

  //! Where the timeline is at the start of the span of `tk`: its date in
  //! frames, and its musical position in quarters. A take closed by the
  //! looper itself in the middle of a span closes at that point of it.
  struct timeline_point
  {
    double frames;
    double quarters;
  };
  timeline_point timeline(const ossia::token_request& tk) const noexcept
  {
    const double ratio = m_facade.modelToSamples();
    const double frames = double(tk.prev_date.impl) * ratio + double(m_close_offset);
    const auto& t = m_tick;
    double quarters = t.musical_start_position;
    if(const double span = double((t.date - t.prev_date).impl); span > 0.)
      quarters += (t.musical_end_position - t.musical_start_position)
                  * (double((tk.prev_date - t.prev_date).impl) / span);
    if(sampleRate > 0.)
      quarters += double(m_close_offset) / sampleRate * t.tempo / 60.;
    return {frames, quarters};
  }

  //! Called by the looper whenever it records or overdubs.
  void taking(LoopMode m) noexcept { m_taking = m; }
  bool is_taking() const noexcept { return m_taking != LoopMode::Stop; }

  //! What the recording going on, or about to start, gives way to when it
  //! ends by itself or by Rec / Play / Dub: overdubbing when it was asked for
  //! as an overdub of an empty loop, else After record.
  LoopMode end_of_recording(AfterRecord after) const noexcept
  {
    return m_into_overdub || after == AfterRecord::Overdub ? LoopMode::Overdub
                                                           : LoopMode::Play;
  }

  //! A recording reaching the end of its buffer ends as if its bars had run
  //! out.
  void end_recording(AfterRecord after) noexcept
  {
    recordStartBar = -1.;
    recordEndBar = -1.;
    endedByPostaction = false;
    actualMode = end_of_recording(after);
    recording_handed_over();
    m_forced_close = true;
  }

  //! Loop set to `m`, from its port or by Play, Stop, Record or Overdub: what
  //! is asked for from now on, whatever was waiting for the grid. Overdubbing
  //! an empty loop records it, as Record does, and the recording then gives
  //! way to overdubbing. `empty`: there is no loop and none is being recorded.
  void ask(LoopMode m, bool empty) noexcept
  {
    m_queued = 0;
    if(m == LoopMode::Overdub && empty)
    {
      m_into_overdub = true;
      wanted = LoopMode::Record;
      return;
    }
    if(m != LoopMode::Record)
      m_into_overdub = false;
    wanted = m;
  }

  //! A loop emptied while Overdub is asked for, e.g. by Clear: it is recorded.
  void settle(bool empty) noexcept
  {
    if(wanted == LoopMode::Overdub && empty && m_queued == 0)
      ask(LoopMode::Overdub, true);
  }

  //! An action. Rec / Play / Dub and Play / Stop act on the mode the looper
  //! will be in once what waits for the grid is taken up: pressed again
  //! before the grid point, they queue their change after it, and each change
  //! is taken up on a grid point of its own. `empty`: there is no loop and
  //! none is being recorded.
  void press(TransportAction a, bool empty, AfterRecord after_record, AfterOverdub after) noexcept
  {
    const LoopMode now = m_queued > 0 ? m_queue[m_queued - 1].mode : wanted;
    // A recording waiting makes the loop that the next change acts on.
    bool none = empty && now != LoopMode::Record;
    for(int i = 0; i < m_queued; i++)
      none = none && m_queue[i].mode != LoopMode::Record;
    none = none && wanted != LoopMode::Record;

    switch(a)
    {
      case TransportAction::RecPlayDub:
        if(now == LoopMode::Record)
          push({end_of_recording(after_record), false});
        else if(none)
          push({LoopMode::Record, false});
        else if(now == LoopMode::Play)
          push({LoopMode::Overdub, false});
        else if(now == LoopMode::Overdub)
          push({after == AfterOverdub::Play ? LoopMode::Play : LoopMode::Overdub,
                after == AfterOverdub::NewLayer});
        else
          push({LoopMode::Play, false});
        break;
      case TransportAction::PlayStop:
        if(now != LoopMode::Stop)
          push({LoopMode::Stop, false});
        else if(!none)
          push({LoopMode::Play, false});
        break;
      case TransportAction::NewLayer:
        if(now == LoopMode::Overdub && !empty)
          push({LoopMode::Overdub, true});
        break;
      case TransportAction::Retrigger:
        if(!empty || wanted == LoopMode::Record)
          m_restart = true;
        break;
    }
  }

  //! Undo or Clear dropped the recording going on: it starts again on the
  //! next frame, with its bars, start and tempo counted from there.
  void restart_recording() noexcept
  {
    if(actualMode == LoopMode::Record)
      m_rerecord = true;
  }

  //! The looper has something to do on the next grid point: the transport
  //! calls its looper_point() there.
  void request_point() noexcept { m_point_requested = true; }

  //! Changes queued after the one waiting for the grid.
  bool queued() const noexcept { return m_queued > 0; }

  //! What waits for the next grid point besides a change of mode.
  bool restart_pending() const noexcept { return m_restart; }
  bool new_pass_pending() const noexcept
  {
    if(m_retake)
      return true;
    for(int i = 0; i < m_queued; i++)
      if(m_queue[i].new_layer)
        return true;
    return false;
  }

  //! The looper made a loop out of what it heard before, without recording:
  //! it plays it from `position`, and Play is what is asked for.
  void play_captured(int64_t position) noexcept
  {
    actualMode = LoopMode::Play;
    quantizedPlayMode = LoopMode::Play;
    wanted = LoopMode::Play;
    m_queued = 0;
    m_into_overdub = false;
    m_retake = false;
    recordStartBar = -1.;
    recordEndBar = -1.;
    m_taking = LoopMode::Stop;
    playbackPos = position;
  }

  void reset() noexcept
  {
    quantizedPlayMode = LoopMode::Stop;
    actualMode = LoopMode::Stop;
    playbackPos = 0;
    recordStartBar = -1.;
    recordEndBar = -1.;
    endedByPostaction = false;
    m_taking = LoopMode::Stop;
    m_rephase = false;
    m_restart = false;
    m_retake = false;
    m_rerecord = false;
    m_queued = 0;
    m_point_requested = false;
    m_last_date.reset();
  }

  template <typename Engine>
  void operator()(
      const ossia::token_request& tk, ossia::exec_state_facade st,
      const transport_settings& s, Engine& e)
  {
    using namespace ossia;

    m_facade = st;
    m_tick = tk;
    const auto timings = st.timings(tk);
    this->tickStartSample = timings.start_sample;
    this->postaction_bars = s.postaction_bars;
    this->sampleRate = st.sampleRate();
    this_buffer_quantif_time = std::nullopt;

    const LoopMode m = s.mode;
    const bool on_grid = !no_grid(s.quantif) && tk.prev_date != 0_tv;
    const loop_period period = e.looper_period(tk);
    count_rounds(period);

    // The timeline jumped, e.g. the score was moved to another date: a loop
    // in phase with it goes there as well.
    if(s.start_from == StartFrom::InPhase && m_last_date && tk.prev_date != *m_last_date
       && actualMode == LoopMode::Play && m_taking == LoopMode::Stop)
      m_rephase = true;
    m_last_date = tk.date;

    if(on_grid)
    {
      this->quantif = s.quantif;
      // A point on the tick's very first sample counts. Bar lines land there
      // whenever the bar divides evenly into the buffer -- which at 120bpm and
      // 48kHz is every buffer of 256 frames or fewer -- and refusing them left
      // the mode waiting for a point that would never come.
      if(const auto pt = grid_point(tk, s, period, timings.length))
      {
        this_buffer_quantif_time = date_at(tk, *pt, timings.length);
        m_point_this_tick = frame + *pt;
      }
    }
    else
    {
      this->quantif = {};
    }

    if(m != quantizedPlayMode || m_restart || m_retake || m_point_requested)
    {
      const bool stopping = m == LoopMode::Stop && quantizedPlayMode != LoopMode::Stop
                            && !m_restart && !m_retake && !m_point_requested;
      const int64_t to_end = stopping && s.stop_at == StopAt::LoopEnd
                                 ? e.looper_frames_to_end()
                                 : int64_t(-1);
      if(late_extension_stop(s, e))
      {
        take_up(tk, s, e, m_late_frames);
        preAction(tk, s, e);
      }
      else if(to_end >= 0)
      {
        if(to_end == 0)
        {
          take_up(tk, s, e);
          preAction(tk, s, e);
        }
        else if(to_end < timings.length)
        {
          const auto at = tk.prev_date
                          + ossia::time_value{int64_t(
                              std::round(double(to_end) * st.samplesToModel()))};
          auto sub_tk = tk;
          sub_tk.set_end_time(at);
          preAction(sub_tk, s, e);
          sub_tk = tk;
          sub_tk.set_start_time(at);
          take_up(sub_tk, s, e);
          preAction(sub_tk, s, e);
        }
        else
        {
          preAction(tk, s, e);
        }
      }
      else if(on_grid && !(stopping && s.stop_at == StopAt::Now))
      {
        if(late_stop(s, e))
        {
          m_closing_late = true;
          take_up(tk, s, e, m_late_frames);
          m_closing_late = false;
          preAction(tk, s, e);
        }
        else if(auto& time = this_buffer_quantif_time)
        {
          // tempo = 60 -> 1 quarter = 1 second
          // tempo = 100 -> 1 quarter = 1 * 60/100 = 0.6 second
          // tempo = 100 -> 1 bar = 2 second
          // tempo = 120 -> 1 bar = 2 second
          if(*time > tk.prev_date)
          {
            // Finish what we were doing until the quantization date. The span
            // is half-open, so the two halves tile the tick exactly; ending a
            // flick early instead rounds down to a sample nobody writes.
            {
              auto sub_tk = tk;
              sub_tk.set_end_time(*time);
              preAction(sub_tk, s, e);
            }

            // We can switch to the new mode, for the remaining of the tick
            {
              auto sub_tk = tk;
              sub_tk.set_start_time(*time);
              take_up(sub_tk, s, e);
              preAction(sub_tk, s, e);
            }
          }
          else
          {
            // We can switch to the new mode
            take_up(tk, s, e);
            preAction(tk, s, e);
          }
        }
        else
        {
          // We cannot switch yet
          preAction(tk, s, e);
        }
      }
      else
      {
        // No quantization, we can switch to the new mode
        take_up(tk, s, e);
        preAction(tk, s, e);
      }
    }
    else
    {
      // No change
      preAction(tk, s, e);
    }

    if(m_point_this_tick)
    {
      lastPointFrame = m_point_this_tick;
      m_point_this_tick.reset();
    }
    frame += timings.length;

    // What was asked for is taken up: the change queued after it waits for
    // the next grid point.
    if(m_queued > 0 && wanted == quantizedPlayMode && !m_retake)
    {
      const step next = m_queue[0];
      std::move(m_queue.begin() + 1, m_queue.begin() + m_queued, m_queue.begin());
      m_queued--;
      take_step(next);
    }
  }

private:
  //! A change asked for by Rec / Play / Dub or Play / Stop: a mode, and for a
  //! new layer, a new pass in it.
  struct step
  {
    LoopMode mode;
    bool new_layer;
  };
  static constexpr int max_queued = 4;
  std::array<step, max_queued> m_queue{};
  int m_queued{};

  //! Takes the step up, or queues it after what still waits for the grid.
  void push(step st) noexcept
  {
    const bool pending = wanted != quantizedPlayMode || m_retake || m_queued > 0;
    if(!pending)
      take_step(st);
    else if(m_queued < max_queued)
      m_queue[m_queued++] = st;
  }
  void take_step(step st) noexcept
  {
    wanted = st.mode;
    if(st.new_layer)
      m_retake = true;
  }

  //! The rounds the loop has gone since it was made, for the multiples of
  //! Loop above one: "2x loop" is every other restart.
  int64_t m_rounds{};
  int64_t m_last_round_position{-1};

  void count_rounds(const loop_period& p) noexcept
  {
    if(p.length <= 0 || !p.going_round)
    {
      m_rounds = 0;
      m_last_round_position = -1;
      return;
    }
    // Back past half the loop is a restart: the end reached, or Retrigger.
    if(m_last_round_position >= 0 && p.position + p.length / 2 < m_last_round_position)
      m_rounds++;
    m_last_round_position = p.position;
  }

  //! Where the next point of Quantif's grid falls in the span of `tk`, as an
  //! offset from its first frame. Loop is the loop's own restart while there
  //! is a loop; before there is one, it is the length the recording will have:
  //! Bars bars, or one bar when Bars is zero.
  std::optional<int> grid_point(
      const ossia::token_request& tk, const transport_settings& s,
      const loop_period& period, int64_t frames) const noexcept
  {
    halp::tick_musical t{};
    t.frames = int(frames);
    t.tempo = tk.tempo;
    t.signature.num = tk.signature.upper;
    t.signature.denom = tk.signature.lower;
    t.position_in_frames = frame;
    t.start_position_in_quarters = tk.musical_start_position;
    t.end_position_in_quarters = tk.musical_end_position;
    t.last_signature_change = tk.musical_start_last_signature;
    t.bar_at_start = tk.musical_start_last_bar;
    t.bar_at_end = tk.musical_end_last_bar;

    halp::grid_value g = s.quantif;
    halp::grid_period p{};
    if(period.length > 0)
    {
      p.length = period.length;
      p.position = period.going_round ? m_rounds * period.length + period.position
                                      : period.position;
    }
    else if(g.unit == halp::grid_unit::period)
    {
      g = halp::grid_value::bars(g.value * std::max(1, s.postaction_bars));
    }
    return halp::next_grid_point(g, t, sampleRate, p);
  }

  //! The date of frame `f` of the span of `tk`, such that splitting the tick
  //! there puts the split on that frame.
  static ossia::time_value
  date_at(const ossia::token_request& tk, int64_t f, int64_t frames) noexcept
  {
    if(f <= 0 || frames <= 0)
      return tk.prev_date;
    const double span = double((tk.date - tk.prev_date).impl);
    return tk.prev_date
           + ossia::time_value{int64_t(std::llround(span * double(f) / double(frames)))};
  }

  LoopMode m_taking{LoopMode::Stop};
  //! The recording asked for, or going on, was asked for as an overdub of an
  //! empty loop: it gives way to overdubbing.
  bool m_into_overdub{};
  //! Undo or Clear dropped the recording: the next span starts it again.
  bool m_rerecord{};
  //! Waiting for the next grid point: back to the loop's start, a new pass,
  //! something the looper asked for.
  bool m_restart{};
  bool m_retake{};
  bool m_point_requested{};
  bool m_closing_late{};
  bool m_forced_close{};
  //! The loop is heard again: the next span asks the looper where it is.
  bool m_rephase{};
  std::optional<ossia::time_value> m_last_date;
  ossia::token_request m_tick{};
  int64_t m_late_frames{};
  std::optional<int64_t> m_point_this_tick;

  //! A recording asked to end just after a grid point ends on that point: a
  //! performer pressing a footswitch on the downbeat is a little late, and
  //! waiting for the next point makes the loop a whole grid step too long.
  template <typename Engine>
  bool late_stop(const transport_settings& s, Engine& e)
  {
    if(actualMode != LoopMode::Record || m_taking != LoopMode::Record)
      return false;
    if(s.mode == LoopMode::Record || s.late_seconds <= 0.)
      return false;
    // A point on this tick's first sample is not late: switch on it.
    if(this_buffer_quantif_time && m_point_this_tick && *m_point_this_tick == frame)
      return false;
    if(!lastPointFrame)
      return false;

    const int64_t late = frame - *lastPointFrame;
    if(late <= 0 || late > int64_t(s.late_seconds * sampleRate))
      return false;

    e.looper_trim_recording(late);
    m_late_frames = late;
    return true;
  }

  //! An overdub that has just made the loop longer, asked to stop a little
  //! after the end it grew from, ends on that end: the performer meant to stop
  //! there. Checked before the mode is taken up, whether or not the change
  //! waits for the grid, while the playhead is still where the overdub left it.
  //! A stop on the very frame the loop grew is on time, not late: it ends on
  //! the old end with no late window at all.
  template <typename Engine>
  bool late_extension_stop(const transport_settings& s, Engine& e)
  {
    if(actualMode != LoopMode::Overdub || m_taking != LoopMode::Overdub)
      return false;
    // Only the end of the pass is late: a restart carries it on.
    if(s.mode == quantizedPlayMode && !m_retake)
      return false;
    const int64_t into = e.looper_trim_extension(
        std::max<int64_t>(0, int64_t(s.late_seconds * sampleRate)));
    if(into < 0)
      return false;
    m_late_frames = into;
    return true;
  }

  //! The transport ended a recording itself: what it took up is what is
  //! asked for now, unless something else was asked for in the meantime.
  void recording_handed_over() noexcept
  {
    quantizedPlayMode = actualMode;
    m_into_overdub = false;
    if(wanted == LoopMode::Record)
      wanted = actualMode;
  }

  //! Taking up a mode. A recording always starts its buffer again -- there is
  //! nothing yet to carry on from. Whether playing and overdubbing do is what
  //! the Start from setting is for: reading the loop from the top on every change
  //! jumps, which is heard when overdubbing hands over to playing.
  void enterMode(LoopMode m, StartFrom from)
  {
    const LoopMode was = actualMode;
    quantizedPlayMode = m;
    actualMode = m;
    if(m != LoopMode::Record)
      m_into_overdub = false;
    if(m == LoopMode::Record || from == StartFrom::LoopStart)
      playbackPos = 0;
    else if(
        from == StartFrom::InPhase && was == LoopMode::Stop
        && (m == LoopMode::Play || m == LoopMode::Overdub))
      m_rephase = true;
  }

  //! Takes up, on the first sample of the span of `tk`, what waits for it: the
  //! mode asked for, a restart, a new pass, what the looper asked for. A late
  //! stop puts the playhead `position` frames into the loop.
  template <typename Engine>
  void take_up(
      const ossia::token_request& tk, const transport_settings& s, Engine& e,
      std::optional<int64_t> position = {})
  {
    if(std::exchange(m_point_requested, false))
      e.looper_point();
    const bool change = s.mode != quantizedPlayMode;
    const bool retake = std::exchange(m_retake, false);
    const bool restart = std::exchange(m_restart, false);
    if(change)
      enterMode(s.mode, s.start_from);
    // A recording that starts again drops what it had: nothing to close.
    const bool rerecord = restart && !change && actualMode == LoopMode::Record;
    if(position)
    {
      playbackPos = *position;
    }
    else if(restart)
    {
      playbackPos = 0;
      m_rephase = false;
      if(rerecord)
        m_taking = LoopMode::Stop;
    }
    // A restart alone carries the pass on from the loop's start.
    if(change || retake || rerecord)
      changeAction(tk, e);
  }

  template <typename Engine>
  void close(const ossia::token_request& tk, Engine& e)
  {
    // Every mode change goes through here; the take is only closed once for
    // material that was recorded since the last one.
    const LoopMode was = m_taking;
    if(was == LoopMode::Stop)
      return;
    m_taking = LoopMode::Stop;
    e.looper_close_take(tk, was, endedByPostaction);
  }

  //! A recording starts at the start of the span of `tk`: its bars, its
  //! date and the quarters its tempo is worked out from count from there.
  void begin_recording_window(const ossia::token_request& tk) noexcept
  {
    // Zero bars means no automatic end: the end bar would otherwise be the
    // start bar and preAction() would leave Record on its first tick.
    if(postaction_bars > 0)
    {
      recordStartBar = tk.musical_start_position;
      recordEndBar = tk.musical_start_position
                     + (4. * double(tk.signature.upper) / tk.signature.lower)
                           * postaction_bars;
    }
    else
    {
      recordStartBar = -1.;
      recordEndBar = -1.;
    }
    endedByPostaction = false;
    recordStartDate = tk.prev_date;
    recordStartPosition = tk.musical_start_position;
  }

  template <typename Engine>
  void changeAction(const ossia::token_request& tk, Engine& e)
  {
    if(quantizedPlayMode == LoopMode::Record)
    {
      begin_recording_window(tk);
    }
    else
    {
      recordStartBar = -1.;
      recordEndBar = -1.;
    }
    close(tk, e);
  }

  template <typename Engine>
  void preAction(ossia::token_request tk, const transport_settings& s, Engine& e)
  {
    using namespace ossia;
    if(recordStartBar == -1.)
    {
      action(tk, e);
    }
    else
    {
      // If we are in the token request which steps into the bar in which
      // we change because we are e.g. 4 bars after the recording started
      auto switch_to_main_mode = [&] {
        actualMode = end_of_recording(s.after_record);
        recording_handed_over();

        recordStartBar = -1.;
        endedByPostaction = true;
        if(s.start_from == StartFrom::LoopStart)
          playbackPos = 0;
      };

      // Change of bar at the first sample
      if(tk.musical_start_last_bar >= recordEndBar)
      {
        switch_to_main_mode();
        close(tk, e);
        action(tk, e);
      }

      // Change of bar in the middle
      else if(tk.musical_end_last_bar >= recordEndBar)
      {
        const auto quant_date = tk.get_quantification_date(1.0);
        if(quant_date && *quant_date > tk.prev_date && *quant_date < tk.date)
        {
          const ossia::time_value t = *quant_date;

          // Finish what we were doing until the quantization date
          {
            auto sub_tk = tk;
            sub_tk.set_end_time(t);
            action(sub_tk, e);
          }

          // We can switch to the new mode. The take closes on the bar line,
          // which is where the loop starts again.
          auto sub_tk = tk;
          sub_tk.set_start_time(t);
          switch_to_main_mode();
          close(sub_tk, e);

          // Remaining of the tick
          action(sub_tk, e);
        }
        else if(tk.musical_end_position - tk.musical_end_last_bar < 1e-9)
        {
          // The bar line is this tick's own end, so the whole of it still
          // belongs to what came before -- and a point there is not reported,
          // the tick's end being exclusive. The next tick starts on the line
          // and takes the switch; ending here instead drops this tick's
          // recording, which comes back as a buffer of silence in the loop.
          action(tk, e);
        }
        else
        {
          // The bar line is not locatable inside the tick: switch on its first
          // sample rather than leaving the buffer unwritten.
          switch_to_main_mode();
          close(tk, e);
          action(tk, e);
        }
      }

      // No change of bar yet, we continue
      else
      {
        action(tk, e);
      }
    }
  }

  template <typename Engine>
  void action(const ossia::token_request& tk, Engine& e)
  {
    const auto [start, length] = m_facade.timings(tk);
    const int64_t first = start - tickStartSample;
    if(std::exchange(m_rephase, false))
      e.looper_rephase(tk);
    if(std::exchange(m_rerecord, false) && actualMode == LoopMode::Record)
      begin_recording_window(tk);
    const int64_t done = e.looper_run(first, length);
    // The engine ended the recording itself (its buffer is full): the take is
    // closed, then the rest of the span is After record's.
    if(m_forced_close)
    {
      m_forced_close = false;
      // The loop starts again where the buffer filled up.
      m_close_offset = done;
      close(tk, e);
      m_close_offset = 0;
      if(done < length)
        e.looper_run(first + done, length - done);
    }
  }

  ossia::exec_state_facade m_facade{};
  int64_t m_close_offset{};
};

//! Where the timeline is at the start of a tick, before the transport has run
//! it.
inline transport::timeline_point
timeline_start(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept
{
  return {double(tk.prev_date.impl) * st.modelToSamples(), tk.musical_start_position};
}
}
