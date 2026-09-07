#pragma once
#include <Fx/Types.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/dataflow/value_port.hpp>

#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>

#include <cmath>

#include <algorithm>
#include <optional>

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
  halp_meta(
      description,
      "Limit and quantize a value stream, or debounce after quiet milliseconds "
      "(debounce ignores quantization)")
  halp_meta(uuid, "76cfd504-7c10-4bdb-a1b4-fbe449cc06f0")

  enum class Mode
  {
    Limit,
    Debounce
  };

  struct ins
  {
    ossia_port<"in", ossia::value_port> port{};
    quant_selector<"Quantization"> quantification; // FIXME use proper time widget
    struct : halp::hslider_i32<"Duration", halp::irange{0, 1000, 10}>
    {
      halp_meta(
          description,
          "Minimum interval in milliseconds, or quiet time before a debounced value")
    } ms;
    struct : halp::enum_t<Mode, "Mode">
    {
      void update(Node& self)
      {
        if(self.previous_mode != this->value)
          self.reset();
      }
    } mode;
  } inputs;
  struct
  {
    halp::timed_callback<"out", ossia::value> out;
  } outputs;

  // Raw value ports carry buffer-relative timestamps, whereas timed callbacks
  // take tick-relative timestamps. The native token retains the slice offset
  // that tick_flicks cannot represent.
  using tick = ossia::token_request;
  ossia::exec_state_facade ossia_state;

  static constexpr int64_t flicks_per_ms = 705'600;
  int64_t last_time{};
  std::optional<ossia::value> pending;
  long double deadline{};
  std::optional<int64_t> previous_end;
  Mode previous_mode{Mode::Limit};
  ossia::small_vector<const ossia::timed_value*, 16> ordered_events;

  void reset() noexcept
  {
    pending.reset();
    previous_end.reset();
    last_time = 0;
    previous_mode = inputs.mode.value;
  }

  void prepare(halp::setup) noexcept { reset(); }
  void start() noexcept { reset(); }
  void stop() noexcept { reset(); }
  void transport(auto) noexcept { reset(); }

  bool should_output(float quantif, int64_t ms, const tick& t)
  {
    if(quantif != 0.)
      return true;
    else if(t.date.impl >= (last_time + ms * flicks_per_ms))
    {
      last_time = t.date.impl;
      return true;
    }
    return false;
  }

  void operator()(const tick& t)
  {
    if(previous_mode != inputs.mode.value
       || (previous_end && t.prev_date.impl != *previous_end))
      reset();
    previous_end = t.date.impl;

    const auto [start, frames] = ossia_state.timings(t);
    if(inputs.mode.value == Mode::Debounce)
    {
      // A rewind or stopped transport cannot carry a forward-time deadline.
      if(t.date <= t.prev_date || t.speed <= 0.)
      {
        pending.reset();
        return;
      }
      if(frames <= 0)
        return;
      debounce(t, start, frames);
      return;
    }

    auto quantif = inputs.quantification.value;
    auto ms = inputs.ms.value;
    for(const ossia::timed_value& v : inputs.port.value->get_data())
    {
      if(v.timestamp < start || v.timestamp >= start + frames)
        continue;
      if(quantif <= 0.)
      {
        if(should_output(quantif, ms, t))
          outputs.out(v.timestamp - start, v.value);
      }
      else
      {
        for(const auto& point : t.get_quantification_dates(1. / quantif))
        {
          outputs.out(
              t.physical_position(point.position, ossia_state.modelToSamples()),
              v.value);
          break;
        }
      }
    }
  }

  void debounce(const tick& t, int64_t start, int64_t frames)
  {
    // Milliseconds measure model time (705600 flicks/ms), independent of the
    // musical grid. Map through the carried sample span, including fractional
    // flicks, so non-unit transport speeds and partial buffers stay accurate.
    const long double begin = t.prev_date.impl;
    const long double duration = static_cast<long double>(t.date.impl) - begin;
    const int64_t delay = std::max(0, inputs.ms.value) * flicks_per_ms;
    const auto emit = [&] {
      // Never emit before the quiet interval has elapsed: round up to the first
      // eligible sample. The tick is half-open; its end belongs to the next.
      const auto frame
          = std::max(0.L, std::ceil((deadline - begin) * frames / duration));
      if(frame < frames)
      {
        outputs.out(static_cast<int64_t>(frame), std::move(*pending));
        pending.reset();
      }
    };
    const auto event = [&](const ossia::timed_value& v) {
      if(v.timestamp < start || v.timestamp >= start + frames)
        return;
      const auto date = begin + (v.timestamp - start) * duration / frames;
      // An arrival exactly at the deadline wins, extending the same burst.
      // Zero delay is immediate for every event, including equal timestamps.
      if(pending && deadline < date)
        emit();
      if(delay == 0)
      {
        pending.reset();
        outputs.out(v.timestamp - start, v.value);
      }
      else
      {
        pending = v.value;
        deadline = date + delay;
      }
    };

    const auto& data = inputs.port.value->get_data();
    if(std::is_sorted(data.begin(), data.end(), [](const auto& a, const auto& b) {
      return a.timestamp < b.timestamp;
    }))
    {
      for(const auto& v : data)
        event(v);
    }
    else
    {
      // Fan-in may append independently sorted streams. Sort pointers, not
      // arbitrary values, retaining arrival order for equal timestamps.
      ordered_events.clear();
      for(const auto& v : data)
        ordered_events.push_back(&v);
      std::sort(
          ordered_events.begin(), ordered_events.end(),
          [](const auto* a, const auto* b) {
        return a->timestamp < b->timestamp || (a->timestamp == b->timestamp && a < b);
      });
      for(const auto* v : ordered_events)
        event(*v);
    }
    if(pending && deadline < t.date.impl)
      emit();
  }

  struct ui
  {
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::background_mid)
    halp::control<&ins::quantification> q;
    halp::control<&ins::ms> t;
    halp::control<&ins::mode> mode;
  };
};
}
