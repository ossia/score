#pragma once
#include <Fx/MathMapping_generic.hpp>
#include <Fx/Types.hpp>

#include <ossia/dataflow/value_port.hpp>

#include <boost/container/static_vector.hpp>

#include <halp/layout.hpp>

namespace Nodes::ArrayMapping
{
struct Node
{
  halp_meta(name, "Arraymap")
  halp_meta(c_name, "ArrayMapping")
  halp_meta(category, "Control/Mappings")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/exprtk.html#array-handling")
  halp_meta(author, "ossia score, ExprTK (Arash Partow)")
  halp_meta(description, "Applies a math expression to each member of an input.");
  halp_meta(uuid, "1fe9c806-b601-4ee0-9fbb-0ab817c4dd87");

  struct ins
  {
    struct : halp::val_port<"in", ossia::value>
    {
      // Messages to this port trigger a new computation cycle with updated timestamps
      halp_flag(active_port);
    } port;
    halp::lineedit<"Expression", "x"> expr;
  } inputs;

  struct
  {
    value_out port{};
  } outputs;

  struct State
  {
    struct Expr
    {
      void init(double& cur_time, double& cur_deltatime, double& cur_pos, double& count)
      {
        expr.add_variable("i", instance);
        expr.add_variable("n", count);

        expr.add_variable("x", x);
        expr.add_variable("px", px);
        expr.add_variable("po", po);

        expr.add_variable("t", cur_time);
        expr.add_variable("dt", cur_deltatime);
        expr.add_variable("pos", cur_pos);
        expr.add_constants();

        expr.register_symbol_table();
      }
      double instance;

      double x;
      double px;
      double po;

      ossia::math_expression expr;
    };

    boost::container::static_vector<Expr, 1024> expressions;
    double count{};
    double cur_time{};
    double cur_deltatime{};
    double cur_pos{};

    int64_t last_value_time{};
    std::string last_expression;
  } state;

  using tick = halp::tick_flicks;

  void operator()(const tick& tk)
  {
    GenericMathMapping<State>::run_polyphonic(
        inputs.port.value, outputs.port.call, inputs.expr.value, tk, state);
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
