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
    int postaction_bars{};
    double sampleRate{48000.};
    bool isPostRecording{false};

    static constexpr int64_t default_buffer_size = 192000 * 32;
    void reset_elapsed() { }
    int channels() const noexcept { return actualChannels; }
    void set_channels(int chans)
    {
      const int64_t cur_channels = std::ssize(audio);
      actualChannels = chans;
      if(actualChannels > cur_channels)
      {
        audio.resize(actualChannels);

        int64_t min_size = audio[0].size();
        int64_t min_capa = std::max(int64_t(audio[0].capacity()), default_buffer_size);
        for(int i = cur_channels; i < actualChannels; i++)
        {
          audio[i].reserve(min_capa);
          audio[i].resize(min_size);
        }
      }
      else if(actualChannels < cur_channels)
      {
        for(int i = actualChannels; i < cur_channels; i++)
        {
          audio[i].resize(0);
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
    const double sr = state.sampleRate;
    const double bar_samples
        = sr * 4. * (double(tk.signature.upper) / tk.signature.lower) * (60. / tk.tempo);
    const double total_samples = std::floor(state.postaction_bars * bar_samples);

    // If there are more samples than expected we crop
    const bool quantify_length = (state.quantif > 0.f) && (state.channels() > 0);
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
      int samples = chan.size();
      if(int min_n = std::min(samples, (int)128); min_n > 0)
      {
        float f = 1. / min_n;
        float ff = 0.;
        for(int i = 0; i < min_n; i++)
        {
          chan[i] *= ff;
          ff += f;
        }

        for(int i = samples - min_n; i < samples; i++)
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
    if(state.quantizedPlayMode == LoopMode::Record)
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

    if(quantif != 0 && tk.prev_date != 0_tv)
    {
      state.quantif = quantif;
      if(auto time = tk.get_quantification_date(1. / quantif);
         time && time > tk.prev_date)
      {
        state.this_buffer_quantif_time = *time;
        state.this_buffer_quantif_sample
            = tk.to_physical_time_in_tick(*time, ossia_state.modelToSamples());
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
            // Finish what we were doing until the quantization date
            {
              auto sub_tk = tk;
              sub_tk.set_end_time(*time - 1_tv);

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
        if(auto quant_date = tk.get_quantification_date(1.0))
        {
          ossia::time_value t = *quant_date;
          if(t > tk.prev_date)
          {
            // Finish what we were doing until the quantization date
            {
              auto sub_tk = tk;
              sub_tk.set_end_time(t - 1_tv);
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
            qDebug("very weird");
          }
        }
        else
        {
          qDebug("weird");
        }
        // just in case:
        switch_to_main_mode();
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
    action(timings.start_sample, timings.length, echoRecord);
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
    // Copy input to output, and append input to buffer
    const auto chans = state.channels();

    if(chans == 0)
      return;

    int64_t k = state.playbackPos;
    for(int i = 0; i < chans; i++)
    {
      auto& out = p2.samples[i];
      auto& record = state.audio[i];
      const int64_t chan_samples = record.size();

      k = state.playbackPos;
      if(state.playbackPos + samples < chan_samples)
      {
        for(int64_t j = first_pos; j < samples; j++)
        {
          out[j] = record[k];
          k++;
        }
      }
      else
      {
        int64_t max = chan_samples - state.playbackPos;
        int64_t j = first_pos;
        for(; j < max; j++)
        {
          out[j] = record[k];
          k++;
        }

        //if(state.quantif == 0.f)
        {
          // No quantification, we directly loop the content
          k = 0;

          // TODO refactor sound_reader so that we can use it to have the proper repeated loop behaviour here...
          for(; j < std::min(samples, chan_samples); j++)
          {
            out[j] = record[k];
            k++;
          }
        }

        /*
        else if(state.this_buffer_quantif_sample)
        {
          // Quantification in this tick
          int64_t last_silence  = std::min(N, *state.this_buffer_quantif_sample);

          // First silence
          for (; j < last_silence; j++)
          {
            out[j] = 0.f;
            k++;
          }

          // Then loop our content back
          k = 0;
          for (; j < std::min(N, chan_samples); j++)
          {
            out[j] = record[k];
            k++;
          }
        }
        else
        {
          // Quantification not in this tick, just silence
          for (; j < N; j++)
          {
            out[j] = 0.f;
            k++;
          }
        }
        */
      }
    }

    state.playbackPos = k;
  }

  // We just copy input to output
  void stop(int64_t first_pos, int64_t samples)
  {
    auto& p1 = inputs.audio;
    auto& p2 = outputs.audio;
    const auto chans = p1.channels;

    for(int i = 0; i < chans; i++)
    {
      auto& in = p1.samples[i];
      auto& out = p2.samples[i];

      for(int64_t j = first_pos; j < samples; j++)
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

    for(int i = 0; i < chans; i++)
    {
      auto& in = p1.samples[i];
      auto& out = p2.samples[i];
      auto& record = state.audio[i];

      record.resize(state.playbackPos + samples);
      int64_t k = state.playbackPos;

      for(int64_t j = first_pos; j < samples; j++)
      {
        out[j] = in[j];
        record[k] = in[j];
        k++;
      }
    }
    state.playbackPos += samples;
  }

  void record_noecho(int64_t first_pos, int64_t samples)
  {
    auto& p1 = inputs.audio;
    // Copy input to output, and append input to buffer
    const auto chans = p1.channels;
    state.set_channels(chans);

    for(int i = 0; i < chans; i++)
    {
      auto& in = p1.samples[i];
      auto& record = state.audio[i];

      record.resize(state.playbackPos + samples);
      int64_t k = state.playbackPos;

      for(int64_t j = first_pos; j < samples; j++)
      {
        record[k] = in[j];
        k++;
      }
    }
    state.playbackPos += samples;
  }

  void overdub(int64_t first_pos, int64_t samples)
  {
    auto& p1 = inputs.audio;
    auto& p2 = outputs.audio;
    //! if we go past the end we have to start from the beginning ? or we keep
    //! extending ?

    // Copy input to output, and append input to buffer
    const auto chans = p1.channels;
    state.set_channels(chans);

    for(int i = 0; i < chans; i++)
    {
      auto& in = p1.samples[i];
      auto& out = p2.samples[i];
      auto& record = state.audio[i];
      const int64_t record_samples = record.size();

      int64_t k = state.playbackPos;

      for(int64_t j = first_pos; j < samples; j++)
      {
        if(k >= record_samples)
          k = 0;

        record[k] += in[j];
        out[j] = record[k];

        k++;
      }
    }
    state.playbackPos += samples;
  }

  void overdub_noecho(int64_t first_pos, int64_t samples)
  {
    auto& p1 = inputs.audio;
    auto& p2 = outputs.audio;
    //! if we go past the end we have to start from the beginning ? or we keep
    //! extending ?

    // Copy input to output, and append input to buffer
    const auto chans = p1.channels;
    state.set_channels(chans);

    for(int i = 0; i < chans; i++)
    {
      auto& in = p1.samples[i];
      auto& out = p2.samples[i];
      auto& record = state.audio[i];
      const int64_t record_samples = record.size();

      int64_t k = state.playbackPos;

      for(int64_t j = first_pos; j < samples; j++)
      {
        if(k >= record_samples)
          k = 0;

        out[j] = record[k];
        record[k] += in[j];

        k++;
      }
    }
    state.playbackPos += samples;
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
