#pragma once
#include <Engine/Node/SimpleApi.hpp>

#include <ossia/network/value/value_conversion.hpp>

namespace Nodes
{
namespace MidiHiRes
{
struct Input
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Midi hi-res input";
    static const constexpr auto objectKey = "MidiHiResIn";
    static const constexpr auto category = "Midi";
    static const constexpr auto author = "ossia score";
    static const constexpr auto kind = Process::ProcessCategory::MidiEffect;
    static const constexpr auto description = "Creates a float from MSB/LSB CCs";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const uuid_constexpr auto uuid
        = make_uuid("28ca746e-c304-4ba6-bd5b-78934a1dec55");

    static const constexpr value_in value_ins[]{{"msb", false}, {"lsb", false}};
    static const constexpr value_out value_outs[]{"int", "float"};
  };

  using control_policy = ossia::safe_nodes::default_tick;

  static void
  run(const ossia::value_port& msb, const ossia::value_port& lsb, ossia::value_port& out,
      ossia::value_port& out_f, const ossia::token_request& tk,
      ossia::exec_state_facade st)
  {
    auto& msbs = msb.get_data();
    auto& lsbs = msb.get_data();
    if(msbs.empty() && lsbs.empty())
      return;

    const auto m = msbs.empty() ? 0 : ossia::convert<int>(msbs.back().value);
    const auto l = lsbs.empty() ? 0 : ossia::convert<int>(lsbs.back().value);

    auto [start, dur] = st.timings(tk);
    out.write_value(m * 127 + l, start);
    out_f.write_value(double(m * 127 + l) / (128. * 128.), start);
  }
};

struct Output
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Midi hi-res output";
    static const constexpr auto objectKey = "MidiHiResOut";
    static const constexpr auto category = "Midi";
    static const constexpr auto author = "ossia score";
    static const constexpr auto kind = Process::ProcessCategory::MidiEffect;
    static const constexpr auto description
        = "Creates MIDI LSB/MSB from a 0-16384 or 0-1 value";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const uuid_constexpr auto uuid
        = make_uuid("d6f5173b-b823-4571-b31f-660832b6132b");

    static const constexpr value_in value_ins[]{"int", "float"};
    static const constexpr value_out value_outs[]{"msb", "lsb"};
  };

  using control_policy = ossia::safe_nodes::default_tick;

  static void
  run(const ossia::value_port& in, const ossia::value_port& in_f, ossia::value_port& msb,
      ossia::value_port& lsb, const ossia::token_request& tk,
      ossia::exec_state_facade st)
  {
    for(auto& [val, t] : in.get_data())
    {
      const int32_t v = ossia::convert<int>(val);
      const int32_t m = ossia::clamp(int32_t(v / 127), 0, 127);
      const int32_t l = ossia::clamp(int32_t(v - m * 127), 0, 127);

      msb.write_value(m, t);
      lsb.write_value(l, t);
    }

    for(auto& [val, t] : in_f.get_data())
    {
      const float v = ossia::convert<float>(val) * (128. * 128.);
      const int32_t m = ossia::clamp(int32_t(v / 127.), 0, 127);
      const int32_t l = ossia::clamp(int32_t(v - m * 127), 0, 127);

      msb.write_value(m, t);
      lsb.write_value(l, t);
    }
  }
};
}
}
