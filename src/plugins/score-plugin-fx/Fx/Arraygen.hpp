#pragma once
#include <Fx/MathMapping_generic.hpp>
#include <Fx/Types.hpp>

#include <boost/container/static_vector.hpp>

#include <halp/layout.hpp>

namespace Nodes::ArrayGenerator
{
struct Node
{
  halp_meta(name, "Arraygen")
  halp_meta(c_name, "ArrayGenerator")
  halp_meta(category, "Control/Generators")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/exprtk.html#array-handling")
  halp_meta(author, "ossia score, ExprTK (Arash Partow)")
  halp_meta(description, "Applies a math expression to each member of an input.");
  halp_meta(uuid, "cf3df02f-a563-4e92-a739-b321d3a84252");

  using code_writer = Nodes::MathMappingCodeWriter;

  struct ins
  {
    halp::lineedit<"Expression", "i"> expr;
    halp::spinbox_i32<"Size", halp::irange{0, 1024, 12}> sz;
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

        expr.add_variable("po", po);

        expr.add_variable("t", cur_time);
        expr.add_variable("dt", cur_deltatime);
        expr.add_variable("pos", cur_pos);
        expr.add_constants();

        expr.register_symbol_table();
      }
      double instance;

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

  ossia::exec_state_facade ossia_state;

  using tick = halp::tick_flicks;
  void operator()(const tick& tk)
  {
    GenericMathMapping<State>::run_polyphonic(
        inputs.sz.value, outputs.port.call, inputs.expr.value, tk, state);
  }

  struct ui
  {
    halp_meta(layout, halp::layouts::vbox)

    struct : halp::control<&ins::expr>
    {
      halp_flag(dynamic_size);
    } expr;
    halp::control<&ins::sz> sz;
  };
};
}
