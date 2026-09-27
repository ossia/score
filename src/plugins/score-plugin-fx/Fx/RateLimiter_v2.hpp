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

namespace Nodes::RateLimiter::v2
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
      "Limit a value stream to one value per interval, on the musical grid when "
      "the interval is synced, or debounce after a quiet interval")
  halp_meta(uuid, "485d39ed-e495-448c-95df-5f2ea905ef5b")

  enum class Mode
  {
    Limit,
    Debounce
  };

  struct ins
  {
    ossia_port<"in", ossia::value_port> port{};
    struct : halp::time_chooser<"Interval", halp::range{0., 10., 0.01}>
    {
      halp_meta(
          description,
          "Minimum interval between two values, the latest one held until it "
          "is over (synced: at most one per note value, on the grid), or quiet "
          "time before a debounced value")
    } interval;
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

  static constexpr double flicks_per_s = 705'600'000.;
  //! Limit: when the last value went out; debounce: when the pending one is due.
  std::optional<long double> last_sent;
  std::optional<ossia::value> pending;
  long double deadline{};
  std::optional<int64_t> previous_end;
  Mode previous_mode{Mode::Limit};
  ossia::small_vector<const ossia::timed_value*, 16> ordered_events;

  void reset() noexcept
  {
    pending.reset();
    previous_end.reset();
    last_sent.reset();
    previous_mode = inputs.mode.value;
  }

  void prepare(halp::setup) noexcept { reset(); }
  void start() noexcept { reset(); }
  void stop() noexcept { reset(); }
  void transport(auto) noexcept { reset(); }

  //! The input values of the tick, in time order.
  template <typename F>
  void for_each_event(int64_t start, int64_t frames, F&& f)
  {
    const auto& data = inputs.port.value->get_data();
    const auto in_tick = [&](const ossia::timed_value& v) {
      return v.timestamp >= start && v.timestamp < start + frames;
    };
    if(std::is_sorted(data.begin(), data.end(), [](const auto& a, const auto& b) {
      return a.timestamp < b.timestamp;
    }))
    {
      for(const auto& v : data)
        if(in_tick(v))
          f(v);
    }
    else
    {
      // Fan-in may append independently sorted streams. Sort pointers, not
      // arbitrary values, retaining arrival order for equal timestamps.
      ordered_events.clear();
      for(const auto& v : data)
        if(in_tick(v))
          ordered_events.push_back(&v);
      std::sort(
          ordered_events.begin(), ordered_events.end(),
          [](const auto* a, const auto* b) {
        return a->timestamp < b->timestamp || (a->timestamp == b->timestamp && a < b);
      });
      for(const auto* v : ordered_events)
        f(*v);
    }
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
      if(frames > 0)
        debounce(t, start, frames);
    }
    else if(frames <= 0)
    {
      return;
    }
    else if(inputs.interval.sync)
    {
      limit_on_grid(t, start, frames);
    }
    else
    {
      limit(t, start, frames);
    }
  }

  //! At most one value per interval of model time. A value that comes too
  //! early is held, the latest one winning, and goes out when the interval is
  //! over: the last value of a burst is never lost.
  void limit(const tick& t, int64_t start, int64_t frames)
  {
    const long double begin = t.prev_date.impl;
    const long double duration = static_cast<long double>(t.date.impl) - begin;
    const long double delay = std::max(0.f, inputs.interval.value) * flicks_per_s;

    // The held value, if its time has come by `date`
    const auto flush = [&](long double date) {
      if(!pending || !last_sent || duration <= 0.)
        return;
      const long double due = *last_sent + delay;
      if(due > date)
        return;
      // The first sample at or after the date; the interval, a float, may
      // land a hair past the sample it means.
      const auto frame
          = std::max(0.L, std::ceil((due - begin) * frames / duration - 1e-3L));
      if(frame >= frames)
        return;
      outputs.out(static_cast<int64_t>(frame), std::move(*pending));
      pending.reset();
      last_sent = std::max(due, begin);
    };

    for_each_event(start, frames, [&](const ossia::timed_value& v) {
      const long double date
          = duration > 0. ? begin + (v.timestamp - start) * duration / frames : begin;
      flush(date);
      if(!last_sent || date >= *last_sent + delay)
      {
        pending.reset();
        last_sent = date;
        outputs.out(v.timestamp - start, v.value);
      }
      else
      {
        pending = v.value;
      }
    });
    flush(t.date.impl);
  }

  //! Synced, the interval is a note value: the latest value goes out on each
  //! point of that grid.
  void limit_on_grid(const tick& t, int64_t start, int64_t frames)
  {
    const double seconds = std::max(0.f, inputs.interval.value);
    // The rate of the musical grid (1 a whole note, 4 a quarter...) for the
    // length of the interval at this tempo.
    const double rate = (seconds > 0. && t.tempo > 0.) ? 240. / (seconds * t.tempo) : 0.;
    const auto points = t.forward() ? t.get_quantification_dates(rate)
                                    : ossia::quantification_points{};
    auto point = points.begin();
    const auto emit_until = [&](int64_t frame) {
      for(; point != points.end(); ++point)
      {
        const int64_t f
            = t.physical_position(point->position, ossia_state.modelToSamples());
        if(f > frame)
          break;
        if(pending)
        {
          outputs.out(f, std::move(*pending));
          pending.reset();
        }
      }
    };
    for_each_event(start, frames, [&](const ossia::timed_value& v) {
      // The grid points before this value send what came before it
      emit_until(v.timestamp - start - 1);
      pending = v.value;
    });
    emit_until(frames);
  }

  void debounce(const tick& t, int64_t start, int64_t frames)
  {
    // The quiet interval is model time (a synced one at the current tempo),
    // not the musical grid. Map through the carried sample span, including fractional
    // flicks, so non-unit transport speeds and partial buffers stay accurate.
    const long double begin = t.prev_date.impl;
    const long double duration = static_cast<long double>(t.date.impl) - begin;
    const int64_t delay = int64_t(std::max(0.f, inputs.interval.value) * flicks_per_s);
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
    for_each_event(start, frames, [&](const ossia::timed_value& v) {
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
    });
    if(pending && deadline < t.date.impl)
      emit();
  }

  struct ui
  {
    halp_meta(layout, halp::layouts::hbox)
    halp_meta(background, halp::colors::background_mid)
    halp::control<&ins::interval> t;
    halp::control<&ins::mode> mode;
  };
};
}
