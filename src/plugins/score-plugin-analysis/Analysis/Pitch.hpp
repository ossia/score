#pragma once
#include <Engine/Node/SimpleApi.hpp>

#include <ossia/detail/config.hpp>

#include <Analysis/GistState.hpp>

#if defined(OSSIA_ENABLE_KFR)
#include <kfr/base.hpp>
#include <kfr/dsp.hpp>
#endif

#include <numeric>
namespace Analysis
{
struct Pitch
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Pitch detector";
    static const constexpr auto objectKey = "Pitch";
    static const constexpr auto category = "Analysis/Pitch";
    static const constexpr auto author = "ossia score, Gist library";
    static const constexpr auto kind = Process::ProcessCategory::Analyzer;
    static const constexpr auto description = "Get the pitch of a signal";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const uuid_constexpr auto uuid
        = make_uuid("ed511605-8265-4b2c-8c4b-d3b189539b3b");

    static const constexpr audio_in audio_ins[]{"in"};
    static const constexpr value_out value_outs[]{"out"};
  };

#if defined(OSSIA_ENABLE_KFR)
  struct State : GistState
  {
    State()
        : hipass{kfr::to_sos(
            kfr::iir_highpass(kfr::butterworth<kfr::fbase>(12), 200, this->rate))}
    {
    }

    void filter(ossia::audio_port& in)
    {
      while(hipass.size() < in.channels())
      {
        hipass.emplace_back(kfr::to_sos(
            kfr::iir_highpass(kfr::butterworth<kfr::fbase>(12), 200, this->rate)));
      }

      int c = 0;
      for(ossia::audio_channel& chan : in)
      {
        hipass[c++].apply(chan.data(), chan.size());
      }
    }

    std::vector<kfr::biquad_filter<kfr::fbase, 32>> hipass;
  };
#else
  using State = GistState;
#endif
  using control_policy = ossia::safe_nodes::last_tick;

  static void
  run(const ossia::audio_port& in, ossia::value_port& out, ossia::token_request tk,
      ossia::exec_state_facade e, State& st)
  {
    if(in.channels() == 0)
      return;

#if defined(OSSIA_ENABLE_KFR)
    st.filter(const_cast<ossia::audio_port&>(in));
#endif
    st.process<&Gist<double>::pitch>(in, out, tk, e);
  }
};
}
