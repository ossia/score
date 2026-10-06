#pragma once
#include <ossia/network/value/value.hpp>

#include <boost/circular_buffer.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/value_types.hpp>

#include <optional>
#include <string_view>
#include <utility>

namespace avnd_tools
{

struct Queue
{
  halp_meta(name, "Buffer queue")
  halp_meta(author, "ossia team")
  halp_meta(category, "Control/Data Processing")
  halp_meta(description, "Queue input messages and output them as a buffer")
  halp_meta(c_name, "avnd_buffer_queue")
  halp_meta(uuid, "8f68b81e-e5ba-4a10-a888-6581a5d770fe")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/buffer-queue.html")

  // The combo boxes of Mode and Data list their labels in enumerator order.
  // When the output is sent; Bang sends in every mode.
  //  OnInput*: the ticks a message arrives, even if Lock or Clear drops it.
  //  OnChange*: the ticks the queue's contents change (a message queued, a
  //  clear); the removal done by Pop does not count.
  //  OnBangPopOldest: each output then removes the oldest value, whatever
  //  Data is (with Pop, what it sent).
  enum OutputMode
  {
    EveryTick,
    OnInput,
    OnChange,
    WhenFull,
    OnInputWhenFull,
    OnChangeWhenFull,
    OnBang,
    OnBangPopOldest,
  };
  enum OutputData
  {
    Oldest,
    Newest,
    WholeBuffer,
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
    // Held: the queue is cleared and the input dropped. An impulse (a
    // message, a cable) is a press lasting one tick: it clears once.
    halp::maintained_button<"Clear"> clear;
    halp::maintained_button<"Lock"> lock;
    struct : halp::combobox_t<"Mode", OutputMode>
    {
      struct range
      {
        std::string_view values[8]{
            "Every tick",
            "On input",
            "On change",
            "When full",
            "On input, when full",
            "On change, when full",
            "On bang",
            "On bang, pop oldest"};
        OutputMode init{EveryTick};
      };
    } mode;
    struct : halp::combobox_t<"Data", OutputData>
    {
      struct range
      {
        std::string_view values[3]{"Oldest", "Newest", "Whole buffer"};
        OutputData init{Oldest};
      };
    } data;
    // Like Counter's "Output": sends the output now, whatever the mode.
    struct : halp::impulse_button<"Bang">
    {
      void update(Queue& self) { self.banged = true; }
    } bang;
    // Each output removes what it sent: the oldest value, the newest, or the
    // whole queue (with WhenFull or OnChangeWhenFull: chunks of Max length).
    halp::toggle<"Pop"> pop;
  } inputs;

  struct
  {
    halp::val_port<"Output", std::optional<ossia::value>> output;
  } outputs;

  boost::circular_buffer<ossia::value> buffer;
  bool banged{};

  bool triggered(bool received, bool changed) const noexcept
  {
    const bool full = buffer.full();
    switch(inputs.mode)
    {
      case OutputMode::EveryTick:
        return true;
      case OutputMode::OnInput:
        return received;
      case OutputMode::OnChange:
        return changed;
      case OutputMode::WhenFull:
        return full;
      case OutputMode::OnInputWhenFull:
        return received && full;
      case OutputMode::OnChangeWhenFull:
        return changed && full;
      case OutputMode::OnBang:
      case OutputMode::OnBangPopOldest:
        return false;
    }
    return false;
  }

  void send()
  {
    if(buffer.empty())
    {
      if(inputs.data == OutputData::WholeBuffer)
        outputs.output.value = std::vector<ossia::value>{};
      else
        outputs.output.value = ossia::value{};
      return;
    }

    switch(inputs.data)
    {
      case OutputData::Oldest:
        outputs.output.value = buffer.front();
        if(inputs.pop || inputs.mode == OutputMode::OnBangPopOldest)
          buffer.pop_front();
        break;
      case OutputData::WholeBuffer:
        outputs.output.value = std::vector<ossia::value>(buffer.begin(), buffer.end());
        if(inputs.pop)
          buffer.clear();
        else if(inputs.mode == OutputMode::OnBangPopOldest)
          buffer.pop_front();
        break;
      case OutputData::Newest:
        outputs.output.value = buffer.back();
        if(inputs.pop)
          buffer.pop_back();
        else if(inputs.mode == OutputMode::OnBangPopOldest)
          buffer.pop_front();
        break;
    }
  }

  void operator()()
  {
    const bool bang = std::exchange(banged, false);
    outputs.output.value.reset();

    const bool received = bool(inputs.input.value);
    bool changed = false;
    if(received)
    {
      if(!inputs.lock && !inputs.clear && buffer.capacity() > 0)
      {
        buffer.push_back(std::move(*inputs.input.value));
        changed = true;
      }
      inputs.input.value.reset();
    }

    if(inputs.clear && !buffer.empty())
    {
      buffer.clear();
      changed = true;
    }

    if(bang || triggered(received, changed))
      send();
  }
};
}
