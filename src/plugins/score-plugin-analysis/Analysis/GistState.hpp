#pragma once
#include <Audio/Settings/Model.hpp>
#include <Gist.h>
#include <ossia/dataflow/audio_port.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/dataflow/value_port.hpp>
#include <ossia/network/value/value.hpp>
#include <ossia/detail/flat_map.hpp>
#include <mutex>

namespace ossia::safe_nodes
{
template <typename T>
using timed_vec = ossia::flat_map<int64_t, T>;
}

namespace Analysis
{
struct GistState
{
  // For efficiency we take a reference to the vector<value> member
  // of the ossia variant
  explicit GistState(int bufferSize, int rate):
    out_val{std::vector<ossia::value>{}}
  , output{out_val.v.m_impl.m_value8}
  , bufferSize{bufferSize}
  , rate{rate}
  {
    output.reserve(2);
    gist.reserve(2);
    gist.emplace_back(bufferSize, rate);
    gist.emplace_back(bufferSize, rate);
  }

  explicit GistState(Audio::Settings::Model& settings):
    GistState{settings.getBufferSize(), settings.getRate()}
  {
  }

  explicit GistState():
    GistState{score::AppContext().settings<Audio::Settings::Model>()}
  {
  }

  ~GistState()
  {
    gist.clear();
  }

  void preprocess(const ossia::audio_port& audio)
  {
    const auto N = audio.channels();
    output.resize(N);
    if(gist.size() < N)
    {
      gist.clear();
      gist.reserve(N);
      while(gist.size() < N)
        gist.emplace_back(bufferSize, rate);
    }
  }

  // No gain //
  template<auto Func>
  void process_mono(
      const ossia::audio_port& audio,
      ossia::value_port& out_port,
      const ossia::token_request& tk,
      const ossia::exec_state_facade& e)
  {
    float ret = 0.f;
    auto& c0 = audio.get()[0];
    auto& g0 = gist[0];
    {
      const auto samples = std::ssize(c0);
      if(samples > 0)
      {
        if(g0.getAudioFrameSize() != samples)
          g0.setAudioFrameSize(samples);

        g0.processAudioFrame(c0.data(), samples);
        ret = (g0.*Func)();
      }
    }

    const auto [tick_start, d] = e.timings(tk);
    out_port.write_value(ret, tick_start);
  }

  template<auto Func>
  void process_stereo(
      const ossia::audio_port& audio,
      ossia::value_port& out_port,
      const ossia::token_request& tk,
      const ossia::exec_state_facade& e)
  {
    ossia::vec2f ret = {0.f, 0.f};
    auto& c0 = audio.get()[0];
    auto& g0 = gist[0];
    {
      const auto samples = std::ssize(c0);
      if(samples > 0)
      {
        if(g0.getAudioFrameSize() != samples)
          g0.setAudioFrameSize(samples);

        g0.processAudioFrame(c0.data(), samples);
        ret[0] = (g0.*Func)();
      }
    }
    auto& c1 = audio.get()[1];
    auto& g1 = gist[1];
    {
      const auto samples = std::ssize(c1);
      if(samples > 0)
      {
        if(g1.getAudioFrameSize() != samples)
          g1.setAudioFrameSize(samples);

        g1.processAudioFrame(c0.data(), samples);
        ret[1] = (g1.*Func)();
      }
    }

    const auto [tick_start, d] = e.timings(tk);
    out_port.write_value(ret, tick_start);
  }

  template<auto Func>
  void process_multi(
      const ossia::audio_port& audio,
      ossia::value_port& out_port,
      const ossia::token_request& tk,
      const ossia::exec_state_facade& e)
  {
    auto it = output.begin();
    auto git = gist.begin();
    for(auto& channel : audio.get())
    {
      const auto samples = std::ssize(channel);
      if(samples > 0)
      {
        if(git->getAudioFrameSize() != samples)
          git->setAudioFrameSize(samples);

        git->processAudioFrame(channel.data(), samples);
        *it = float(((*git).*Func)());
      }
      else
      {
        *it = 0.f;
      }
      ++it;
      ++git;
    }

    const auto [tick_start, d] = e.timings(tk);
    out_port.write_value(out_val, tick_start);
  }


