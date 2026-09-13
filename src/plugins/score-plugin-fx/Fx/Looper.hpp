#pragma once
#include <Fx/Types.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/nodes/media.hpp>
#include <ossia/dataflow/token_request.hpp>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>

namespace Nodes::AudioLooper
{
struct Node
{
  halp_meta(name, "Looper (audio)")
  halp_meta(c_name, "Looper (audio)")
  halp_meta(category, "Audio/Utilities")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/audio-looper.html")
  halp_meta(description, "Loop audio")
  halp_meta(uuid, "a0ad4227-ac3d-448b-a19b-19581ed4e2c6");
  halp_meta(recommended_height, 65);

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

  struct ins
  {
    halp::dynamic_audio_bus<"in", double> audio;
    halp::enum_t<LoopMode, "Loop"> mode;
    quant_selector<"Quantif"> quantif;
    halp::toggle<"Passthrough", halp::toggle_setup{.init = true}> passthrough;
    halp::enum_t<Postaction, "Post-action"> postaction;
    halp::spinbox_i32<"Bars", halp::irange{0, 64, 4}> postaction_bars;
  } inputs;
  struct
  {
    halp::mimic_audio_bus<"out", &ins::audio> audio;
  } outputs;

  struct State
  {
    LoopMode quantizedPlayMode{LoopMode::Stop};
    LoopMode actualMode{LoopMode::Stop};
    ossia::audio_vector audio;
    int64_t playbackPos{};
    ossia::time_value recordStart{};
    ossia::quarter_note recordStartBar{-1.};
    ossia::quarter_note recordEndBar{-1.};
    int actualChannels = 0;
    float quantif{0.0};
    std::optional<ossia::time_value> this_buffer_quantif_time;
    std::optional<int64_t> this_buffer_quantif_sample;
    int64_t tickStartSample{};
    int postaction_bars{};
    double sampleRate{48000.};
    bool isPostRecording{false};
    bool faded{false};

    static constexpr int64_t default_buffer_size = 192000 * 32;
    void reset_elapsed() { }
    int channels() const noexcept { return actualChannels; }
    void set_channels(int chans)
    {
      if(chans == actualChannels)
        return;

      const int prev_channels = actualChannels;
      actualChannels = chans;
      if(std::ssize(audio) < chans)
        audio.resize(chans);

      // Channels that come back after the input narrowed must line up with the
      // ones that stayed: play() and overdub() index them all with one position.
      const int64_t len = prev_channels > 0 ? std::ssize(audio[0]) : 0;
      for(int i = prev_channels; i < chans; i++)
      {
        if(std::ssize(audio[i]) != len)
        {
          audio[i].reserve(std::max(int64_t(audio[i].capacity()), default_buffer_size));
          audio[i].resize(len);
        }
      }
    }

    State()
    {
      audio.resize(2);
      for(auto& vec : audio)
        vec.reserve(default_buffer_size);
    }
  } state;

