#pragma once
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/detail/flicks.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <vector>

namespace Media::Step
{
/**
 * @brief Plays the steps of one of several sequences.
 *
 * A step lasts `duration` seconds, or when synced a fraction of a whole note,
 * so that it follows the tempo. Either way the steps are counted from the
 * start of the process: its first step plays as it starts, wherever that falls
 * on the bars. A sequence asked for on
 * `sequence_select` starts at its first step at the next point of the
 * `switch_quantification` grid (Patternist's convention: 0 is at once, 1 a
 * bar, above 1 a fraction of a whole note).
 */
class step_node final : public ossia::nonowning_graph_node
{
public:
  ossia::value_inlet sequence_select;
  ossia::value_inlet switch_quantification;
  ossia::value_inlet duration;
  ossia::value_outlet out;

  //! The values to send, already mapped to [min; max]. The whole list, so that
  //! a switch moves an index rather than copying, on the audio thread.
  std::vector<ossia::float_vector> sequences;

  int current_sequence{};
  //! Sequence the next quantification point switches to; -1 when none waits.
  int pending_sequence{-1};
  double switch_rate{1.};

  //! Seconds, or a fraction of a whole note when synced.
  double step_duration{0.0625};
  bool synced{true};

  //! The step that plays next.
  int current{};
  //! The step that played last; read by the UI thread.
  std::atomic_int last{-1};

  //! Musical position, in quarter notes, of the start of the process: synced
  //! steps are counted from it.
  double origin{};
  //! Where the next tick starts if time goes on; anything else is a jump.
  int64_t next_date{std::numeric_limits<int64_t>::min()};

  step_node()
  {
    m_inlets.push_back(&sequence_select);
    m_inlets.push_back(&switch_quantification);
    m_inlets.push_back(&duration);
    m_outlets.push_back(&out);
  }

  std::string label() const noexcept override { return "Step"; }

  void request_sequence(int idx) noexcept
  {
    if(sequences.empty())
      return;
    idx = std::clamp(idx, 0, int(sequences.size()) - 1);
    pending_sequence = (idx == current_sequence) ? -1 : idx;
  }

  //! A plain number is seconds; {value, mode}: mode 0 seconds, else synced.
  void set_duration(const ossia::value& v) noexcept
  {
    switch(v.get_type())
    {
      case ossia::val_type::FLOAT:
      case ossia::val_type::INT:
      case ossia::val_type::BOOL:
        step_duration = ossia::convert<float>(v);
        synced = false;
        break;
      default: {
        auto vec = ossia::convert<ossia::vec2f>(v);
        step_duration = vec[0];
        synced = vec[1] != 0.f;
        break;
      }
    }
  }

  void read_controls() noexcept
  {
    if(auto& d = sequence_select->get_data(); !d.empty())
      request_sequence(ossia::convert<int>(d.back().value));
    if(auto& d = switch_quantification->get_data(); !d.empty())
      switch_rate = ossia::convert<float>(d.back().value);
    if(auto& d = duration->get_data(); !d.empty())
      set_duration(d.back().value);
  }

  void run(const ossia::token_request& tk, ossia::exec_state_facade st) noexcept override
  {
    // Before any early return: the inlets are cleared once the node has run.
    read_controls();

    if(tk.paused() || sequences.empty())
      return;
    current_sequence = std::clamp(current_sequence, 0, int(sequences.size()) - 1);
    if(pending_sequence >= int(sequences.size()))
      pending_sequence = -1;

    const double ratio = st.modelToSamples();
    const int64_t tick_start = tk.physical_start(ratio);
    const bool rewinding = tk.backward();

    if(tk.prev_date.impl != next_date)
      follow_jump(tk);
    next_date = tk.date.impl;

    constexpr auto never = std::numeric_limits<int64_t>::max();
    int64_t switch_at = never;
    if(pending_sequence >= 0)
    {
      if(switch_rate <= 0.)
      {
        switch_at = tick_start;
      }
      else
      {
        const auto points = tk.get_quantification_dates(switch_rate);
        if(!points.empty())
          switch_at = tick_start + tk.physical_position(points.front().position, ratio);
      }
    }

    play_steps(tk, ratio, tick_start, rewinding, std::numeric_limits<int64_t>::min(), switch_at);
    if(switch_at != never)
    {
      current_sequence = pending_sequence;
      pending_sequence = -1;
      current = 0;
      play_steps(tk, ratio, tick_start, rewinding, switch_at, never);
    }
  }

