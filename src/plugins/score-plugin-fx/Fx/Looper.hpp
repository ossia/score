#pragma once
#include <Engine/Node/SimpleApi.hpp>

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
    static const uuid_constexpr auto uuid
        = make_uuid("a0ad4227-ac3d-448b-a19b-19581ed4e2c6");
    static const constexpr double recommended_height = 65;

    static const constexpr auto controls = tuplet::make_tuple(
        Control::Widgets::LoopChooser(),
        Control::Widgets::QuantificationChooser(),
        Control::Toggle("Passthrough", true),
        Control::Widgets::LoopPostActionChooser(),
        Control::IntSpinBox("Bars", 0, 64, 4)
        );
    static const constexpr audio_in audio_ins[]{"in"};
    static const constexpr audio_out audio_outs[]{"out"};
  };

  using control_policy = ossia::safe_nodes::last_tick;

  struct State
  {
    Control::Widgets::LoopMode quantizedPlayMode{Control::Widgets::LoopMode::Stop};
    Control::Widgets::LoopMode actualMode{Control::Widgets::LoopMode::Stop};
    ossia::audio_array audio;
    int64_t playbackPos{};
    ossia::time_value recordStart{};
    ossia::quarter_note recordStartBar{-1.};
    ossia::quarter_note prevTickLastBar{0.};
    int elapsedBars{-1};
    bool isPostRecording{false};
  };

  static void changeAction(const ossia::token_request& tk, State& state)
  {
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

    if(state.quantizedPlayMode == Control::Widgets::LoopMode::Record)
    {
      state.recordStart = tk.prev_date;
      state.recordStartBar = tk.musical_start_position;
      state.elapsedBars = -1;
    }
    else
    {
      state.recordStart = ossia::time_value{-1LL};
      state.recordStartBar = -1.;
      state.elapsedBars = -1;
    }
  }

  static void checkPostAction(
      const std::string& postaction,
      int postaction_bars,
      const ossia::token_request& tk,
      State& st)
  {

  }
  static void
  run(const ossia::audio_port& p1,
      const std::string& mode,
      float quantif,
      bool passthrough,
      const std::string& postaction,
      int postaction_bars,
      ossia::audio_port& p2,
      ossia::token_request tk,
      ossia::exec_state_facade st,
      State& state)
  {
    if(tk.date == tk.prev_date)
      return;

    using namespace ossia;

    auto m = Control::Widgets::GetLoopMode(mode);
    if (m != state.quantizedPlayMode)
    {
      if (quantif != 0 && tk.prev_date != 0_tv)
      {
        if (auto time = tk.get_quantification_date(1. / quantif); time && time > tk.prev_date)
        {
          if(time > tk.prev_date)
          {
            // Finish what we were doing until the quantization date
            {
              auto sub_tk = tk;
              sub_tk.set_end_time(*time - 1_tv);

              preAction(p1, p2, sub_tk, st, state, postaction, postaction_bars, passthrough);
            }

            // We can switch to the new mode
            state.quantizedPlayMode = m;
            state.actualMode = m;
            state.playbackPos = 0;

            // Remaining of the tick
            {
              auto sub_tk = tk;
              sub_tk.set_start_time(*time);

              changeAction(sub_tk, state);
              preAction(p1, p2, sub_tk, st, state, postaction, postaction_bars, passthrough);
            }
          }
          else
          {
            // We can switch to the new mode
            state.quantizedPlayMode = m;
            state.actualMode = m;
            state.playbackPos = 0;

            changeAction(tk, state);
            preAction(p1, p2, tk, st, state, postaction, postaction_bars, passthrough);
          }
        }
        else
        {
          // We cannot switch yet
          preAction(p1, p2, tk, st, state, postaction, postaction_bars, passthrough);
        }
      }
      else
      {
        // No quantization, we can switch to the new mode
        state.quantizedPlayMode = m;
        state.actualMode = m;
        state.playbackPos = 0;

        changeAction(tk, state);
        preAction(p1, p2, tk, st, state, postaction, postaction_bars, passthrough);
      }
    }
    else
    {
      // No change
      preAction(p1, p2, tk, st, state, postaction, postaction_bars, passthrough);
    }
  }

  static void preAction(
      const ossia::audio_port& p1,
      ossia::audio_port& p2,
      ossia::token_request tk,
      ossia::exec_state_facade st,
      State& state,
      const std::string& postaction,
      int postaction_bars,
      bool passthrough)
  {
    using namespace ossia;
    if(state.recordStartBar == -1.)
    {
      action(p1, p2, tk, st, state, passthrough);
    }
    else
    {
      // If we are in the token request which steps into the bar in which
      // we change because we are e.g. 4 bars after the recording started

      // Change of bar at the first sample
      if(tk.musical_start_last_bar != state.prevTickLastBar)
      {
        state.elapsedBars++;
        if(state.elapsedBars >= postaction_bars)
        {
          state.actualMode = Control::Widgets::GetLoopMode(postaction);
          state.recordStart = ossia::time_value{-1};
          state.recordStartBar = -1.;
          state.elapsedBars = -1;
        }
        action(p1, p2, tk, st, state, passthrough);
      }

      // Change of bar in the middle
      else if(tk.musical_end_last_bar != tk.musical_start_last_bar)
      {
        state.elapsedBars++;
        if(state.elapsedBars >= postaction_bars)
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
                action(p1, p2, sub_tk, st, state, passthrough);
              }

              // We can switch to the new mode
              state.actualMode = Control::Widgets::GetLoopMode(postaction);
              state.playbackPos = 0;

              // Remaining of the tick
              {
                auto sub_tk = tk;
                sub_tk.set_start_time(t);

                action(p1, p2, sub_tk, st, state, passthrough);
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
          state.actualMode = Control::Widgets::GetLoopMode(postaction);
          state.recordStart = ossia::time_value{-1};
          state.recordStartBar = -1.;
          state.elapsedBars = -1;
        }
      }
      else
      {
        action(p1, p2, tk, st, state, passthrough);
      }
    }

    state.prevTickLastBar = tk.musical_end_last_bar;
  }

  static void action(
      const ossia::audio_port& p1,
      ossia::audio_port& p2,
      const ossia::token_request& tk,
      ossia::exec_state_facade st,
      State& state,
      bool echoRecord)
  {
    auto timings = st.timings(tk);
    action(p1, p2, state, timings.start_sample, timings.length, echoRecord);
  }

  static void action(
      const ossia::audio_port& p1,
      ossia::audio_port& p2,
      State& state,
      int64_t start,
      int64_t length,
      bool echoRecord)
  {
    switch (state.actualMode)
    {
      case Control::Widgets::LoopMode::Play:
        if (state.audio.size() == 0 || state.audio[0].size() == 0)
          stop(p1, p2, state, start, length);
        else
          play(p1, p2, state, start, length);
        break;
      case Control::Widgets::LoopMode::Stop:
        stop(p1, p2, state, start, length);
        break;
      case Control::Widgets::LoopMode::Record:
        echoRecord ? record(p1, p2, state, start, length)
                   : record_noecho(p1, p2, state, start, length);
        break;
      case Control::Widgets::LoopMode::Overdub:
        echoRecord ? overdub(p1, p2, state, start, length)
                   : overdub_noecho(p1, p2, state, start, length);
        break;
    }
  }

  static void play(
      const ossia::audio_port& p1,
      ossia::audio_port& p2,
      State& state,
      int64_t first_pos,
      int64_t N)
  {
    // Copy input to output, and append input to buffer
    const auto chans = state.audio.size();
    p2.set_channels(chans);
    state.audio.resize(chans);

    int64_t k = state.playbackPos;
    for (std::size_t i = 0; i < chans; i++)
    {
      auto& out = p2.channel(i);
      auto& record = state.audio[i];

      out.resize(N);
      k = state.playbackPos;
      if (state.playbackPos + N < int64_t(record.size()))
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
  static void stop(
      const ossia::audio_port& p1,
      ossia::audio_port& p2,
      State& state,
      int64_t first_pos,
      int64_t N)
  {
    const auto chans = p1.channels();
    p2.set_channels(chans);

    for (std::size_t i = 0; i < chans; i++)
    {
      auto& in = p1.channel(i);
      auto& out = p2.channel(i);

      const int64_t samples = in.size();
      int64_t max = std::min(N, samples);

      out.resize(samples);

      for (int64_t j = first_pos; j < max; j++)
      {
        out[j] = in[j];
      }
    }
  }

  static void record(
      const ossia::audio_port& p1,
      ossia::audio_port& p2,
      State& state,
      int64_t first_pos,
      int64_t N)
  {
    // Copy input to output, and append input to buffer
    const auto chans = p1.channels();
    p2.set_channels(chans);
    state.audio.resize(chans);

    for (std::size_t i = 0; i < chans; i++)
    {
      auto& in = p1.channel(i);
      auto& out = p2.channel(i);
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

  static void record_noecho(
      const ossia::audio_port& p1,
      ossia::audio_port& p2,
      State& state,
      int64_t first_pos,
      int64_t N)
  {
    // Copy input to output, and append input to buffer
    const auto chans = p1.channels();
    p2.set_channels(chans);
    state.audio.resize(chans);

    for (std::size_t i = 0; i < chans; i++)
    {
      auto& in = p1.channel(i);
      auto& record = state.audio[i];

      const int64_t samples = in.size();
      int64_t max = std::min(N, samples);

      record.resize(state.playbackPos + samples);
      int64_t k = state.playbackPos;

      for (int64_t j = first_pos; j < max; j++)
      {
        record[k] = in[j];
        k++;
      }
    }
    state.playbackPos += N;
  }

  static void overdub(
      const ossia::audio_port& p1,
      ossia::audio_port& p2,
      State& state,
      int64_t first_pos,
      int64_t N)
  {
    //! if we go past the end we have to start from the beginning ? or we keep
    //! extending ?

    // Copy input to output, and append input to buffer
    const auto chans = p1.channels();
    p2.set_channels(chans);
    state.audio.resize(chans);

    for (std::size_t i = 0; i < chans; i++)
    {
      auto& in = p1.channel(i);
      auto& out = p2.channel(i);
      auto& record = state.audio[i];

      const int64_t samples = in.size();
      int64_t max = std::min(N, samples);

      out.resize(samples);
      if (int64_t(record.size()) < state.playbackPos + samples)
        record.resize(state.playbackPos + samples);
      int64_t k = state.playbackPos;

      for (int64_t j = first_pos; j < max; j++)
      {
        record[k] += in[j];
        out[j] = record[k];
        k++;
      }
    }
    state.playbackPos += N;
  }

  static void overdub_noecho(
      const ossia::audio_port& p1,
      ossia::audio_port& p2,
      State& state,
      int64_t first_pos,
      int64_t N)
  {
    //! if we go past the end we have to start from the beginning ? or we keep
    //! extending ?

    // Copy input to output, and append input to buffer
    const auto chans = p1.channels();
    p2.set_channels(chans);
    state.audio.resize(chans);

    for (std::size_t i = 0; i < chans; i++)
    {
      auto& in = p1.channel(i);
      auto& out = p2.channel(i);
      auto& record = state.audio[i];

      const int64_t samples = in.size();
      int64_t max = std::min(N, samples);

      out.resize(samples);
      if (int64_t(record.size()) < state.playbackPos + samples)
        record.resize(state.playbackPos + samples);
      int64_t k = state.playbackPos;

      for (int64_t j = first_pos; j < max; j++)
      {
        out[j] = record[k];
        record[k] += in[j];
        k++;
      }
    }
    state.playbackPos += N;
  }

  static void item(
      Process::Enum& mode,
      Process::ComboBox& quantif,
      Process::Toggle& echo,
      Process::Enum& playmode,
      Process::IntSpinBox& playmode_bars,
      const Process::ProcessModel& process,
      QGraphicsItem& parent,
      QObject& context,
      const Process::Context& doc)
  {
    using namespace Process;
    using namespace std;
    using namespace tuplet;
    const Process::PortFactoryList& portFactory
        = doc.app.interfaces<Process::PortFactoryList>();
    const auto c0 = 10;
    const auto c1 = 220;

    auto c0_bg = new score::BackgroundItem{&parent};
    c0_bg->setRect({0., 0., 340., 60});
    auto mode_item = makeControlNoText(
        get<0>(Metadata::controls),
        mode,
        parent,
        context,
        doc,
        portFactory);
    mode_item.root.setPos(c0, 10);
    mode_item.control.setPos({4, 0});
    mode_item.control.setRect({0, 0, 200, 30});
    mode_item.port.setPos({-8, 10});

    auto quant_item = makeControlNoText(
        get<1>(Metadata::controls),
        quantif,
        parent,
        context,
        doc,
        portFactory);
    quant_item.root.setPos(c1, 10);
    quant_item.control.setPos({10, 0});
    quant_item.port.setPos({-3, 4});

    auto echo_item = makeControl(
        get<2>(Metadata::controls),
        echo,
        parent,
        context,
        doc,
        portFactory);
    echo_item.root.setPos(c1, 35);
    echo_item.control.setPos({10, 0});
    echo_item.port.setPos({-3, 4});
    echo_item.text.setPos({30, 3});
  }
};
}
