#pragma once
#include <Engine/Node/PdNode.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <exprtk.hpp>
#include <numeric>
#include <ossia/detail/logger.hpp>
namespace Nodes
{
template<typename State>
bool updateExpr(State& self, const std::string& expr)
{
  if(expr != self.cur_expr_txt)
  {
    self.cur_expr_txt = expr;
    self.ok = self.parser.compile(self.cur_expr_txt, self.expr);
    if(!self.ok)
    {
      ossia::logger().error("Error while parsing: {}", self.parser.error());
    }
  }

  return self.ok;
}

namespace MathGenerator
{
struct Node
{
    struct Metadata
    {
        static const constexpr auto prettyName = "Value Generator";
        static const constexpr auto objectKey = "MathGenerator";
        static const constexpr auto category = "Control";
        static const constexpr auto tags = std::array<const char*, 0>{};
        static const constexpr auto uuid = make_uuid("d757bd0d-c0a1-4aec-bf72-945b722ab85b");
    };
    struct State
    {
        State()
        {
          syms.add_variable("t", cur_time);
          syms.add_variable("dt", cur_deltatime);
          syms.add_variable("pos", cur_pos);
          syms.add_constants();

          expr.register_symbol_table(syms);
        }
        double cur_time{};
        double cur_deltatime{};
        double cur_pos{};
        exprtk::symbol_table<double> syms;
        exprtk::expression<double> expr;
        exprtk::parser<double> parser;
        std::string cur_expr_txt;
        bool ok = false;
    };

    static const constexpr auto info =
            Control::create_node()
            .value_outs({{"out"}})
            .controls(Control::LineEdit("Expression (ExprTK)", "cos(t) + log(pos * 1 / dt)"))
            .build();

    using control_policy = Control::LastTick;
    static void run(
            const std::string& expr,
            ossia::value_port& output,
            ossia::time_value prev_date,
            ossia::token_request tk,
            ossia::execution_state& st,
            State& self)
    {
      if(!updateExpr(self, expr))
        return;

      self.cur_time = tk.date.impl;
      self.cur_deltatime = tk.date.impl - prev_date.impl;
      self.cur_pos = tk.position;

      auto res = self.expr.value();
      output.add_value(res, tk.offset);
    }
};
}


namespace MathAudioGenerator
{
struct Node
{
    struct Metadata
    {
        static const constexpr auto prettyName = "Audio Generator";
        static const constexpr auto objectKey = "MathAudioGenerator";
        static const constexpr auto category = "Audio";
        static const constexpr auto tags = std::array<const char*, 0>{};
        static const constexpr auto uuid = make_uuid("eae294b3-afeb-4fba-bbe4-337998d3748a");
    };

    struct State
    {
        State()
        {
          syms.add_variable("t", cur_time);
          syms.add_variable("a", p1);
          syms.add_variable("b", p2);
          syms.add_variable("c", p3);
          syms.add_variable("fs", fs);
          syms.add_constants();

          expr.register_symbol_table(syms);
        }
        double cur_time{};
        double p1{}, p2{}, p3{};
        double fs{44100};
        exprtk::symbol_table<double> syms;
        exprtk::expression<double> expr;
        exprtk::parser<double> parser;
        std::string cur_expr_txt;
        bool ok = false;
    };

    static const constexpr auto info =
            Control::create_node()
            .audio_outs({{"out"}})
            .controls(Control::LineEdit("Expression (ExprTK)", "a * cos( 2 * pi * t * b / fs )")
                      , Control::FloatSlider("Param (a)", 0., 1., 0.5)
                      , Control::FloatSlider("Param (b)", 0., 1., 0.5)
                      , Control::FloatSlider("Param (c)", 0., 1., 0.5)
                      )
            .build();

    using control_policy = Control::LastTick;
    static void run(
            const std::string& expr,
            float a, float b, float c,
            ossia::audio_port& output,
            ossia::time_value prev_date,
            ossia::token_request tk,
            ossia::execution_state& st,
            State& self)
    {
      if(tk.date > prev_date)
      {
        auto count = tk.date - prev_date;
        if(!updateExpr(self, expr))
          return;

        output.samples.resize(1);
        auto& cur = output.samples[0];
        if(cur.size() < tk.offset + count)
          cur.resize(tk.offset + count);

        self.p1 = a;
        self.p2 = b;
        self.p3 = c;
        self.fs = st.sampleRate;
        for(int64_t i = 0; i < count; i++)
        {
          self.cur_time = prev_date + i;
          cur[tk.offset + i] = self.expr.value();
        }
      }
    }
};
}
}
