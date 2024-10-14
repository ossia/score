#pragma once
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
namespace Nodes
{
struct Direction
{
  halp_meta(name, "Angle mapper")
  halp_meta(c_name, "AngleMapper")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "")
  halp_meta(description, "Map the variation of an angle")
  halp_meta(uuid, "9b0e21ba-965a-4aa4-beeb-60cc5128c418")
  halp_flag(time_independent);

  struct
  {
    halp::val_port<"in", float> in;
  } inputs;
  struct
  {
    halp::callback<"out", float> out;
  } outputs;

  std::optional<float> prev_value;
  void operator()()
  {
    // returns -1, 0, 1 to say if we're going backwards, staying equal, or
    // going forward.
    float val = inputs.in.value;
    if(!prev_value)
    {
      prev_value = val;
      return;
    }
    else
    {
      float output;

      if(prev_value < val)
      {
        output = 1.f;
        prev_value = val;
      }
      else if(prev_value > val)
      {
        output = -1.f;
        prev_value = val;
      }
      else
      {
        output = 0.f;
      }

      outputs.out(output);
    }
  }
};
}
