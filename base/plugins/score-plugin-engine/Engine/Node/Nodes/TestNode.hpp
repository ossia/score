#include <Engine/Node/PdNode.hpp>

namespace Process
{
static inline constexpr void test_nodes()
{
  constexpr NodeBuilder<> n{};
  constexpr auto res =
  n.audio_ins({{"foo"}, {"bar"}})
   .audio_outs({{"mimi"}});

  static_assert(get_ports<AudioInInfo>(res.build()).size() == 2);
  static_assert(get_ports<AudioOutInfo>(res.build()).size() == 1);
  static_assert(get_ports<AudioInInfo>(res.build())[0].name[0] == 'f');
  static_assert(get_ports<AudioInInfo>(res.build())[1].name[0] == 'b');
  static_assert(get_ports<AudioOutInfo>(res.build())[0].name[0] == 'm');
}
}

namespace Nodes
{

struct SomeInfo
{
  struct Metadata
  {
    static const constexpr auto prettyName = "My Funny Process";
    static const constexpr auto objectKey = "FunnyProcess";
    static const constexpr auto category = "Test";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto uuid = make_uuid("f6b88ec9-cd56-43e8-a568-33208d5a8fb7");
  };

  struct DefaultState
  {

  };

  static const constexpr auto info =
      Process::create_node()
      .audio_ins({{"audio1"}, {"audio2"}})
      .midi_ins()
      .midi_outs({{"midi1"}})
      .value_ins()
      .value_outs()
      .controls(Process::FloatSlider{"foo", .0, 10., 5.})
      .state<DefaultState>()
      .build();

  static void run(
      const ossia::audio_port& p1,
      const ossia::audio_port& p2,
      const Process::timed_vec<float>& o1,
      ossia::midi_port& p,
      DefaultState&,
      ossia::time_value prev_date,
      ossia::token_request tk,
      ossia::execution_state& st)
  {
  }
};
/*
struct test_gen
{
    using process = Process::ControlProcess<SomeInfo>;
    using process_factory = Process::GenericProcessModelFactory<process>;

    using executor = Process::Executor<SomeInfo>;
    using executor_factory = Engine::Execution::ProcessComponentFactory_T<executor>;

    using inspector = Process::InspectorWidget<SomeInfo>;
    using inspector_factory = Process::InspectorFactory<SomeInfo>;

    using layer_factory = WidgetLayer::LayerFactory<process, inspector>;
};
*/

}
