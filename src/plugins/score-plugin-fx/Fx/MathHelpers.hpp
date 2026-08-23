#pragma once

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/math/math_expression.hpp>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>

#include <string_view>

namespace Nodes
{

//! Whether `id` occurs in `expr` as a whole identifier.
//!
//! A plain substring search is not enough: looking for "po" that way also
//! matches "pos", "pov" and "pow", which is how `var rrr := pos; return [pos]`
//! ended up on the per-element-feedback code path.
inline bool uses_identifier(std::string_view expr, std::string_view id) noexcept
{
  static constexpr auto is_ident_char = [](char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
           || c == '_';
  };

  for(auto pos = expr.find(id); pos != std::string_view::npos;
      pos = expr.find(id, pos + 1))
  {
    const auto end = pos + id.size();
    const bool left_ok = (pos == 0) || !is_ident_char(expr[pos - 1]);
    const bool right_ok = (end >= expr.size()) || !is_ident_char(expr[end]);
    if(left_ok && right_ok)
      return true;
  }
  return false;
}

template <typename State>
static void setMathExpressionTiming(
    State& self, int64_t input_time, int64_t prev_time, std::integral auto parent_dur)
    = delete;

template <typename State>
static void setMathExpressionTiming(
    State& self, int64_t input_time, int64_t prev_time,
    std::floating_point auto parent_pos)
{
  self.cur_time = input_time;
  self.cur_deltatime = (input_time - prev_time);
  self.cur_pos = parent_pos;
}

template <typename State>
static void setMathExpressionTiming(State& self, const halp::tick_flicks& tk)
{
  self.cur_time = tk.end_in_flicks;
  self.cur_deltatime = tk.end_in_flicks - tk.start_in_flicks;
  self.cur_pos = tk.parent_duration > 0 ? tk.relative_position : 0.;
}
}
