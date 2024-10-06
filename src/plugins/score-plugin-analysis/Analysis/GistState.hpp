#pragma once
#include <Audio/Settings/Model.hpp>

#include <score/application/ApplicationContext.hpp>

#include <ossia/dataflow/audio_port.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/dataflow/value_port.hpp>
#include <ossia/detail/flat_map.hpp>
#include <ossia/network/value/value.hpp>

#include <Analysis/Helpers.hpp>

#include <Gist.h>

namespace ossia::safe_nodes
{
template <typename T>
using timed_vec = ossia::flat_map<int64_t, T>;
}

namespace Analysis
{
struct GistState
{
  explicit GistState(int bufferSize, int rate)
      : bufferSize{bufferSize}
      , rate{rate}
  {
    gist.reserve(2);
    gist.emplace_back(bufferSize, rate);
    gist.emplace_back(bufferSize, rate);
  }

  explicit GistState(Audio::Settings::Model& settings)
      : GistState{settings.getBufferSize(), settings.getRate()}
  {
  }

  explicit GistState()
      : GistState{score::AppContext().settings<Audio::Settings::Model>()}
  {
  }

  ~GistState() { gist.clear(); }

  static int channels(auto& audio) noexcept { return audio.channels; }

  static auto data(auto* channel) noexcept { return channel; }

  static auto frames(auto& channel, int d) noexcept
  {
    if constexpr(requires { channel.size(); })
      return channel.size();
    else
      return d;
  }

  // FIXME have it sample-accurate, for onset detection
  template <typename V>
  static auto write_value(auto& out_port, V&& ret)
  {
    if constexpr(std::is_same_v<V, ossia::impulse>)
      out_port();
    else
      out_port.value = std::move(ret);
  }

