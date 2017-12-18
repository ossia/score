#pragma once
#include <ossia/detail/optional.hpp>
#include <utility>
#include <string>
#include <string_view>

namespace Process
{
using time_signature = std::pair<uint16_t, uint16_t>;
inline ossia::optional<time_signature> get_time_signature(const std::string_view& v)
{
  constexpr const auto is_valid_denom = [] (int denom)
  {
    return denom == 1  ||
        denom == 2  ||
        denom == 4  ||
        denom == 8  ||
        denom == 16 ||
        denom == 32 ||
        denom == 64;
  };

  try
  {
    if(v.size() > 3)
    {
      auto it = v.find('/');
      if(it > 0 && it < (v.size() - 1))
      {
        std::string_view num{v.data(), it};
        std::string_view denom{v.data() + it + 1, v.size() - it};
        int num_n = std::stoi(std::string(num)); // OPTIMIZEME
        int denom_n = std::stoi(std::string(denom));

        if(num_n >= 1 && num_n <= 512 && is_valid_denom(denom_n))
          return time_signature(num_n, denom_n);
      }
    }
  } catch (...) { }
  return {};
}

}
