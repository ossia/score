#pragma once
#include <ossia/dataflow/token_request.hpp>
#include <ossia/detail/flicks.hpp>

namespace Recording
{
//! Dates of the box further apart than this between two ticks are a jump
//! (2 ms): a take ends where it was
inline constexpr int64_t seek_tolerance = 2 * ossia::flicks_per_millisecond<int64_t>;

/**
 * When a take waiting for a line of the grid starts. Counted in, the lines
 * closer than `bars` bars to where it began waiting go by: the count-in
 * clicks a whole bar at least, playing or starting playback.
 */
struct StartGate
{
  //! Musical position, in quarters, where the take began waiting
  double from{-1.};

  void reset() noexcept { from = -1.; }
  void waitFrom(const ossia::token_request& tk) noexcept
  {
    if(from < 0.)
      from = tk.musical_start_position;
  }
  bool accepts(
      double position, double bars, const ossia::time_signature& sig) const noexcept
  {
    if(bars <= 0.)
      return true;
    const double quarters_per_bar
        = (sig.upper > 0 && sig.lower > 0) ? 4. * sig.upper / sig.lower : 4.;
    return position >= from + bars * quarters_per_bar - 1e-6;
  }
};
}
