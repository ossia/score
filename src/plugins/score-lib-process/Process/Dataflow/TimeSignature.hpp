#pragma once
#include <score/tools/std/Optional.hpp>
#include <score/serialization/DataStreamFwd.hpp> 
#include <ossia/editor/scenario/time_signature.hpp>
#include <ossia-qt/time.hpp>

#include <string>
#include <utility>
#include <verdigris>

#include <string_view>
namespace Control
{
inline std::optional<ossia::time_signature> get_time_signature(const std::string_view& v)
{
  constexpr const auto is_valid_denom = [](int denom) {
    return denom == 1 || denom == 2 || denom == 4 || denom == 8 || denom == 16 || denom == 32
           || denom == 64;
  };

  try
  {
    if (v.size() >= 3)
    {
      auto it = v.find('/');
      if (it > 0 && it < (v.size() - 1))
      {
        std::string_view num{v.data(), it};
        std::string_view denom{v.data() + it + 1, v.size() - it};
        int num_n = std::stoi(std::string(num)); // OPTIMIZEME
        int denom_n = std::stoi(std::string(denom));

        if (num_n >= 1 && num_n <= 512 && is_valid_denom(denom_n))
          return ossia::time_signature{(uint16_t)num_n, (uint16_t)denom_n};
      }
    }
  }
  catch (...)
  {
  }
  return {};
}
}

SCORE_SERIALIZE_DATASTREAM_DECLARE(, ossia::time_signature)

Q_DECLARE_METATYPE(std::optional<ossia::time_signature>)
W_REGISTER_ARGTYPE(std::optional<ossia::time_signature>)
