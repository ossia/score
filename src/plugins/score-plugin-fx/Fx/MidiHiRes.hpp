#pragma once
#include <ossia/network/value/value_conversion.hpp>

#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/sample_accurate_controls.hpp>

namespace Nodes
{
namespace MidiHiRes
{
struct Input
{
  halp_meta(name, "Midi hi-res input")
  halp_meta(c_name, "MidiHiResIn")
  halp_meta(category, "Midi")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/javascript.html#midi")
  halp_meta(description, "Creates a float from MSB/LSB CCs")
  halp_meta(uuid, "28ca746e-c304-4ba6-bd5b-78934a1dec55")

  struct
  {
    // FIXME is_event false
    halp::accurate<halp::val_port<"msb", int>> msb;
    halp::accurate<halp::val_port<"lsb", int>> lsb;
  } inputs;
  struct
  {
    halp::callback<"int", int> i;
    halp::callback<"float", float> f;
  } outputs;

  void operator()()
  {
    auto& msbs = inputs.msb.values;
    auto& lsbs = inputs.lsb.values;
    if(msbs.empty() && lsbs.empty())
      return;

    const auto m = msbs.empty() ? 0 : ossia::convert<int>(msbs.rbegin()->second);
    const auto l = lsbs.empty() ? 0 : ossia::convert<int>(lsbs.rbegin()->second);

    outputs.i(m * 127 + l);
    outputs.f(double(m * 127 + l) / (128. * 128.));
  }
};

struct Output
{
  halp_meta(name, "Midi hi-res output")
  halp_meta(c_name, "MidiHiResOut")
  halp_meta(category, "Midi")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/javascript.html#midi")
  halp_meta(author, "ossia score")
  halp_meta(description, "Creates MIDI LSB/MSB from a 0-16384 or 0-1 value")
  halp_meta(uuid, "d6f5173b-b823-4571-b31f-660832b6132b")

  struct
  {
    halp::accurate<halp::val_port<"int", int>> i;
    halp::accurate<halp::val_port<"float", float>> f;
  } inputs;
  struct
  {
    halp::accurate<halp::val_port<"msb", int>> msb;
    halp::accurate<halp::val_port<"lsb", int>> lsb;
  } outputs;

  void operator()()
  {
    for(auto& [val, t] : inputs.i.values)
    {
      const int32_t v = ossia::convert<int>(val);
      const int32_t m = ossia::clamp(int32_t(v / 127), 0, 127);
      const int32_t l = ossia::clamp(int32_t(v - m * 127), 0, 127);

      outputs.msb.values.emplace(t, m);
      outputs.lsb.values.emplace(t, l);
    }

    for(auto& [val, t] : inputs.f.values)
    {
      const float v = ossia::convert<float>(val) * (128. * 128.);
      const int32_t m = ossia::clamp(int32_t(v / 127.), 0, 127);
      const int32_t l = ossia::clamp(int32_t(v - m * 127), 0, 127);

      outputs.msb.values.emplace(t, m);
      outputs.lsb.values.emplace(t, l);
    }
  }
};
}
}
