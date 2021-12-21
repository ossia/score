#pragma once
#include <Engine/Node/SimpleApi.hpp>
#include <Analysis/GistState.hpp>
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
    static const uuid_constexpr auto uuid = make_uuid("ed511605-8265-4b2c-8c4b-d3b189539b3b");

    static const constexpr audio_in audio_ins[]{"in"};
    static const constexpr value_out value_outs[]{"out"};
  };

  using State = GistState;
  using control_policy = ossia::safe_nodes::last_tick;

  static void
  run(const ossia::audio_port& in,
      ossia::value_port& out,
      ossia::token_request tk,
      ossia::exec_state_facade e,
      State& st)
  {
    st.process<&Gist<double>::pitch>(in, out, tk, e);
  }
};
}
