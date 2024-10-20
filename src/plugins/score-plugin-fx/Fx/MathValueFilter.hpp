#pragma once
#include <Fx/MathMapping_generic.hpp>
#include <Fx/Types.hpp>

#include <ossia/dataflow/value_port.hpp>

#include <halp/layout.hpp>

namespace Nodes::MathMapping
{
struct Node
{
  halp_meta(name, "Expression Value Filter")
  halp_meta(c_name, "MathMapping")
  halp_meta(category, "Control/Mappings")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/exprtk.html#exprtk-support")
  halp_meta(author, "ossia score, ExprTK (Arash Partow)")
  static const constexpr auto description
      = "Applies a math expression to an input.\n"
        "Available variables: a,b,c, t (samples), dt (delta), pos (position "
        "in parent), x (value)\n"
        "See the documentation at http://www.partow.net/programming/exprtk";
  halp_meta(uuid, "ae84e8b6-74ff-4259-aeeb-305d95cdfcab")

  // FIXME only correct way is to tick the entire graph more granularly
  // in the meantime, make it so that event port gets triggered for every value
  struct ins
  {
    struct : halp::val_port<"in", ossia::value>
    {
      // Messages to this port trigger a new computation cycle with updated timestamps
      halp_flag(active_port);
    } port;
    halp::lineedit<"Expression (ExprTK)", "cos(t) + log(pos * (1+abs(x)) / dt)"> expr;
    halp::hslider_f32<"Param (a)", halp::range{0., 1., 0.5}> a;
    halp::hslider_f32<"Param (b)", halp::range{0., 1., 0.5}> b;
    halp::hslider_f32<"Param (c)", halp::range{0., 1., 0.5}> c;
  } inputs;

  struct
  {
    value_out port{};
  } outputs;

  using tick = halp::tick_flicks;
  struct State
  {
    State()
    {
      xv.resize(1024);
      pxv.resize(1024);
      pov.resize(1024);
      expr.add_vector("xv", xv);
      expr.add_vector("pxv", pxv);
      expr.add_vector("pov", pov);

      expr.add_variable("x", x);
      expr.add_variable("px", px);
      expr.add_variable("po", po);

      expr.add_variable("t", cur_time);
      expr.add_variable("dt", cur_deltatime);
      expr.add_variable("pos", cur_pos);
      expr.add_variable("fs", fs);

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
    std::vector<double> xv;
    std::vector<double> pxv;
    std::vector<double> pov;

    double x{};
    double px{};
    double po{};

    double cur_time{};
    double cur_deltatime{};
    double cur_pos{};
    double fs{44100};

    double a{}, b{}, c{};
    double pa{}, pb{}, pc{};

    double m1{}, m2{}, m3{};

    ossia::math_expression expr;
    int64_t last_value_time{};

    bool ok = false;
  } state;

  halp::setup setup;
  void prepare(halp::setup s) { setup = s; }

  void operator()(const tick& tk)
  {
    auto& self = this->state;
    if(!self.expr.set_expression(this->inputs.expr))
      return;

    self.a = this->inputs.a;
    self.b = this->inputs.b;
    self.c = this->inputs.c;
    self.fs = setup.rate;

    if(self.expr.has_variable("xv"))
      GenericMathMapping<State>::run_array(
          inputs.port.value, outputs.port.call, tk, state);
    else
      GenericMathMapping<State>::run_scalar(
          inputs.port.value, outputs.port.call, tk, state);

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
