#pragma once

#include <ossia/dataflow/value_port.hpp>
#include <ossia/network/value/value.hpp>

#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/dynamic_port.hpp>
#include <halp/meta.hpp>

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace avnd_tools
{
struct Rendezvous
{
  halp_meta(name, "Rendezvous")
  halp_meta(c_name, "avnd_rendezvous")
  halp_meta(author, "ossia team")
  halp_meta(category, "Control/Mappings")
  halp_meta(description, "Emit an ordered list when every input has a fresh event")
  halp_meta(uuid, "ec5529c6-4c69-47bd-9eb7-3bfbc3cc5ccf")

  // Retain native events so First also means the first of multiple arrivals
  // within one processing tick, including events with equal timestamps.
  struct Input
  {
    halp_meta(name, "Input {}")
    ossia::value_port* value{};
    static constexpr bool is_event() { return true; }
  };

  struct
  {
    struct : halp::spinbox_i32<"Input count", halp::range{0, 512, 2}>
    {
      static std::function<void(Rendezvous&, int)> on_controller_interaction()
      {
        return [](Rendezvous& object, int value) {
          object.inputs.in_i.request_port_resize(std::clamp(value, 0, 512));
        };
      }

      void update(Rendezvous& object)
      {
        const int count = std::clamp(this->value, 0, 512);
        if(count != object.requested_count)
        {
          object.requested_count = count;
          object.clear();
        }
      }
    } controller;

    struct : halp::toggle<"Keep first">
    {
      halp_meta(
          description,
          "Keep the first fresh value on each input until the cycle completes. "
          "When disabled, keep the latest value. Changes affect future arrivals.")
    } keep_first;

    halp::dynamic_port<Input> in_i;
    halp::impulse_button<"Clear"> clear;
  } inputs;

  struct
  {
    halp::callback<"Output", const std::vector<ossia::value>&> out;
    // Number of inlets still missing a fresh event, not a count of messages.
    halp::val_port<"Waiting", int> waiting;
  } outputs;

  void operator()()
  {
    const auto count = inputs.in_i.ports.size();
    if(pending.size() != count)
    {
      pending.resize(count);
      received.resize(count);
      reset_cycle();
    }

    // Clear wins over arrivals in the same tick and is consumed only here.
    if(inputs.clear.value)
    {
      inputs.clear.value.reset();
      clear();
      return;
    }

    for(std::size_t i = 0; i < count; ++i)
    {
      const auto* input = inputs.in_i.ports[i].value;
      if(!input || input->get_data().empty())
        continue;
      if(!received[i] || !inputs.keep_first.value)
      {
        const auto& arrivals = input->get_data();
        pending[i]
            = inputs.keep_first.value ? arrivals.front().value : arrivals.back().value;
      }
      if(!received[i])
      {
        received[i] = true;
        ++ready_count;
      }
    }

    outputs.waiting.value = static_cast<int>(count - ready_count);
    if(count != 0 && ready_count == count)
    {
      // Publish during processing, never from a UI/update callback, because
      // the host clears graph output ports before invoking operator().
      outputs.out(pending);
      reset_cycle();
    }
  }

private:
  int requested_count = 2;
  std::vector<ossia::value> pending;
  std::vector<bool> received;
  std::size_t ready_count{};

  void reset_cycle()
  {
    for(auto& value : pending)
      value = ossia::value{};
    std::fill(received.begin(), received.end(), false);
    ready_count = 0;
    outputs.waiting.value = static_cast<int>(inputs.in_i.ports.size());
  }

  void clear() { reset_cycle(); }
};
}
