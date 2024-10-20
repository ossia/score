#pragma once
#include <Fx/Types.hpp>

#include <ossia/dataflow/value_port.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/network/value/format_value.hpp>

#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>

namespace Nodes::RateLimiter
{
struct Node
{
  halp_meta(name, "Rate Limiter")
  halp_meta(c_name, "RateLimiter")
  halp_meta(category, "Control/Mappings")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/rate-limiter.html#rate-limiter")
  halp_meta(author, "ossia score")
  halp_meta(description, "Limit and quantize the rate of a value stream")
  halp_meta(uuid, "76cfd504-7c10-4bdb-a1b4-fbe449cc06f0")

  struct ins
  {
    // FIXME all incorrect when token_request smaller than tick
    ossia_port<"in", ossia::value_port> port{};
    quant_selector<"Quantization"> quantification; // FIXME use proper time widget
    halp::hslider_i32<"ms.", halp::irange{0, 1000, 10}> ms;
  } inputs;
  struct
  {
    halp::timed_callback<"out", ossia::value> out;
  } outputs;

  using tick = halp::tick_flicks;
  bool should_output(float quantif, int64_t ms, tick t)
  {
    if(quantif != 0.)
      return true;
    else if(t.end_in_flicks >= (last_time + ms * flicks_per_ms))
    {
      last_time = t.end_in_flicks;
      return true;
    }
    return false;
  }

  static const constexpr int64_t flicks_per_ms = 705'600;
  int64_t last_time{};

  void operator()(const tick& t)
  {
    auto& in = *inputs.port.value;
    auto quantif = inputs.quantification.value;
    auto ms = inputs.ms.value;
    for(const ossia::timed_value& v : in.get_data())
    {
      if(quantif <= 0.)
      {
        if(should_output(quantif, ms, t))
          outputs.out(v.timestamp, v.value); // FIXME fix accuracy of timestamp
      }
      else
      {
        for(auto [t, q] : t.get_quantification_date(1. / quantif))
        {
          outputs.out(t, v.value);
          break;
        }
      }
    }
  }

  struct ui
  {
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::mid)
    halp::control<&ins::quantification> q;
    halp::control<&ins::ms> t;
  };
};
}
