#pragma once
#include <Fx/MathMapping_generic.hpp>
#include <Fx/Types.hpp>

#include <ossia/dataflow/value_port.hpp>

#include <halp/layout.hpp>

namespace Nodes::MicroMapping
{
struct Node
{
  halp_meta(name, "Micromap")
  halp_meta(c_name, "MicroMapping")
  halp_meta(category, "Control/Mappings")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/exprtk.html#exprtk-support")
  halp_meta(author, "ossia score, ExprTK (Arash Partow)")
  halp_meta(description, "Applies a math expression to an input.")
  halp_meta(uuid, "25c64b87-a44a-4fed-9f60-0a48906fd3ec")

  struct ins
  {
    struct : halp::val_port<"in", ossia::value>
    {
      // Messages to this port trigger a new computation cycle with updated timestamps
      halp_flag(active_port);
    } port;
    halp::lineedit<"Expression", "x / 127"> expr;
  } inputs;

  struct
  {
    value_out port{};
  } outputs;

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

    ossia::math_expression expr;
    int64_t last_value_time{};

    //bool ok = false;
  } state;

  halp::setup setup;
  void prepare(halp::setup s) { setup = s; }

  using tick = halp::tick_flicks;

  void operator()(const tick& tk)
  {
    auto& self = this->state;
    if(!self.expr.set_expression(this->inputs.expr))
      return;

    if(self.expr.has_variable("xv"))
      GenericMathMapping<State>::run_array(
          inputs.port.value, outputs.port.call, tk, state);
    else
      GenericMathMapping<State>::run_scalar(
          inputs.port.value, outputs.port.call, tk, state);
  }

  struct ui
  {
    halp_meta(layout, halp::layouts::vbox)

    struct : halp::control<&ins::expr>
    {
      halp_flag(dynamic_size);
    } expr;
  };
};
}
