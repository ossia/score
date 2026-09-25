#pragma once
#include <halp/static_string.hpp>

#include <ossia/network/value/value.hpp>

#include <algorithm>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace Threedim
{

struct string_list_values : std::vector<std::string>
{
  using std::vector<std::string>::vector;
  using std::vector<std::string>::operator=;
};

inline const std::string* string_list_row_text(const ossia::value& row) noexcept
{
  if(auto str = row.target<std::string>())
    return str;
  if(auto pair = row.target<std::vector<ossia::value>>(); pair && pair->size() == 2)
    return (*pair)[1].target<std::string>();
  return nullptr;
}

inline bool from_ossia_value(const ossia::value& src, string_list_values& dst)
{
  dst.clear();
  if(auto str = src.target<std::string>())
  {
    dst.push_back(*str);
    return true;
  }
  if(auto list = src.target<std::vector<ossia::value>>())
  {
    dst.reserve(list->size());
    for(const auto& row : *list)
      if(auto text = string_list_row_text(row))
        dst.push_back(*text);
    return true;
  }
  return false;
}

inline ossia::value keyed_string_list(const ossia::value& src)
{
  std::vector<ossia::value> rows;
  if(auto str = src.target<std::string>())
  {
    rows.emplace_back(std::vector<ossia::value>{10000, *str});
    return rows;
  }

  auto list = src.target<std::vector<ossia::value>>();
  if(!list)
    return src;

  int next_key = 10000;
  for(const auto& row : *list)
    if(auto pair = row.target<std::vector<ossia::value>>(); pair && pair->size() == 2)
      if(auto key = (*pair)[0].target<int>(); key && *key < std::numeric_limits<int>::max())
        next_key = std::max(next_key, *key + 1);

  rows.reserve(list->size());
  for(const auto& row : *list)
  {
    if(auto pair = row.target<std::vector<ossia::value>>();
       pair && pair->size() == 2 && (*pair)[0].target<int>()
       && (*pair)[1].target<std::string>())
      rows.push_back(row);
    else if(auto str = row.target<std::string>())
      rows.emplace_back(std::vector<ossia::value>{next_key++, *str});
  }
  return rows;
}

template <halp::static_string Name>
struct string_list_control
{
  enum class widget
  {
    string_list
  };
  static constexpr auto name() { return std::string_view{Name.value}; }
  static ossia::value migrate_value(const ossia::value& v)
  {
    return keyed_string_list(v);
  }
  string_list_values value;
};

}