  // Gain, gate
  template<auto Func>
  void process_mono(
      const ossia::audio_port& audio,
      float gain,
      float gate,
      ossia::value_port& out_port,
      const ossia::token_request& tk,
      const ossia::exec_state_facade& e)
  {
    float ret = 0.f;
    auto& c0 = audio.get()[0];
    auto& g0 = gist[0];
    {
      const auto samples = std::ssize(c0);
      if(samples > 0)
      {
        if(g0.getAudioFrameSize() != samples)
          g0.setAudioFrameSize(samples);

        g0.processAudioFrame(c0.data(), samples, gain, gate);
        ret = (g0.*Func)();
      }
    }

    const auto [tick_start, d] = e.timings(tk);
    out_port.write_value(ret, tick_start);
  }

  template<auto Func>
  void process_stereo(
      const ossia::audio_port& audio,
      float gain,
      float gate,
      ossia::value_port& out_port,
      const ossia::token_request& tk,
      const ossia::exec_state_facade& e)
  {
    ossia::vec2f ret = {0.f, 0.f};
    auto& c0 = audio.get()[0];
    auto& g0 = gist[0];
    {
      const auto samples = std::ssize(c0);
      if(samples > 0)
      {
        if(g0.getAudioFrameSize() != samples)
          g0.setAudioFrameSize(samples);

        g0.processAudioFrame(c0.data(), samples, gain, gate);
        ret[0] = (g0.*Func)();
      }
    }
    auto& c1 = audio.get()[1];
    auto& g1 = gist[1];
    {
      const auto samples = std::ssize(c1);
      if(samples > 0)
      {
        if(g1.getAudioFrameSize() != samples)
          g1.setAudioFrameSize(samples);

        g1.processAudioFrame(c0.data(), samples, gain, gate);
        ret[1] = (g1.*Func)();
      }
    }

    const auto [tick_start, d] = e.timings(tk);
    out_port.write_value(ret, tick_start);
  }

  template<auto Func>
  void process_multi(
      const ossia::audio_port& audio,
      float gain,
      float gate,
      ossia::value_port& out_port,
      const ossia::token_request& tk,
      const ossia::exec_state_facade& e)
  {
    auto it = output.begin();
    auto git = gist.begin();
    for(auto& channel : audio.get())
    {
      const auto samples = std::ssize(channel);
      if(samples > 0)
      {
        if(git->getAudioFrameSize() != samples)
          git->setAudioFrameSize(samples);

        git->processAudioFrame(channel.data(), samples, gain, gate);
        float r{};
        *it = r = float(((*git).*Func)());
      }
      else
      {
        *it = 0.f;
      }
      ++it;
      ++git;
    }

    const auto [tick_start, d] = e.timings(tk);
    out_port.write_value(out_val, tick_start);
  }


  // Gain, gate, pulse
  template<auto Func>
  void process_mono(
      const ossia::audio_port& audio,
      float gain,
      float gate,
      ossia::value_port& out_port,
      ossia::value_port& pulse_port,
      const ossia::token_request& tk,
      const ossia::exec_state_facade& e)
  {
    float ret = 0.f;
    auto& c0 = audio.get()[0];
    auto& g0 = gist[0];
    {
      const auto samples = std::ssize(c0);
      if(samples > 0)
      {
        if(g0.getAudioFrameSize() != samples)
          g0.setAudioFrameSize(samples);

        g0.processAudioFrame(c0.data(), samples, gain, gate);
        ret = (g0.*Func)();
      }
    }

    const auto [tick_start, d] = e.timings(tk);
    out_port.write_value(ret, tick_start);

    if(ret >= 1.f)
      pulse_port.write_value(ossia::impulse{}, tick_start);
  }

