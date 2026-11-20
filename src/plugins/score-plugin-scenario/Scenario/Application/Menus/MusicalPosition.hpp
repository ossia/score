#pragma once
#include <Process/TimeValue.hpp>

#include <ossia/editor/scenario/time_signature.hpp>

#include <algorithm>
#include <cstdint>
#include <iterator>

namespace Scenario
{
/**
 * @brief How the pulses of a bar are laid out in the transport's pulse grid.
 *
 * The pulses fill the grid row by row from the top left. Every row is full
 * except possibly the last one, which is filled from the left.
 */
struct PulseGridLayout
{
  int columns{1};
  int rows{1};
  int pulses{1};

  constexpr int row(int pulse) const noexcept { return pulse / columns; }
  constexpr int column(int pulse) const noexcept { return pulse % columns; }
  constexpr int pulsesInRow(int r) const noexcept
  {
    return r < rows - 1 ? columns : pulses - columns * (rows - 1);
  }
};

/**
 * @brief Grid for a time signature numerator.
 *
 * The smallest square holding the pulses: c columns for the smallest c with
 * c * c >= pulses, and only the rows that are used. The indicator has a fixed
 * square footprint, so fewer columns means larger cells: 7 is laid out
 * 3 + 3 + 1 rather than 4 + 3, and 19 is 5 + 5 + 5 + 4.
 */
constexpr PulseGridLayout pulseGridLayout(int numerator) noexcept
{
  const int n = std::max(1, numerator);

  int cols = 1;
  while(cols * cols < n)
    cols++;

  return {.columns = cols, .rows = (n + cols - 1) / cols, .pulses = n};
}

//! Where a date falls in the bars of a time signature map.
struct MusicalPosition
{
  //! Bars elapsed since the beginning. A bar cut short by a signature change
  //! counts as a bar.
  int64_t bar{};
  //! Pulse of the bar, in units of the signature's denominator.
  int pulse{};
  //! Quarter of the pulse.
  int subdivision{};
  ossia::time_signature signature{};

  friend bool operator==(const MusicalPosition&, const MusicalPosition&) = default;
};

/**
 * @brief Musical position of a date.
 * @param signatures Sorted range of (TimeVal, ossia::time_signature) pairs.
 *
 * 4/4 applies where no signature is set: an empty map, or before its first
 * entry.
 */
template <typename Signatures>
MusicalPosition musicalPosition(const Signatures& signatures, TimeVal date) noexcept
{
  constexpr int64_t whole = 4 * ossia::quarter_duration<int64_t>;
  auto bar_duration = [](ossia::time_signature sig) {
    return std::max<int64_t>(
        1, whole * std::max<int64_t>(1, sig.upper) / std::max<int64_t>(1, sig.lower));
  };

  const int64_t t = std::max<int64_t>(0, date.impl);

  int64_t bars = 0;
  int64_t origin = 0;
  ossia::time_signature sig{4, 4};

  for(auto it = std::begin(signatures); it != std::end(signatures); ++it)
  {
    const int64_t change = it->first.impl;
    if(change > t)
      break;

    if(change > origin)
    {
      const int64_t bar = bar_duration(sig);
      bars += (change - origin + bar - 1) / bar;
      origin = change;
    }
    sig = it->second;
  }

  sig.upper = std::max<uint16_t>(1, sig.upper);
  sig.lower = std::max<uint16_t>(1, sig.lower);

  const int64_t bar = bar_duration(sig);
  const int64_t since = t - origin;
  const int64_t in_bar = since % bar;

  // Computed against the bar rather than from a rounded pulse duration, so
  // that the last pulse of a bar never overflows into a pulse that does not
  // exist.
  const int64_t quarter_pulses = in_bar * sig.upper * 4 / bar;

  MusicalPosition res;
  res.bar = bars + since / bar;
  res.pulse = int(std::min<int64_t>(quarter_pulses / 4, sig.upper - 1));
  res.subdivision = int(quarter_pulses % 4);
  res.signature = sig;
  return res;
}
}
