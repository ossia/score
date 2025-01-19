#pragma once
#include <Fx/NoiseFilter.hpp>
#include <Fx/Types.hpp>

#include <ossia/dataflow/value_port.hpp>
#include <ossia/detail/logger.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace Nodes::Smooth
{
using namespace dno;

namespace v1
{
struct Node : NoiseState
{
  halp_meta(name, "Smooth (old)")
  halp_meta(c_name, "ValueFilter")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/smooth.html#smooth")
  halp_meta(description, "Filter noisy value stream")
  halp_meta(uuid, "809c014d-7d02-45dc-8849-de7a7db5fe67")
  halp_flag(deprecated);

  struct
  {
    // FIXME all incorrect when token_request smaller than tick
    struct : halp::val_port<"in", ossia::value>
    {
      // Messages to this port trigger a new computation cycle with updated timestamps
      halp_flag(active_port);
      void update(Node& self) { self.trigger = true; }
    } port;
    halp::enum_t<dno::type, "Type"> type;
    halp::knob_f32<"Amount", halp::range{0., 1., 0.1}> amount;
    halp::log_hslider_f32<"Freq (1e/LP)", halp::range{0.001, 300., 120.}> freq;
    halp::log_hslider_f32<"Cutoff (1e/LP)", halp::range{0.001, 10., 1.}> cutoff;
    halp::hslider_f32<"Beta (1e only)", halp::range{0.001, 10., 1.}> beta;
  } inputs;
  struct
  {
    value_out port{};
  } outputs;
  bool trigger{false};

  void operator()()
  {
    if(!std::exchange(trigger, false))
      return;
    auto& v = this->inputs.port.value;

    auto filtered = this->filter(
        v, inputs.type, inputs.amount, inputs.freq, inputs.cutoff, inputs.beta);
    outputs.port(std::move(filtered)); // TODO fix accuracy of timestamp

    this->last = v;
  }
};
}
}
