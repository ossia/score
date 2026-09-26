#pragma once
#include <ossia/network/value/value.hpp>

#include <boost/circular_buffer.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/value_types.hpp>

#include <optional>
#include <utility>

namespace avnd_tools
{

struct Queue
{
  halp_meta(name, "Buffer queue")
  halp_meta(author, "ossia team")
  halp_meta(category, "Control/Data processing")
  halp_meta(description, "Queue input messages and output them as a buffer")
  halp_meta(c_name, "avnd_buffer_queue")
  halp_meta(uuid, "8f68b81e-e5ba-4a10-a888-6581a5d770fe")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/buffer-queue.html")

  enum OutputMode
  {
    Always,
    WhenFull,
    ManualBang,
    ManualPop,
  };
  enum OutputData
  {
    SingleValue,
    WholeBuffer
  };
  struct
  {
    // An event port: only the messages received this tick are queued.
    halp::val_port<"Input", std::optional<ossia::value>> input;
    struct : halp::spinbox_i32<"Max length", halp::range{0, 100000, 100}>
    {
      void update(Queue& self)
      {
        if(this->value < 0)
          return;
        self.buffer.set_capacity(this->value);
      }
    } length;
    // An impulse, from the button or a message: a maintained button read an
    // incoming impulse as false and never cleared.
    struct : halp::impulse_button<"Clear">
    {
      void update(Queue& self) { self.buffer.clear(); }
    } clear;
    halp::maintained_button<"Lock"> lock;
    halp::enum_t<OutputMode, "Mode"> mode;
    halp::enum_t<OutputData, "Data"> data;
    // Like Counter's "Output": sends the output now, whatever the mode (and
    // pops in the manual pop mode).
    struct : halp::impulse_button<"Bang">
    {
      void update(Queue& self) { self.banged = true; }
    } bang;
  } inputs;

  struct
  {
    // Optional: in the manual and "when full" modes the last output must not
    // be sent again at every tick.
    halp::val_port<"Output", std::optional<ossia::value>> output;
  } outputs;

  boost::circular_buffer<ossia::value> buffer;
  bool banged{};

  void operator()()
  {
    const bool bang = std::exchange(banged, false);
    outputs.output.value.reset();
    // Sent again only when it changed: a whole buffer of up to 100000 values
    // is not copied out at every tick.
    bool changed = false;
    if(inputs.input.value)
    {
      if(!inputs.lock)
        buffer.push_back(std::move(*inputs.input.value));
        changed = true;
      }
      inputs.input.value.reset();
    }

    switch(inputs.mode)
    {
      case OutputMode::Always:
        if(!bang && !changed)
          return;
        break;
      case OutputMode::WhenFull:
        if(!bang && (!changed || buffer.size() < buffer.capacity()))
          return;
        break;
      case OutputMode::ManualBang:
      case OutputMode::ManualPop:
        if(!bang)
          return;
        break;
    }

    if(buffer.empty())
    {
      if(inputs.data == OutputData::WholeBuffer)
      {
        outputs.output.value = std::vector<ossia::value>{};
      }
      else
      {
        outputs.output.value = ossia::value{};
      }
      return;
    }

    switch(inputs.data)
    {
      case OutputData::SingleValue:
        outputs.output.value = buffer.front();
        break;
      case OutputData::WholeBuffer: {
        outputs.output.value = std::vector<ossia::value>(buffer.begin(), buffer.end());
        break;
      }
    }

    if(inputs.mode == OutputMode::ManualPop)
    {
      buffer.pop_front();
    }
  }
};
}