  void fade(const ossia::token_request& tk)
  {
    // Every mode change goes through here; the ramp must only ever be applied
    // to material that was recorded since the last one.
    if(state.faded)
      return;
    state.faded = true;

    const double sr = state.sampleRate;
    const double bar_samples
        = sr * 4. * (double(tk.signature.upper) / tk.signature.lower) * (60. / tk.tempo);
    const double total_samples = std::floor(state.postaction_bars * bar_samples);

    // If there are more samples than expected we crop
    const bool quantify_length = (state.quantif > 0.f) && (state.postaction_bars > 0)
                                 && (state.channels() > 0);
    if(quantify_length)
    {
      if(total_samples < state.audio[0].size())
      {
        for(auto& a : state.audio)
          a.resize(total_samples);
      }
    }

    // Apply a small fade on the first and last samples
    for(auto& chan : state.audio)
    {
      const int64_t samples = std::ssize(chan);
      if(int64_t min_n = std::min(samples, int64_t(128)); min_n > 0)
      {
        float f = 1. / min_n;
        float ff = 0.;
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

    // If there are less samples than expected we extend
    if(quantify_length)
    {
      if(total_samples > state.audio[0].size())
      {
        for(auto& a : state.audio)
          a.resize(total_samples);
      }
    }
  }

  void changeAction(const ossia::token_request& tk)
  {
    // Zero bars means no automatic post-action: the end bar would otherwise be
    // the start bar and preAction() would leave Record on its first tick.
    if(state.quantizedPlayMode == LoopMode::Record && state.postaction_bars > 0)
    {
      state.recordStart = tk.prev_date;
      state.recordStartBar = tk.musical_start_position;
      state.recordEndBar = tk.musical_start_position
                           + (4. * double(tk.signature.upper) / tk.signature.lower)
                                 * state.postaction_bars;
      state.reset_elapsed();
    }
    else
    {
      state.recordStart = ossia::time_value{-1LL};
      state.recordStartBar = -1.;
      state.recordEndBar = -1.;
      state.reset_elapsed();
    }

    fade(tk);
  }

  void checkPostAction(
      const std::string& postaction, int postaction_bars, const ossia::token_request& tk,
      State& st)
  {
  }

  ossia::exec_state_facade ossia_state;

  using tick = ossia::token_request;
  void operator()(const tick& tk)
  {
    using namespace ossia;

    if(tk.date == tk.prev_date)
      return;

    const LoopMode m = this->inputs.mode;
    const int postaction_bars = this->inputs.postaction_bars;
    const Postaction postaction = this->inputs.postaction;
    const float quantif = this->inputs.quantif.value;
    const bool passthrough = this->inputs.passthrough;

    state.this_buffer_quantif_time = std::nullopt;
    state.this_buffer_quantif_sample = std::nullopt;
    state.postaction_bars = postaction_bars;
    state.sampleRate = ossia_state.sampleRate();

    // The sample pointers we were handed already start at this tick's first
    // sample; every span below is expressed relative to that.
    state.tickStartSample = ossia_state.timings(tk).start_sample;

    if(quantif != 0 && tk.prev_date != 0_tv)
    {
      state.quantif = quantif;
      if(auto pt = tk.get_quantification_point(1. / quantif);
         pt && pt->date > tk.prev_date)
      {
        const double ratio = ossia_state.modelToSamples();
        state.this_buffer_quantif_time = pt->date;
        // Through the point's musical position, not its date: the date is
        // truncated to a whole flick, and flooring that into a sample rounds
        // twice, so the loop point would land a sample before the metronome
        // click on the very bar line it is quantized to.
        state.this_buffer_quantif_sample
            = tk.physical_start(ratio) + tk.physical_position(pt->position, ratio);
      }
    }
    else
    {
      state.quantif = 0.0f;
    }

    if(m != state.quantizedPlayMode)
    {
      if(quantif != 0 && tk.prev_date != 0_tv)
      {
        if(auto& time = state.this_buffer_quantif_time)
        {
          // tempo = 60 -> 1 quarter = 1 second
          // tempo = 100 -> 1 quarter = 1 * 60/100 = 0.6 second
          // tempo = 100 -> 1 bar = 2 second
          // tempo = 120 -> 1 bar = 2 second
          // tempo = 120 -> 1 bar = 2 second
          if(*time > tk.prev_date)
          {
            // Finish what we were doing until the quantization date. The span
            // is half-open, so the two halves tile the tick exactly; ending a
            // flick early instead rounds down to a sample nobody writes.
            {
              auto sub_tk = tk;
              sub_tk.set_end_time(*time);

              preAction(sub_tk, postaction, postaction_bars, passthrough);
            }

            // We can switch to the new mode
            state.quantizedPlayMode = m;
            state.actualMode = m;
            state.playbackPos = 0;

            // Remaining of the tick
            {
              auto sub_tk = tk;
              sub_tk.set_start_time(*time);

              changeAction(sub_tk);
              preAction(sub_tk, postaction, postaction_bars, passthrough);
            }
          }
          else
          {
            // We can switch to the new mode
            state.quantizedPlayMode = m;
            state.actualMode = m;
            state.playbackPos = 0;

            changeAction(tk);
            preAction(tk, postaction, postaction_bars, passthrough);
          }
        }
        else
        {
          // We cannot switch yet
          preAction(tk, postaction, postaction_bars, passthrough);
        }
      }
      else
      {
        // No quantization, we can switch to the new mode
        state.quantizedPlayMode = m;
        state.actualMode = m;
        state.playbackPos = 0;

        changeAction(tk);
        preAction(tk, postaction, postaction_bars, passthrough);
      }
    }
    else
    {
      // No change
      preAction(tk, postaction, postaction_bars, passthrough);
    }
  }

  void preAction(
      ossia::token_request tk, Postaction postaction, int postaction_bars,
      bool passthrough)
  {
    using namespace ossia;
    if(state.recordStartBar == -1.)
    {
      action(tk, passthrough);
    }
    else
    {
      // If we are in the token request which steps into the bar in which
      // we change because we are e.g. 4 bars after the recording started
      auto switch_to_main_mode = [&] {
        switch(postaction)
        {
          case Postaction::Play:
            state.actualMode = LoopMode::Play;
            break;
          case Postaction::Overdub:
            state.actualMode = LoopMode::Overdub;
            break;
          default:
            state.actualMode = LoopMode::Stop;
            break;
        }

        state.recordStart = ossia::time_value{-1};
        state.recordStartBar = -1.;
        state.reset_elapsed();
        state.playbackPos = 0;
      };

      // Change of bar at the first sample
      if(tk.musical_start_last_bar >= state.recordEndBar)
      {
        switch_to_main_mode();
        fade(tk);
        action(tk, passthrough);
      }

      // Change of bar in the middle
      else if(tk.musical_end_last_bar >= state.recordEndBar)
      {
        const auto quant_date = tk.get_quantification_date(1.0);
        if(quant_date && *quant_date > tk.prev_date && *quant_date < tk.date)
        {
          const ossia::time_value t = *quant_date;

          // Finish what we were doing until the quantization date
          {
            auto sub_tk = tk;
            sub_tk.set_end_time(t);
            action(sub_tk, passthrough);
          }

          // We can switch to the new mode
          switch_to_main_mode();
          fade(tk);

          // Remaining of the tick
          {
            auto sub_tk = tk;
            sub_tk.set_start_time(t);

            action(sub_tk, passthrough);
          }
        }
        else
        {
          // The bar line is not locatable inside the tick: switch on its first
          // sample rather than leaving the buffer unwritten.
          switch_to_main_mode();
          fade(tk);
          action(tk, passthrough);
        }
      }

      // No change of bar yet, we continue
      else
      {
        action(tk, passthrough);
      }
    }
  }

  void action(const ossia::token_request& tk, bool echoRecord)
  {
    auto timings = ossia_state.timings(tk);
    action(timings.start_sample - state.tickStartSample, timings.length, echoRecord);
  }

  void action(int64_t start, int64_t length, bool echoRecord)
  {
    switch(state.actualMode)
    {
      case LoopMode::Play:
        if(state.channels() == 0 || state.audio[0].size() == 0)
          stop(start, length);
        else
          play(start, length);
        break;
      case LoopMode::Stop:
        stop(start, length);
        break;
      case LoopMode::Record:
        echoRecord ? record(start, length) : record_noecho(start, length);
        break;
      case LoopMode::Overdub:
        echoRecord ? overdub(start, length) : overdub_noecho(start, length);
        break;
    }
  }

  void play(int64_t first_pos, int64_t samples)
  {
    auto& p2 = outputs.audio;
    const int64_t last = first_pos + samples;
    const int out_chans = p2.channels;
    const int loop_chans = std::min(state.channels(), out_chans);

    int64_t k = state.playbackPos;
    for(int i = 0; i < loop_chans; i++)
    {
      auto& out = p2.samples[i];
      auto& record = state.audio[i];
      const int64_t chan_samples = std::ssize(record);

      if(chan_samples <= 0)
      {
        for(int64_t j = first_pos; j < last; j++)
          out[j] = 0.;
        continue;
      }

      // The loop is free to be shorter than the tick, and to end anywhere
      // inside it.
      k = state.playbackPos % chan_samples;
      for(int64_t j = first_pos; j < last; j++)
      {
        out[j] = record[k];
        if(++k == chan_samples)
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

    state.playbackPos = k;
  }

  // We just copy input to output
  void stop(int64_t first_pos, int64_t samples)
  {
    auto& p1 = inputs.audio;
    auto& p2 = outputs.audio;
    const auto chans = p1.channels;
    const int64_t last = first_pos + samples;

    for(int i = 0; i < chans; i++)
    {
      auto& in = p1.samples[i];
      auto& out = p2.samples[i];

      for(int64_t j = first_pos; j < last; j++)
      {
        out[j] = in[j];
      }
    }
  }

  void record(int64_t first_pos, int64_t samples)
  {
    auto& p1 = inputs.audio;
    auto& p2 = outputs.audio;
    // Copy input to output, and append input to buffer
    const auto chans = p1.channels;
    state.set_channels(chans);
    const int64_t last = first_pos + samples;

    for(int i = 0; i < chans; i++)
    {
      auto& in = p1.samples[i];
      auto& out = p2.samples[i];
      auto& record = state.audio[i];

      record.resize(state.playbackPos + samples);
      int64_t k = state.playbackPos;

      for(int64_t j = first_pos; j < last; j++)
      {
        out[j] = in[j];
        record[k] = in[j];
        k++;
      }
    }
    state.playbackPos += samples;
    state.faded = false;
  }

  void record_noecho(int64_t first_pos, int64_t samples)
  {
    auto& p1 = inputs.audio;
    auto& p2 = outputs.audio;
    // Append input to buffer, and silence the output
    const auto chans = p1.channels;
    state.set_channels(chans);
    const int64_t last = first_pos + samples;

    for(int i = 0; i < chans; i++)
    {
      auto& in = p1.samples[i];
      auto& record = state.audio[i];

      record.resize(state.playbackPos + samples);
      int64_t k = state.playbackPos;

      for(int64_t j = first_pos; j < last; j++)
      {
        record[k] = in[j];
        k++;
      }
    }

    for(int i = 0; i < p2.channels; i++)
    {
      auto& out = p2.samples[i];
      for(int64_t j = first_pos; j < last; j++)
        out[j] = 0.;
    }

    state.playbackPos += samples;
    state.faded = false;
  }

  void overdub(int64_t first_pos, int64_t samples)
  {
    auto& p1 = inputs.audio;
    auto& p2 = outputs.audio;

    // Mix input into the buffer, and copy the result to the output
    const auto chans = p1.channels;
    state.set_channels(chans);
    const int64_t last = first_pos + samples;

    int64_t k = state.playbackPos;
    for(int i = 0; i < chans; i++)
    {
      auto& in = p1.samples[i];
      auto& out = p2.samples[i];
      auto& record = state.audio[i];
      const int64_t record_samples = std::ssize(record);

      // Nothing to layer onto: an overdub of an empty loop is an echo.
      if(record_samples <= 0)
      {
        for(int64_t j = first_pos; j < last; j++)
          out[j] = in[j];
        continue;
      }

      k = state.playbackPos % record_samples;
      for(int64_t j = first_pos; j < last; j++)
      {
        record[k] += in[j];
        out[j] = record[k];

        if(++k == record_samples)
          k = 0;
      }
    }
    state.playbackPos = k;
    state.faded = false;
  }

  void overdub_noecho(int64_t first_pos, int64_t samples)
  {
    auto& p1 = inputs.audio;
    auto& p2 = outputs.audio;

    // Mix input into the buffer, and copy what was there to the output
    const auto chans = p1.channels;
    state.set_channels(chans);
    const int64_t last = first_pos + samples;

    int64_t k = state.playbackPos;
    for(int i = 0; i < chans; i++)
    {
      auto& in = p1.samples[i];
      auto& out = p2.samples[i];
      auto& record = state.audio[i];
      const int64_t record_samples = std::ssize(record);

      if(record_samples <= 0)
      {
        for(int64_t j = first_pos; j < last; j++)
          out[j] = 0.;
        continue;
      }

      k = state.playbackPos % record_samples;
      for(int64_t j = first_pos; j < last; j++)
      {
        out[j] = record[k];
        record[k] += in[j];

        if(++k == record_samples)
          k = 0;
      }
    }
    state.playbackPos = k;
    state.faded = false;
  }

  struct ui
  {
    halp_meta(layout, halp::layouts::hbox)
    struct
    {
      halp_meta(layout, halp::layouts::vbox)
      halp_meta(background, halp::colors::background_mid)
      halp::control<&ins::mode> f;
      halp::control<&ins::quantif> q;
      halp::control<&ins::passthrough> p;
    } left;
    struct
    {
      halp_meta(layout, halp::layouts::vbox)
      halp_meta(background, halp::colors::background_mid)
      halp::control<&ins::postaction> p;
      halp::control<&ins::postaction_bars> b;
    } right;
  };
};
}
