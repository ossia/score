#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>

/*
namespace Control
{
static inline constexpr void test_nodes()
{
  constexpr NodeBuilder<> n{};
  constexpr auto res =
  n.audio_ins({{"foo"}, {"bar"}})
   .audio_outs({{"mimi"}});

  constexpr NodeInfo<std::array<AudioOutInfo, 1>, std::array<AudioInInfo, 2>>&
bld = res.build();
{
  constexpr std::tuple<std::array<AudioOutInfo, 1>, std::array<AudioInInfo, 2>>
t{bld};

  constexpr auto res2 = std::get<std::array<AudioInInfo, 2>>(t);
  static_assert(res2[0].name[0] == 'f');

  constexpr auto res1 = std::get<std::array<AudioOutInfo, 1>>(t);
  static_assert(res1[0].name[0] == 'm');
}
  constexpr auto res2 = std::get<std::array<AudioInInfo, 2>>(bld.v);
  static_assert(res2[0].name[0] == 'f');

  constexpr auto res1 = std::get<std::array<AudioOutInfo, 1>>(bld.v);
  static_assert(res1[0].name[0] == 'm');

  static_assert(get_ports<AudioInInfo>(bld.v).size() == 2);
  static_assert(get_ports<AudioOutInfo>(bld.v).size() == 1);
  static_assert(get_ports<AudioInInfo>(bld.v)[0].name[0] == 'f');
  static_assert(get_ports<AudioInInfo>(bld.v)[1].name[0] == 'b');
  constexpr auto ports = get_ports<AudioOutInfo>(bld.v);
  constexpr auto port = ports[0];
  constexpr auto name = port.name;
  static_assert(name[0] == 'm');
}
}

namespace Nodes
{

struct SomeInfo
{
    halp_meta(name, "My Funny Process")
    halp_meta(c_name, "FunnyProcess")
    halp_meta(category, "Test")
    static const constexpr auto uuid =
make_uuid("f6b88ec9-cd56-43e8-a568-33208d5a8fb7");
  
  struct {
    
  } inputs;
  struct {
    
  } outputs;
  static const constexpr auto info =
      Control::create_node()
      .audio_ins({{"audio1"}, {"audio2"}})
      .midi_ins()
      .midi_outs({{"midi1"}})
      .value_ins()
      .value_outs()
      .controls(Control::FloatSlider{"foo", .0, 10., 5.})
      .build();
  using control_policy = Control::DefaultTick;
  static void run(
      const ossia::audio_port& p1,
      const ossia::audio_port& p2,
      const ossia::timed_vec<float>& o1,
      ossia::midi_port& p,
      ossia::time_value tk.prev_date,
      ossia::token_request tk,
      ossia::exec_state_facade ,
      State& st)
  {
  }
};
}*/
