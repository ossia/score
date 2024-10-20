#pragma once
#include <Fx/MathHelpers.hpp>
#include <Fx/MathMapping_generic.hpp>
#include <Fx/Types.hpp>

#include <ossia/dataflow/value_port.hpp>

#include <halp/layout.hpp>

namespace Nodes
{
namespace MathGenerator
{
struct Node
{
  halp_meta(name, "Expression Value Generator")
  halp_meta(c_name, "MathGenerator")
  halp_meta(category, "Control/Generators")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/exprtk.html#exprtk-support")
  halp_meta(author, "ossia score, ExprTK (Arash Partow)")
  static const constexpr auto description
      = "Generate a signal from a math expression.\n"
        "Available variables: a,b,c, t (samples), dt (delta), pos (position "
        "in parent)\n"
        "See the documentation at http://www.partow.net/programming/exprtk";
  halp_meta(uuid, "d757bd0d-c0a1-4aec-bf72-945b722ab85b")

  struct ins
  {
    halp::lineedit<"Expression (ExprTK)", "cos(t) + log(pos * (1+abs(x)) / dt)"> expr;
    halp::hslider_f32<"Param (a)", halp::range{0., 1., 0.5}> a;
    halp::hslider_f32<"Param (b)", halp::range{0., 1., 0.5}> b;
    halp::hslider_f32<"Param (c)", halp::range{0., 1., 0.5}> c;
  } inputs;
  struct
  {
    value_out port{};
  } outputs;

  struct State
  {
    State()
    {
      pov.resize(1024);
      expr.add_vector("pov", pov);
      expr.add_variable("po", po);

      expr.add_variable("t", cur_time);
      expr.add_variable("dt", cur_deltatime);
      expr.add_variable("pos", cur_pos);

      expr.add_variable("a", a);
      expr.add_variable("b", b);
      expr.add_variable("c", c);
      expr.add_variable("pa", pa);
      expr.add_variable("pb", pb);
      expr.add_variable("pc", pc);

      expr.add_variable("m1", m1);
      expr.add_variable("m2", m2);
      expr.add_variable("m3", m3);
      expr.add_constants();
      expr.register_symbol_table();
    }
    std::vector<double> pov;
    double po{};

    double cur_time{};
    double cur_deltatime{};
    double cur_pos{};

    double a{}, b{}, c{};
    double pa{}, pb{}, pc{};

    double m1{}, m2{}, m3{};
    ossia::math_expression expr;
    bool ok = false;
  } state;

  using tick = halp::tick_flicks;

  void operator()(const tick& tk)
  {
    auto& self = this->state;
    if(!self.expr.set_expression(this->inputs.expr))
      return;

    setMathExpressionTiming(self, tk);
    self.a = this->inputs.a;
    self.b = this->inputs.b;
    self.c = this->inputs.c;

    auto res = self.expr.result();
    outputs.port(res);

    GenericMathMapping<State>::store_output(self, res);

    self.pa = self.a;
    self.pb = self.b;
    self.pc = self.c;
  }

  struct ui
  {
    halp_meta(layout, halp::layouts::vbox)
    struct
    {
      halp_meta(layout, halp::layouts::hbox)
      halp::control<&ins::a> a;
      halp::control<&ins::b> b;
      halp::control<&ins::c> c;
    } controls;

    struct : halp::control<&ins::expr>
    {
      halp_flag(dynamic_size);
    } expr;
  };
};
}
}
