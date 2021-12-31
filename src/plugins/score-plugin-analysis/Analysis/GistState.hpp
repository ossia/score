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
    output.resize(audio.channels());
    if(gist.size() < audio.channels())
    {
      gist.clear();
      gist.reserve(audio.channels());
      while(gist.size() < audio.channels())
        gist.emplace_back(bufferSize, rate);
    }
  }

  template<auto Func>
  void process(
      const ossia::audio_port& audio,
      ossia::value_port& out_port,
      const ossia::token_request& tk,
      const ossia::exec_state_facade& e)
  {
    preprocess(audio);
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

  template<auto Func>
  void process(
      const ossia::audio_port& audio,
      float gain,
      ossia::value_port& out_port,
      const ossia::token_request& tk,
      const ossia::exec_state_facade& e)
  {
    preprocess(audio);
    auto it = output.begin();
    auto git = gist.begin();
    for(auto& channel : audio.get())
    {
      const auto samples = std::ssize(channel);
      if(samples > 0)
      {
        if(git->getAudioFrameSize() != samples)
          git->setAudioFrameSize(samples);

        git->processAudioFrame(channel.data(), samples, gain, 0.0);
        *it = float((git->*Func)());
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

  template<auto Func>
  void process(
      const ossia::audio_port& audio,
      float gain,
      float gate,
      ossia::value_port& out_port,
      const ossia::token_request& tk,
      const ossia::exec_state_facade& e)
  {
    preprocess(audio);
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
    template<auto Func>
    void process(
       const ossia::audio_port& audio,
       float gain,
       float gate,
       ossia::value_port& out_port,
       ossia::value_port& pulse_port,
       const ossia::token_request& tk,
       const ossia::exec_state_facade& e)
    {
      preprocess(audio);

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
          bang |= (r > 1.f);
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
