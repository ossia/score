#pragma once

#include <Fx/MathMapping_generic.hpp>
#include <Fx/Types.hpp>

#include <ossia/dataflow/audio_port.hpp>
#include <ossia/dataflow/value_port.hpp>

#include <halp/layout.hpp>

namespace Nodes
{
namespace MathAudioGenerator
{
struct Node
{
  halp_meta(name, "Expression Audio Generator")
  halp_meta(c_name, "MathAudioGenerator")
  halp_meta(category, "Audio/Utilities")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/exprtk.html#exprtk-support")
  halp_meta(author, "ossia score, ExprTK (Arash Partow)")
  halp_meta(
      description,
      "Generate an audio signal from a math expression.\n"
      "Available variables: a,b,c, t (samples), fs (sampling frequency)\n"
      "See the documentation at http://www.partow.net/programming/exprtk")
  halp_meta(uuid, "eae294b3-afeb-4fba-bbe4-337998d3748a")

  using code_writer = Nodes::MathMappingCodeWriter;

  struct ins
  {
    halp::lineedit<
        "Expression",
        "var phi := 2 * pi * (20 + a * 500) / fs;\n"
        "m1[0] += phi;\n"
        "\n"
        "out[0] := b * cos(m1[0]);\n"
        "out[1] := b * cos(m1[0]);\n">
        expr;

    halp::hslider_f32<"Param (a)", halp::range{0., 1., 0.5}> a;
    halp::hslider_f32<"Param (b)", halp::range{0., 1., 0.5}> b;
    halp::hslider_f32<"Param (c)", halp::range{0., 1., 0.5}> c;
  } inputs;

  struct
  {
    halp::variable_audio_bus<"out", double> audio;
  } outputs;

  struct State
  {
    State()
    {
      cur_out.reserve(8);
      m1.reserve(8);
      m2.reserve(8);
      m3.reserve(8);
      cur_out.resize(2);
      m1.resize(2);
      m2.resize(2);
      m3.resize(2);

      expr.add_vector("out", cur_out);
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
      // TODO allow to set how many channels
      if(N == cur_out.size())
        return;

      expr.remove_vector("out");
      expr.remove_vector("m1");
      expr.remove_vector("m2");
      expr.remove_vector("m3");

      cur_out.resize(N);
      m1.resize(N);
      m2.resize(N);
      m3.resize(N);

      expr.add_vector("out", cur_out);
      expr.add_vector("m1", m1);
      expr.add_vector("m2", m2);
      expr.add_vector("m3", m3);

      expr.update_symbol_table();
    }
    std::vector<double> cur_out{};
    double cur_time{};
    double a{}, b{}, c{};
    double pa{}, pb{}, pc{};
    std::vector<double> m1, m2, m3;
    double fs{44100};
    ossia::math_expression expr;
    bool ok = false;
  } state;

  halp::setup setup;
  static constexpr int chans = 2; // FIXME
  void prepare(halp::setup s)
  {
    setup = s;
    outputs.audio.request_channels(chans);
    state.reset_symbols(chans);
  }

  using tick = halp::tick_flicks;
  void operator()(const tick& tk)
  {
    SCORE_ASSERT(outputs.audio.channels == 2);
    auto& self = state;
    // if(tk.forward())
    {
      self.fs = setup.rate;
      if(!self.expr.set_expression(inputs.expr))
        return;

      self.reset_symbols(chans);
      self.a = this->inputs.a;
      self.b = this->inputs.b;
      self.c = this->inputs.c;
      for(int64_t i = 0; i < tk.frames; i++)
      {
        self.cur_time = i;

        // Compute the value
        self.expr.value();

        // Apply the output
        auto& channels = this->outputs.audio.samples;
        for(int j = 0; j < chans; j++)
        {
          channels[j][i] = self.cur_out[j];
        }
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
}
