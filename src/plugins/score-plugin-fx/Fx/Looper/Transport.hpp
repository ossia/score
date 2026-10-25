#pragma once
#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/token_request.hpp>

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
 * recording can end on its own after a number of bars (the post-action).
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
 *   // Restart::InPhase: the loop starts being heard again, at the start of
 *   // the span of `tk`; the looper puts the playhead where the loop would be
 *   // had it kept playing since it was made, by the timeline's clock.
 *   void looper_rephase(const ossia::token_request& tk);
 *
 * The looper calls transport.taking(mode) whenever it records or overdubs.
 */
namespace Nodes::Looper
{
enum class LoopMode
{
  Play,
  Record,
  Overdub,
  Stop
};

enum class Postaction
{
  Play,
  Overdub
};

//! Where the loop is read from when the mode changes.
//!
//! Order matters: the first is what the object did before there was a choice.
enum class Restart
{
  Always,    ///< Every change of mode reads the loop from its beginning again.
  Recording, ///< Only a recording does; playing and overdubbing carry straight on.
  InPhase    ///< As Recording, and a loop heard again after a stop, a transport
             ///< start or a jump of the timeline is where it would be had it
             ///< kept playing.
};

//! What an overdub that goes round the loop several times leaves.
enum class Passes
{
  Whole,   ///< One pass, from when the overdub starts to when it stops.
  PerCycle ///< A pass for each time it goes round, starting on the loop's start.
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

struct transport_settings
{
  LoopMode mode{LoopMode::Stop};
  float quantif{};
  Postaction postaction{};
  int postaction_bars{};
  Restart restart{};
  //! A recording asked to stop at most this long after a grid point ends on
  //! that point instead of waiting for the next one.
  double late_seconds{};
};

struct transport
{
  LoopMode quantizedPlayMode{LoopMode::Stop};
  LoopMode actualMode{LoopMode::Stop};
  int64_t playbackPos{};
  ossia::quarter_note recordStartBar{-1.};
  ossia::quarter_note recordEndBar{-1.};
  float quantif{0.0};
  std::optional<ossia::time_value> this_buffer_quantif_time;
  int64_t tickStartSample{};
  int postaction_bars{};
  double sampleRate{48000.};
  //! Whether the post-action is what ended the recording, rather than the
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

  //! A recording reaching the end of its buffer gives way to the post-action
  //! as if its bars had run out.
  void end_recording(Postaction postaction) noexcept
  {
    recordStartBar = -1.;
    recordEndBar = -1.;
    endedByPostaction = false;
    actualMode = postaction == Postaction::Overdub ? LoopMode::Overdub : LoopMode::Play;
    overdub_after_recording();
    m_forced_close = true;
  }

  //! The mode to take up for the one the looper is asked for. Overdubbing an
  //! empty loop records it: the first pass has nothing to go over, and a
  //! performer who starts with Overdub expects to be heard. The recording
  //! then goes on until the mode changes, or the post-action takes over after
  //! its bars, after which overdubbing proper starts at once, on the sample
  //! the recording ended on.
  LoopMode effective(LoopMode wanted, bool empty) noexcept
  {
    if(wanted != LoopMode::Overdub)
      m_overdub_records = false;
    else if(empty && !is_taking())
      m_overdub_records = true;
    else if(m_overdub_records && actualMode != LoopMode::Record && !empty)
      m_overdub_records = false;
    return m_overdub_records ? LoopMode::Record : wanted;
  }

