#pragma once

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/math/math_expression.hpp>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>

namespace Nodes
{

template <typename State>
static void setMathExpressionTiming(
    State& self, int64_t input_time, int64_t prev_time, std::integral auto parent_dur)
    = delete;

template <typename State>
static void setMathExpressionTiming(
    State& self, int64_t input_time, int64_t prev_time,
    std::floating_point auto parent_dur)
{
  self.cur_time = input_time;
  self.cur_deltatime = (input_time - prev_time);
  self.cur_pos = parent_dur > 0 ? double(input_time) / parent_dur : 0;
}

template <typename State>
static void setMathExpressionTiming(State& self, const halp::tick_flicks& tk)
{
  self.cur_time = tk.end_in_flicks;
  self.cur_deltatime = tk.end_in_flicks - tk.start_in_flicks;
  self.cur_pos = tk.parent_duration > 0 ? tk.relative_position : 0.;
}
}
