#pragma once
#include <boost/circular_buffer.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <ossia/network/value/value.hpp>


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
    WhenFull
  };
  enum OutputData
  {
    SingleValue,
    WholeBuffer
  };
  struct
  {
    halp::val_port<"Input", ossia::value> input;
    struct : halp::spinbox_i32<"Max length", halp::range{0, 100000, 100}>
    {
      void update(Queue& self)
      {
        if(this->value < 0)
          return;
        self.buffer.set_capacity(this->value);
      }
    } length;
    halp::maintained_button<"Clear"> clear;
    halp::maintained_button<"Lock"> lock;
    halp::enum_t<OutputMode, "Mode"> mode;
    halp::enum_t<OutputData, "Data"> data;
  } inputs;

  struct
  {
    halp::val_port<"Output", ossia::value> output;
  } outputs;

  boost::circular_buffer<ossia::value> buffer;

  void operator()()
  {
    if(!inputs.input.value.valid())
      return;

    if(!inputs.lock && !inputs.clear)
      buffer.push_back(std::move(inputs.input.value));

    if(inputs.clear)
      buffer.clear();

    if(inputs.mode == OutputMode::WhenFull)
      if(buffer.size() < buffer.capacity())
        return;

    if(buffer.empty())
    {
      if(inputs.data == OutputData::WholeBuffer)
      {
        outputs.output.value = std::vector<ossia::value>{};
        return;
      }
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
  }
};
}
