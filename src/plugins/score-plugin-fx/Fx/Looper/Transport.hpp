#pragma once
#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/token_request.hpp>

#include <cmath>
#include <cstdint>
#include <optional>

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
  Always,   ///< Every change of mode reads the loop from its beginning again.
  Recording ///< Only a recording does; playing and overdubbing carry straight on.
};

// Order matters: this replaced a toggle, and a stored 0/1 has to keep
// meaning what it did.
enum class Passthrough
{
  None,   ///< The input is never heard, in any mode.
  Record, ///< Heard while recording and overdubbing; playback is the loop alone.
  Full    ///< Always heard, mixed over the loop during playback.
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
    return m_facade.samplesSinceStart() + (m_facade.timings(tk).start_sample - tickStartSample);
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
    m_forced_close = true;
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
  }

  template <typename Engine>
  void operator()(
      const ossia::token_request& tk, ossia::exec_state_facade st,
      const transport_settings& s, Engine& e)
  {
    using namespace ossia;

    m_facade = st;
    const auto timings = st.timings(tk);
    this->tickStartSample = timings.start_sample;
    this->postaction_bars = s.postaction_bars;
    this->sampleRate = st.sampleRate();
    this_buffer_quantif_time = std::nullopt;

    const LoopMode m = s.mode;
    const float quantif = s.quantif;

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
      if(quantif != 0 && tk.prev_date != 0_tv)
      {
        if(late_stop(s, e))
        {
          enterMode(m, s.restart);
          playbackPos = m_late_frames;
          changeAction(tk, e);
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
  bool m_forced_close{};
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

  //! Taking up a mode. A recording always starts its buffer again -- there is
  //! nothing yet to carry on from. Whether playing and overdubbing do is what
  //! the Restart setting is for: reading the loop from the top on every change
  //! jumps, which is heard when overdubbing hands over to playing.
  void enterMode(LoopMode m, Restart restart)
  {
    quantizedPlayMode = m;
    actualMode = m;
    if(m == LoopMode::Record || restart == Restart::Always)
      playbackPos = 0;
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

          // We can switch to the new mode
          switch_to_main_mode();
          close(tk, e);

          // Remaining of the tick
          {
            auto sub_tk = tk;
            sub_tk.set_start_time(t);
            action(sub_tk, e);
          }
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
    const int64_t done = e.looper_run(first, length);
    // The engine ended the recording itself (its buffer is full): the take is
    // closed, then the rest of the span is the post-action's.
    if(m_forced_close)
    {
      m_forced_close = false;
      close(tk, e);
      if(done < length)
        e.looper_run(first + done, length - done);
    }
  }

  ossia::exec_state_facade m_facade{};
};
}