  void preprocess(const auto& audio)
  {
    const auto N = channels(audio);
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
  template <auto Func>
  void process_mono(const auto& audio, auto& out_port, int d)
  {
    float ret = 0.f;
    decltype(auto) c0 = audio.get()[0];
    auto& g0 = gist[0];
    {
      const auto samples = frames(c0, d);
      if(samples > 0)
      {
        if(g0.getAudioFrameSize() != samples)
          g0.setAudioFrameSize(samples);

        g0.processAudioFrame(data(c0), samples);
        ret = (g0.*Func)();
      }
    }

    write_value(out_port, ret);
  }

  template <auto Func>
  void process_stereo(const auto& audio, auto& out_port, int d)
  {
    ossia::vec2f ret = {0.f, 0.f};
    decltype(auto) c0 = audio.get()[0];
    auto& g0 = gist[0];
    {
      const auto samples = frames(c0, d);
      if(samples > 0)
      {
        if(g0.getAudioFrameSize() != samples)
          g0.setAudioFrameSize(samples);

        g0.processAudioFrame(data(c0), samples);
        ret[0] = (g0.*Func)();
      }
    }
    decltype(auto) c1 = audio.get()[1];
    auto& g1 = gist[1];
    {
      const auto samples = frames(c1, d);
      if(samples > 0)
      {
        if(g1.getAudioFrameSize() != samples)
          g1.setAudioFrameSize(samples);

        g1.processAudioFrame(data(c0), samples);
        ret[1] = (g1.*Func)();
      }
    }

    write_value(out_port, ret);
  }

  template <auto Func>
  void process_multi(const auto& audio, auto& out_port, int d)
  {
    auto it = output.begin();
    auto git = gist.begin();
    for(auto& channel : audio.get())
    {
      const auto samples = frames(channel, d);
      if(samples > 0)
      {
        if(git->getAudioFrameSize() != samples)
          git->setAudioFrameSize(samples);

        git->processAudioFrame(data(channel), samples);
        *it = float(((*git).*Func)());
      }
      else
      {
        *it = 0.f;
      }
      ++it;
      ++git;
    }

    write_value(out_port, output);
  }

  // Gain, gate
  template <auto Func>
  void process_mono(const auto& audio, float gain, float gate, auto& out_port, int d)
  {
    float ret = 0.f;
    decltype(auto) c0 = audio.get()[0];
    auto& g0 = gist[0];
    {
      const auto samples = frames(c0, d);
      if(samples > 0)
      {
        if(g0.getAudioFrameSize() != samples)
          g0.setAudioFrameSize(samples);

        g0.processAudioFrame(data(c0), samples, gain, gate);
        ret = (g0.*Func)();
      }
    }

    write_value(out_port, ret);
  }

  template <auto Func>
  void process_stereo(const auto& audio, float gain, float gate, auto& out_port, int d)
  {
    ossia::vec2f ret = {0.f, 0.f};
    decltype(auto) c0 = audio.get()[0];
    auto& g0 = gist[0];
    {
      const auto samples = frames(c0, d);
      if(samples > 0)
      {
        if(g0.getAudioFrameSize() != samples)
          g0.setAudioFrameSize(samples);

        g0.processAudioFrame(data(c0), samples, gain, gate);
        ret[0] = (g0.*Func)();
      }
    }
    decltype(auto) c1 = audio.get()[1];
    auto& g1 = gist[1];
    {
      const auto samples = frames(c1, d);
      if(samples > 0)
      {
        if(g1.getAudioFrameSize() != samples)
          g1.setAudioFrameSize(samples);

        g1.processAudioFrame(data(c0), samples, gain, gate);
        ret[1] = (g1.*Func)();
      }
    }

    write_value(out_port, ret);
  }

  template <auto Func>
  void process_multi(const auto& audio, float gain, float gate, auto& out_port, int d)
  {
    auto it = output.begin();
    auto git = gist.begin();
    for(auto& channel : audio.get())
    {
      const auto samples = frames(channel, d);
      if(samples > 0)
      {
        if(git->getAudioFrameSize() != samples)
          git->setAudioFrameSize(samples);

        git->processAudioFrame(data(channel), samples, gain, gate);
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

    write_value(out_port, output);
  }

  // Gain, gate, pulse
  template <auto Func>
  void process_mono(
      const auto& audio, float gain, float gate, auto& out_port, auto& pulse_port, int d)
  {
    float ret = 0.f;
    decltype(auto) c0 = audio.get()[0];
    auto& g0 = gist[0];
    {
      const auto samples = frames(c0, d);
      if(samples > 0)
      {
        if(g0.getAudioFrameSize() != samples)
          g0.setAudioFrameSize(samples);

        g0.processAudioFrame(data(c0), samples, gain, gate);
        ret = (g0.*Func)();
      }
    }

    write_value(out_port, ret);

    if(ret >= 1.f)
      write_value(pulse_port, ossia::impulse{});
  }

  template <auto Func>
  void process_stereo(
      const auto& audio, float gain, float gate, auto& out_port, auto& pulse_port, int d)
  {
    ossia::vec2f ret = {0.f, 0.f};
    decltype(auto) c0 = audio.get()[0];
    auto& g0 = gist[0];
    {
      const auto samples = frames(c0, d);
      if(samples > 0)
      {
        if(g0.getAudioFrameSize() != samples)
          g0.setAudioFrameSize(samples);

        g0.processAudioFrame(data(c0), samples, gain, gate);
        ret[0] = (g0.*Func)();
      }
    }
    decltype(auto) c1 = audio.get()[1];
    auto& g1 = gist[1];
    {
      const auto samples = frames(c1, d);
      if(samples > 0)
      {
        if(g1.getAudioFrameSize() != samples)
          g1.setAudioFrameSize(samples);

        g1.processAudioFrame(data(c0), samples, gain, gate);
        ret[1] = (g1.*Func)();
      }
    }

    write_value(out_port, ret);
    if(ret[0] >= 1.f || ret[1] >= 1.f)
      write_value(pulse_port, ossia::impulse{});
  }

  template <auto Func>
  void process_multi(
      const auto& audio, float gain, float gate, auto& out_port, auto& pulse_port, int d)
  {
    bool bang = false;
    auto it = output.begin();
    auto git = gist.begin();
    for(auto& channel : audio.get())
    {
      const auto samples = frames(channel, d);
      if(samples > 0)
      {
        if(git->getAudioFrameSize() != samples)
          git->setAudioFrameSize(samples);

        git->processAudioFrame(data(channel), samples, gain, gate);
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

    write_value(out_port, output);
    if(bang)
    {
      write_value(pulse_port, ossia::impulse{});
    }
  }

  template <auto Func, typename... Args>
  void process(const auto& audio, Args&&... args)
  {
    preprocess(audio);

    switch(channels(audio))
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

  template <auto Func>
  void processVector(const auto& audio, ossia::audio_port& mfcc, int d)
  {
    while(gist.size() < channels(audio))
      gist.emplace_back(bufferSize, rate);

    mfcc.set_channels(channels(audio));
    auto it = mfcc.get().begin();
    auto git = gist.begin();
    for(auto& channel : audio.get())
    {
      const auto samples = frames(channel, d);
      if(samples > 0)
      {
        if(git->getAudioFrameSize() != samples)
          git->setAudioFrameSize(samples);

        git->processAudioFrame(data(channel), samples);

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

  template <auto Func>
  void processVector(const auto& audio, float gain, float gate, auto& mfcc, int d)
  {
    while(gist.size() < channels(audio))
      gist.emplace_back(bufferSize, rate);
    auto git = gist.begin();

    // mfcc.set_channels(channels(audio));
    double** it = mfcc.get().begin();

    for(auto& channel : audio.get())
    {
      const auto samples = frames(channel, d);
      std::fill_n(*it, d, 0.f);
      if(samples > 0)
      {
        if(git->getAudioFrameSize() != samples)
          git->setAudioFrameSize(samples);

        git->processAudioFrame(data(channel), samples, gain, gate);

        decltype(auto) res = ((*git).*Func)();
        SCORE_ASSERT(res.size() <= d);
        std::copy_n(res.begin(), res.size(), *it);
      }
      ++it;
      ++git;
    }
  }

  ossia::small_vector<Gist<double>, 2> gist;
  Analysis::analysis_vector output;
  int bufferSize{};
  int rate{};
};
}
