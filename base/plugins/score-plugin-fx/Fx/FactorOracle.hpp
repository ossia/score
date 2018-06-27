#pragma once
#include <Engine/Node/PdNode.hpp>
namespace Nodes::FactorOracle
{
// ICI

struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Factor Oracle";
    static const constexpr auto objectKey = "Factor Oracle";
    static const constexpr auto category = "Impro";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto uuid
        = make_uuid("d90284c0-4196-47e0-802d-7e07342029ec");

    static const constexpr auto controls
    = std::make_tuple(
      Control::Toggle{"Generate", false},
      Control::IntSlider{"Longueur seq", 1, 20, 8}
    );
    static const constexpr auto value_ins
        = ossia::safe_nodes::value_ins<1>{{"in"}};
    static const constexpr auto value_outs
        = ossia::safe_nodes::value_outs<1>{{"out"}};
  };

  struct State
  {
    //FactorOracle oracle;
  };

  using control_policy = ossia::safe_nodes::last_tick;
  static void
  run(const ossia::value_port& in,
      bool generate,
      int longueur_seq,
      ossia::value_port& out,
      ossia::token_request,
      ossia::exec_state_facade,
      State& self)
  {
    /*
    if(generate)
    {
      self.oracle.constructionSequence();
      return;
    }
    else
    {
      // Entrées sont dans p1
      for(auto val : in.get_data())
      {
        char dernier_char_recu = ossia::convert<char>(val.value);
      }

      // Sorties sont dans out
      char a_envoyer = 'a';
      out.add_value(a_envoyer);
    }
    */
  }
};
}
