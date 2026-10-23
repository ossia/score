#pragma once

// The value of a halp::channel_mask with the given channels (from 1) on.

#include <array>
#include <initializer_list>

namespace midi_test
{
inline std::array<bool, 16> channels_on(std::initializer_list<int> channels)
{
  std::array<bool, 16> res{};
  for(int c : channels)
    res[c - 1] = true;
  return res;
}
}
