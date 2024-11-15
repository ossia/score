#pragma once

#include <Fx/MathMapping_generic.hpp>
#include <Fx/Types.hpp>

#include <ossia/dataflow/value_port.hpp>

#include <halp/layout.hpp>

namespace Nodes::MathAudioFilter
{
struct Node
{
  halp_meta(name, "Expression Audio Filter")
  halp_meta(c_name, "MathAudioFilter")
  halp_meta(category, "Audio/Utilities")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/exprtk.html#exprtk-support")
  halp_meta(author, "ossia score, ExprTK (Arash Partow)")
  static const constexpr auto description
      = "Applies a math expression to an audio input.\n"
        "Available variables: a,b,c, t (samples), fs (sampling frequency), "
        "\n"
        "x (value), px (previous value)\n"
        "See the documentation at http://www.partow.net/programming/exprtk";
  halp_meta(uuid, "13e1f4b0-1c2c-40e6-93ad-dfc91aac5335")

  using code_writer = Nodes::MathMappingCodeWriter;

  struct ins
  {
    halp::dynamic_audio_bus<"in", double> audio;
    halp::lineedit<
        "Expression",
        "var n := x[];\n"
        "\n"
        "for (var i := 0; i < n; i += 1) {\n"
        "  var dist := tan(x[i]*log(1 + 200 * a));\n"
        "  out[i] := clamp(-1, dist, 1);\n"
        "}\n">
        expr;
    halp::hslider_f32<"Param (a)", halp::range{0., 1., 0.5}> a;
    halp::hslider_f32<"Param (b)", halp::range{0., 1., 0.5}> b;
    halp::hslider_f32<"Param (c)", halp::range{0., 1., 0.5}> c;
  } inputs;

  struct
  {
    halp::mimic_audio_bus<"out", &decltype(inputs)::audio> audio;
  } outputs;

  struct State
  {
    State()
    {
      cur_in.reserve(8);
      cur_out.reserve(8);
      prev_in.reserve(8);
      m1.reserve(8);
      m2.reserve(8);
      m3.reserve(8);
      cur_in.resize(2);
      cur_out.resize(2);
      prev_in.resize(2);
      m1.resize(2);
      m2.resize(2);
      m3.resize(2);

      expr.add_vector("x", cur_in);
      expr.add_vector("out", cur_out);
      expr.add_vector("px", prev_in);
      expr.add_variable("t", cur_time);
      expr.add_variable("a", a);
      expr.add_variable("b", b);
      expr.add_variable("c", c);
      expr.add_variable("pa", pa);
      expr.add_variable("pb", pb);
      expr.add_variable("pc", pc);
      expr.add_vector("m1", m1);
      expr.add_vector("m2", m2);
      expr.add_vector("m3", m3);
      expr.add_variable("fs", fs);
      expr.add_constants();

      expr.register_symbol_table();
    }

    void reset_symbols(std::size_t N)
    {
      if(N == cur_in.size())
        return;

      expr.remove_vector("x");
      expr.remove_vector("out");
      expr.remove_vector("px");
      expr.remove_vector("m1");
      expr.remove_vector("m2");
      expr.remove_vector("m3");

      cur_in.resize(N);
      cur_out.resize(N);
      prev_in.resize(N);
      m1.resize(N);
      m2.resize(N);
      m3.resize(N);

      expr.add_vector("x", cur_in);
      expr.add_vector("out", cur_out);
      expr.add_vector("px", prev_in);
      expr.add_vector("m1", m1);
      expr.add_vector("m2", m2);
      expr.add_vector("m3", m3);

      expr.update_symbol_table();
    }

    std::vector<double> cur_in{};
    std::vector<double> cur_out{};
    std::vector<double> prev_in{};
    double cur_time{};
    double a{}, b{}, c{};
    double pa{}, pb{}, pc{};
    std::vector<double> m1, m2, m3;
    double fs{44100};
    ossia::math_expression expr;
    bool ok = false;
  } state;

  halp::setup setup;
  void prepare(halp::setup s) { setup = s; }

  using tick = halp::tick_flicks;
  void operator()(const tick& tk)
  {
    SCORE_ASSERT(outputs.audio.channels == 2);
    auto& self = state;
    //if(tk.date > tk.prev_date)
    {
      self.fs = setup.rate;
      if(!self.expr.set_expression(inputs.expr))
        return;

      if(inputs.audio.channels == 0)
        return;

      const int chans = inputs.audio.channels;
      self.reset_symbols(chans);

      self.a = this->inputs.a;
      self.b = this->inputs.b;
      self.c = this->inputs.c;
      for(int64_t i = 0; i < tk.frames; i++)
      {
        for(int j = 0; j < chans; j++)
        {
          self.cur_in[j] = inputs.audio.samples[j][i];
        }
        self.cur_time = i;

        // Compute the value
        self.expr.value();

        // Apply the output
        auto& channels = this->outputs.audio.samples;
        for(int j = 0; j < chans; j++)
        {
          channels[j][i] = self.cur_out[j];
        }
        std::swap(self.cur_in, self.prev_in);
      }
      self.pa = self.a;
      self.pb = self.b;
      self.pc = self.c;
    }
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