  template<auto Func>
  void process_stereo(
      const ossia::audio_port& audio,
      float gain,
      float gate,
      ossia::value_port& out_port,
      ossia::value_port& pulse_port,
      const ossia::token_request& tk,
      const ossia::exec_state_facade& e)
  {
    ossia::vec2f ret = {0.f, 0.f};
    auto& c0 = audio.get()[0];
    auto& g0 = gist[0];
    {
      const auto samples = std::ssize(c0);
      if(samples > 0)
      {
        if(g0.getAudioFrameSize() != samples)
          g0.setAudioFrameSize(samples);

        g0.processAudioFrame(c0.data(), samples, gain, gate);
        ret[0] = (g0.*Func)();
      }
    }
    auto& c1 = audio.get()[1];
    auto& g1 = gist[1];
    {
      const auto samples = std::ssize(c1);
      if(samples > 0)
      {
        if(g1.getAudioFrameSize() != samples)
          g1.setAudioFrameSize(samples);

        g1.processAudioFrame(c0.data(), samples, gain, gate);
        ret[1] = (g1.*Func)();
      }
    }

    const auto [tick_start, d] = e.timings(tk);
    out_port.write_value(ret, tick_start);
    if(ret[0] >= 1.f || ret[1] >= 1.f)
      pulse_port.write_value(ossia::impulse{}, tick_start);
  }

  template<auto Func>
  void process_multi(
     const ossia::audio_port& audio,
     float gain,
     float gate,
     ossia::value_port& out_port,
     ossia::value_port& pulse_port,
     const ossia::token_request& tk,
     const ossia::exec_state_facade& e)
  {
    bool bang = false;
    auto it = output.begin();
    auto git = gist.begin();
    for(auto& channel : audio.get())
    {
      const auto samples = std::ssize(channel);
      if(samples > 0)
      {
        if(git->getAudioFrameSize() != samples)
          git->setAudioFrameSize(samples);

        git->processAudioFrame(channel.data(), samples, gain, gate);
        float r{};
        *it = r = float(((*git).*Func)());
        bang |= (r >= 1.f);
      }
      else
      {
        *it = 0.f;
      }
      ++it;
      ++git;
    }

    const auto [tick_start, d] = e.timings(tk);
    out_port.write_value(out_val, tick_start);
    if(bang) {
      pulse_port.write_value(ossia::impulse{}, tick_start);
    }
  }


  template<auto Func, typename... Args>
  void process(
      const ossia::audio_port& audio, Args&&... args)
  {
    preprocess(audio);

    switch(audio.channels())
    {
      case 1:
        return process_mono<Func>(audio, args...);
        break;
      case 2:
        return process_stereo<Func>(audio, args...);
        break;
      default:
        return process_multi<Func>(audio, args...);
        break;
    }
  }

  template<auto Func>
  void processVector(
      const ossia::audio_port& audio,
      ossia::audio_port& mfcc,
      const ossia::token_request& tk,
      const ossia::exec_state_facade& e)
  {
    while(gist.size() < audio.channels())
      gist.emplace_back(bufferSize, rate);

    mfcc.set_channels(audio.channels());
    auto it = mfcc.get().begin();
    auto git = gist.begin();
    for(auto& channel : audio.get())
    {
      const auto samples = std::ssize(channel);
      if(samples > 0)
      {
        if(git->getAudioFrameSize() != samples)
          git->setAudioFrameSize(samples);

        git->processAudioFrame(channel.data(), samples);

        auto& res = ((*git).*Func)();
        it->assign(res.begin(), res.end());
      }
      else
      {
        it->clear();
      }

      ++it;
      ++git;
    }
  }

    template<auto Func>
    void processVector(
        const ossia::audio_port& audio,
        float gain,
        float gate,
        ossia::audio_port& mfcc,
        const ossia::token_request& tk,
        const ossia::exec_state_facade& e)
    {
      while(gist.size() < audio.channels())
        gist.emplace_back(bufferSize, rate);

      mfcc.set_channels(audio.channels());
      auto it = mfcc.get().begin();
      auto git = gist.begin();
      for(auto& channel : audio.get())
      {
        const auto samples = std::ssize(channel);
        if(samples > 0)
        {
          if(git->getAudioFrameSize() != samples)
            git->setAudioFrameSize(samples);

          git->processAudioFrame(channel.data(), samples, gain, gate);

          auto& res = ((*git).*Func)();
          it->assign(res.begin(), res.end());
        }
        else
        {
          it->clear();
        }

        ++it;
        ++git;
      }
    }

  ossia::small_vector<Gist<double>, 2> gist;
  ossia::value out_val;
  std::vector<ossia::value>& output;
  int bufferSize{};
  int rate{};
};
}
