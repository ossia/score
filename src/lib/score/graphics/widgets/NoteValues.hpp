#pragma once
#include <ossia/detail/math.hpp>

#include <cmath>

#include <cstdint>
#include <iterator>
#include <string_view>

//! The note values a time or grid chooser's knob steps through: straight,
//! dotted and triplet, from 1/64th to four whole notes.
namespace score::note_values
{
struct NoteValue
{
  float fraction; // of a whole note
  const char* label;
};

// Sorted by increasing duration so turning the knob up always means longer.
// The time chooser's twelve legacy ratios are all present so existing
// documents round-trip exactly.
inline constexpr NoteValue table[] = {
    {1.f / 64.f, "1/64"},  {1.f / 48.f, "1/32T"}, {1.f / 32.f, "1/32"},
    {1.f / 24.f, "1/16T"}, {3.f / 64.f, "1/32."}, {1.f / 16.f, "1/16"},
    {1.f / 12.f, "1/8T"},  {3.f / 32.f, "1/16."}, {1.f / 8.f, "1/8"},
    {1.f / 6.f, "1/4T"},   {3.f / 16.f, "1/8."},  {1.f / 4.f, "1/4"},
    {1.f / 3.f, "1/2T"},   {3.f / 8.f, "1/4."},   {1.f / 2.f, "1/2"},
    {2.f / 3.f, "1/1T"},   {3.f / 4.f, "1/2."},   {1.f, "1/1"},
    {3.f / 2.f, "1/1."},   {2.f, "2/1"},          {3.f, "2/1."},
    {4.f, "4/1"},
};
inline constexpr int count = std::ssize(table);
inline constexpr int eighth = 8;

enum class Feel : uint8_t
{
  Straight,
  Dotted,
  Triplet
};

constexpr Feel feelOf(int index) noexcept
{
  std::string_view l = table[index].label;
  if(l.back() == '.')
    return Feel::Dotted;
  if(l.back() == 'T')
    return Feel::Triplet;
  return Feel::Straight;
}

//! The knob position, 0..1, of a note value.
constexpr double position(int index) noexcept
{
  return index / double(count - 1);
}

//! The note value at a knob position.
inline int indexAt(double v01) noexcept
{
  return ossia::clamp(int(std::lround(v01 * (count - 1))), 0, count - 1);
}

//! Snap a 0..1 knob position onto a note value of that feel (any with
//! fullTable), so that the knob angle, the emitted value and the execution
//! feedback all land on the exact same positions.
inline double snapPosition(double v01, Feel feel, bool fullTable) noexcept
{
  const double target = ossia::clamp(v01, 0., 1.) * (count - 1);
  if(fullTable)
    return std::lround(target) / double(count - 1);
  int best = -1;
  double bestDist = 0.;
  for(int i = 0; i < count; i++)
  {
    if(feelOf(i) != feel)
      continue;
    const double d = std::abs(target - i);
    if(best < 0 || d < bestDist)
    {
      bestDist = d;
      best = i;
    }
  }
  return best / double(count - 1);
}

//! The note value nearest to a fraction of a whole note.
inline int nearest(float frac) noexcept
{
  int best = 0;
  float bestDist = std::abs(table[0].fraction - frac);
  for(int i = 1; i < count; i++)
  {
    const float d = std::abs(table[i].fraction - frac);
    if(d < bestDist)
    {
      bestDist = d;
      best = i;
    }
  }
  return best;
}

//! The note value of that feel nearest to a fraction of a whole note.
inline int nearest(float frac, Feel feel) noexcept
{
  int best = -1;
  float bestDist = 0.f;
  for(int i = 0; i < count; i++)
  {
    if(feelOf(i) != feel)
      continue;
    const float d = std::abs(table[i].fraction - frac);
    if(best < 0 || d < bestDist)
    {
      bestDist = d;
      best = i;
    }
  }
  return best;
}

//! How long the note is, straight: a dotted note is 3/2 of it, a triplet 2/3.
inline float straightLength(int index) noexcept
{
  switch(feelOf(index))
  {
    case Feel::Dotted:
      return table[index].fraction / 1.5f;
    case Feel::Triplet:
      return table[index].fraction * 1.5f;
    default:
      return table[index].fraction;
  }
}

inline float withFeel(float straight, Feel feel) noexcept
{
  switch(feel)
  {
    case Feel::Dotted:
      return straight * 1.5f;
    case Feel::Triplet:
      return straight / 1.5f;
    default:
      return straight;
  }
}
}