  //! The looper made a loop out of what it heard before, without recording:
  //! it plays it from `position` until the mode is changed.
  void play_captured(int64_t position) noexcept
  {
    actualMode = LoopMode::Play;
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
    const float quantif = s.quantif;

    // The timeline jumped, e.g. the score was moved to another date: a loop
    // in phase with it goes there as well.
    if(s.restart == Restart::InPhase && m_last_date && tk.prev_date != *m_last_date
       && actualMode == LoopMode::Play && m_taking == LoopMode::Stop)
      m_rephase = true;
    m_last_date = tk.date;

    if(quantif != 0 && tk.prev_date != 0_tv)
    {
      this->quantif = quantif;
      // A point on the tick's very first sample counts. Bar lines land there
      // whenever the bar divides evenly into the buffer -- which at 120bpm and
      // 48kHz is every buffer of 256 frames or fewer -- and refusing them left
      // the mode waiting for a point that would never come.
      if(auto pt = tk.get_quantification_point(1. / quantif);
         pt && pt->date >= tk.prev_date)
      {
        this_buffer_quantif_time = pt->date;

        // Through the point's musical position, not its date: the date is
        // truncated to a whole flick, and flooring that into a sample rounds
        // twice, so the loop point would land a sample before the metronome
        // click on the very bar line it is quantized to.
        const double ratio = st.modelToSamples();
        const int64_t sample
            = tk.physical_start(ratio) + tk.physical_position(pt->position, ratio);
        m_point_this_tick = frame + (sample - tickStartSample);
      }
    }
    else
    {
      this->quantif = 0.0f;
    }

    if(m != quantizedPlayMode)
    {
      if(late_extension_stop(s, e))
      {
        enterMode(m, s.restart);
        playbackPos = m_late_frames;
        changeAction(tk, e);
        preAction(tk, s, e);
      }
      else if(quantif != 0 && tk.prev_date != 0_tv)
      {
        if(late_stop(s, e))
        {
          enterMode(m, s.restart);
          playbackPos = m_late_frames;
          m_closing_late = true;
          changeAction(tk, e);
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

            // We can switch to the new mode
            enterMode(m, s.restart);

            // Remaining of the tick
            {
              auto sub_tk = tk;
              sub_tk.set_start_time(*time);
              changeAction(sub_tk, e);
              preAction(sub_tk, s, e);
            }
          }
          else
          {
            // We can switch to the new mode
            enterMode(m, s.restart);
            changeAction(tk, e);
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
        enterMode(m, s.restart);
        changeAction(tk, e);
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
  }

private:
  LoopMode m_taking{LoopMode::Stop};
  bool m_overdub_records{};
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
    if(s.mode == LoopMode::Overdub)
      return false;
    const int64_t into = e.looper_trim_extension(
        std::max<int64_t>(0, int64_t(s.late_seconds * sampleRate)));
    if(into < 0)
      return false;
    m_late_frames = into;
    return true;
  }

  //! A recording that Overdub started on an empty loop gives way to
  //! overdubbing, whatever the post-action: Overdub is still what is asked
  //! for, and taking it up now rather than on the next grid point keeps the
  //! loop going round from where the recording closed it.
  void overdub_after_recording() noexcept
  {
    if(!m_overdub_records)
      return;
    m_overdub_records = false;
    actualMode = LoopMode::Overdub;
    quantizedPlayMode = LoopMode::Overdub;
  }

  //! Taking up a mode. A recording always starts its buffer again -- there is
  //! nothing yet to carry on from. Whether playing and overdubbing do is what
  //! the Restart setting is for: reading the loop from the top on every change
  //! jumps, which is heard when overdubbing hands over to playing.
  void enterMode(LoopMode m, Restart restart)
  {
    const LoopMode was = actualMode;
    quantizedPlayMode = m;
    actualMode = m;
    if(m == LoopMode::Record || restart == Restart::Always)
      playbackPos = 0;
    else if(
        restart == Restart::InPhase && was == LoopMode::Stop
        && (m == LoopMode::Play || m == LoopMode::Overdub))
      m_rephase = true;
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

  template <typename Engine>
  void changeAction(const ossia::token_request& tk, Engine& e)
  {
    // Zero bars means no automatic post-action: the end bar would otherwise be
    // the start bar and preAction() would leave Record on its first tick.
    if(quantizedPlayMode == LoopMode::Record && postaction_bars > 0)
    {
      recordStartBar = tk.musical_start_position;
      recordEndBar = tk.musical_start_position
                     + (4. * double(tk.signature.upper) / tk.signature.lower)
                           * postaction_bars;
      endedByPostaction = false;
    }
    else
    {
      recordStartBar = -1.;
      recordEndBar = -1.;
    }

    if(quantizedPlayMode == LoopMode::Record)
    {
      recordStartDate = tk.prev_date;
      recordStartPosition = tk.musical_start_position;
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
        switch(s.postaction)
        {
          case Postaction::Play:
            actualMode = LoopMode::Play;
            break;
          case Postaction::Overdub:
            actualMode = LoopMode::Overdub;
            break;
          default:
            actualMode = LoopMode::Stop;
            break;
        }
        overdub_after_recording();

        recordStartBar = -1.;
        endedByPostaction = true;
        if(s.restart == Restart::Always)
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
    const int64_t done = e.looper_run(first, length);
    // The engine ended the recording itself (its buffer is full): the take is
    // closed, then the rest of the span is the post-action's.
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