  //! Length of a synced step, in quarter notes: as the quantification grid,
  //! a bar or more counts in bars.
  double step_quarters(const ossia::token_request& tk) const noexcept
  {
    const double rate = 1. / step_duration;
    if(rate > 1.)
      return 4. / rate;
    const auto& sig = tk.signature;
    const double bar
        = (sig.upper > 0 && sig.lower > 0) ? 4. * sig.upper / sig.lower : 4.;
    return bar / rate;
  }

  //! The process started, or the transport moved it: count the steps from its
  //! start again, and play on from the step the position falls in.
  void follow_jump(const ossia::token_request& tk) noexcept
  {
    // Quarter notes since the start of the process, at this tick's tempo.
    const double elapsed
        = tk.prev_date.impl / ossia::flicks_per_second<double> * tk.tempo / 60.;
    origin = tk.musical_start_position - elapsed;

    const int n = sequences.empty() ? 0 : int(sequences[current_sequence].size());
    if(n == 0 || !(step_duration > 0.) || !std::isfinite(step_duration))
      return;

    // Steps whose time has come before this tick, the one on its start excluded.
    double before{};
    if(synced)
    {
      const double q = step_quarters(tk);
      if(q > 0.)
        before = std::ceil((tk.musical_start_position - origin) / q - 1e-9);
    }
    else
    {
      const double d = step_duration * ossia::flicks_per_second<double>;
      before = std::ceil(tk.prev_date.impl / d);
    }
    if(std::isfinite(before) && before >= 0.)
      current = int(std::fmod(before, double(n)));
  }

  //! Plays the steps whose sample falls in [from; to[.
  void play_steps(
      const ossia::token_request& tk, double ratio, int64_t tick_start, bool rewinding,
      int64_t from, int64_t to) noexcept
  {
    if(!(step_duration > 0.) || !std::isfinite(step_duration))
      return;

    auto play = [&](int64_t sample) {
      if(sample >= from && sample < to)
        play_step(sample, rewinding);
    };

    if(synced)
    {
      // k steps from the start, in tick order both ways: the musical positions
      // of [start; end[ going forward, ]end; start] rewinding.
      const double q = step_quarters(tk);
      const double ms = tk.musical_start_position;
      const double me = tk.musical_end_position;
      if(!(q > 0.) || ms == me)
        return;
      constexpr double eps = 1e-9;
      int64_t k = rewinding ? int64_t(std::floor((ms - origin) / q + eps))
                            : int64_t(std::ceil((ms - origin) / q - eps));
      // A tick never holds more steps than it has samples.
      for(int guard = 0; guard < 4096; guard++, k += rewinding ? -1 : 1)
      {
        const double m = origin + k * q;
        if(rewinding ? (m <= me + eps) : (m >= me - eps))
          break;
        play(tick_start + tk.physical_position(m, ratio));
      }
    }
    else
    {
      // k * d from the start of the interval.
      const double d = step_duration * ossia::flicks_per_second<double>;
      const int64_t lo = std::min(tk.prev_date.impl, tk.date.impl);
      const int64_t hi = std::max(tk.prev_date.impl, tk.date.impl);
      // [prev; date[ going forward, ]date; prev] rewinding: the same steps
      // are crossed on the way out and on the way back.
      int64_t k0 = rewinding ? int64_t(std::floor(lo / d)) + 1 : int64_t(std::ceil(lo / d));
      int64_t k1 = rewinding ? int64_t(std::floor(hi / d)) : int64_t(std::ceil(hi / d)) - 1;
      // A tick never holds more steps than it has samples.
      k1 = std::min(k1, k0 + 4096);
      if(!rewinding)
      {
        for(int64_t k = k0; k <= k1; k++)
          play(tk.to_physical_time_in_tick(ossia::time_value{int64_t(k * d)}, ratio));
      }
      else
      {
        for(int64_t k = k1; k >= k0; k--)
          play(tk.to_physical_time_in_tick(ossia::time_value{int64_t(k * d)}, ratio));
      }
    }
  }

  void play_step(int64_t sample, bool rewinding) noexcept
  {
    const auto& seq = sequences[current_sequence];
    const int n = seq.size();
    if(n == 0)
      return;
    // Rewinding mirrors going forward: step back first, so going out and back
    // over the same ground crosses the same steps.
    if(rewinding)
      current = (current + n - 1) % n;
    if(current < 0 || current >= n)
      current = 0;
    out->write_value(seq[current], sample);
    last.store(current, std::memory_order_relaxed);
    if(!rewinding)
      current = (current + 1) % n;
  }
};
}
