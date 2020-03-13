#pragma once
#include <Engine/Node/PdNode.hpp>

namespace Nodes::AudioLooper
{
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Looper (audio)";
    static const constexpr auto objectKey = "Looper (audio)";
    static const constexpr auto category = "Audio";
    static const constexpr auto author = "ossia score";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto kind = Process::ProcessCategory::AudioEffect;
    static const constexpr auto description = "Loop audio";
    static const constexpr auto uuid = make_uuid("a0ad4227-ac3d-448b-a19b-19581ed4e2c6");

    static const constexpr auto controls
        = std::make_tuple(Control::Widgets::LoopChooser(), Control::Widgets::QuantificationChooser());
    static const constexpr audio_in audio_ins[]{"in"};
    static const constexpr audio_out audio_outs[]{"out"};
  };

  using control_policy = ossia::safe_nodes::last_tick;


  struct State
  {
    Control::Widgets::LoopMode actualMode{Control::Widgets::LoopMode::Stop};
    ossia::audio_array audio;
    int64_t playbackPos{};
  };

  static void
  run(const ossia::audio_port& p1,
      const std::string& mode,
      float quantif,
      ossia::audio_port& p2,
      ossia::token_request tk,
      ossia::exec_state_facade st,
      State& state)
  {
    const double modelRatio = st.modelToSamples();

    auto m = Control::Widgets::GetLoopMode(mode);
    if(m != state.actualMode)
    {
      if(auto time = tk.get_quantification_date(quantif))
      {
        // Finish what we were doing until the quantization date
        {
          auto sub_tk = tk;
          sub_tk.reduce_end_time(*time);
          action(p1, p2, sub_tk, state, modelRatio);
        }

        // We can switch to the new mode
        state.actualMode = m;
        state.playbackPos = 0;

        // Remaining of the tick
        {
          auto sub_tk = tk;
          sub_tk.increase_start_time(tk.date - *time);

          action(p1, p2, sub_tk, state, modelRatio);
        }
      }
      else
      {
        action(p1, p2, tk, state, modelRatio);
      }
    }
    else
    {
      action(p1, p2, tk, state, modelRatio);
    }
  }

  static void action(const ossia::audio_port& p1,
                     ossia::audio_port& p2,
                     ossia::token_request tk,
                     State& state,
                     double modelRatio)
  {
    switch(state.actualMode)
    {
      case Control::Widgets::LoopMode::Play:
        if(state.audio.size() == 0 || state.audio[0].size() == 0)
          stop(p1, p2, tk, state, modelRatio);
        else
          play(p1, p2, tk, state, modelRatio);
        break;
      case Control::Widgets::LoopMode::Stop:
        stop(p1, p2, tk, state, modelRatio);
        break;
      case Control::Widgets::LoopMode::Record:
        record(p1, p2, tk, state, modelRatio);
        break;
      case Control::Widgets::LoopMode::Overdub:
        overdub(p1, p2, tk, state, modelRatio);
        break;
    }
  }

  static void play(const ossia::audio_port& p1,
                   ossia::audio_port& p2,
                   ossia::token_request tk,
                   State& state,
                   double modelRatio)
  {
    // Copy input to output, and append input to buffer
    const auto chans = p1.samples.size();
    p2.samples.resize(chans);
    state.audio.resize(chans);

    const int64_t N = tk.physical_write_duration(modelRatio);
    const int64_t first_pos = tk.physical_start(modelRatio);

    int64_t k = state.playbackPos;
    for (std::size_t i = 0; i < chans; i++)
    {
      auto& out = p2.samples[i];
      auto& record = state.audio[i];

      const int64_t samples = N;

      out.resize(N);
      k = state.playbackPos;
      if (state.playbackPos + N < record.size())
      {
        for (int64_t j = first_pos; j < N; j++)
        {
          out[j] = record[k];
          k++;
        }
      }
      else
      {
        auto max = int64_t(record.size()) - state.playbackPos;
        assert(max >= 0);
        int64_t j = first_pos;
        for (; j < max; j++)
        {
          out[j] = record[k];
          k++;
        }
        k = 0;

        for (; j < N; j++)
        {
          out[j] = record[k];
          k++;
        }
      }
    }
    state.playbackPos = k;
  }

  // We just copy input to output
  static void stop(const ossia::audio_port& p1,
                   ossia::audio_port& p2,
                   ossia::token_request tk,
                   State& state,
                   double modelRatio)
  {
    const auto chans = p1.samples.size();
    p2.samples.resize(chans);
    const int64_t N = tk.physical_write_duration(modelRatio);
    const int64_t first_pos = tk.physical_start(modelRatio);

    for (std::size_t i = 0; i < chans; i++)
    {
      auto& in = p1.samples[i];
      auto& out = p2.samples[i];

      const int64_t samples = in.size();
      int64_t max = std::min(N, samples);

      out.resize(samples);

      for (int64_t j = first_pos; j < max; j++)
      {
        out[j] = in[j];
      }
    }
  }

  static void record(const ossia::audio_port& p1,
                   ossia::audio_port& p2,
                   ossia::token_request tk,
                   State& state,
                   double modelRatio)
  {
    // Copy input to output, and append input to buffer
    const auto chans = p1.samples.size();
    p2.samples.resize(chans);
    state.audio.resize(chans);

    const int64_t N = tk.physical_write_duration(modelRatio);
    const int64_t first_pos = tk.physical_start(modelRatio);

    for (std::size_t i = 0; i < chans; i++)
    {
      auto& in = p1.samples[i];
      auto& out = p2.samples[i];
      auto& record = state.audio[i];

      const int64_t samples = in.size();
      int64_t max = std::min(N, samples);

      out.resize(samples);
      record.resize(state.playbackPos + samples);
      int64_t k = state.playbackPos;

      for (int64_t j = first_pos; j < max; j++)
      {
        out[j] = in[j];
        record[k] = in[j];
        k++;
      }
    }
    state.playbackPos += N;
  }

  static void overdub(const ossia::audio_port& p1,
                   ossia::audio_port& p2,
                   ossia::token_request tk,
                   State& state,
                   double modelRatio)
  {
    //! if we go past the end we have to start from the beginning ? or we keep extending ?

    // Copy input to output, and append input to buffer
    const auto chans = p1.samples.size();
    p2.samples.resize(chans);
    state.audio.resize(chans);

    const int64_t N = tk.physical_write_duration(modelRatio);
    const int64_t first_pos = tk.physical_start(modelRatio);

    for (std::size_t i = 0; i < chans; i++)
    {
      auto& in = p1.samples[i];
      auto& out = p2.samples[i];
      auto& record = state.audio[i];

      const int64_t samples = in.size();
      int64_t max = std::min(N, samples);

      out.resize(samples);
      if(record.size() < state.playbackPos + samples)
        record.resize(state.playbackPos + samples);
      int64_t k = state.playbackPos;

      for (int64_t j = first_pos; j < max; j++)
      {
        out[j] = in[j];
        record[k] += in[j];
        k++;
      }
    }
    state.playbackPos += N;
  }
};
}
