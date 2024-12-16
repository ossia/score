#pragma once
#include <Fx/MathHelpers.hpp>
#include <Fx/Types.hpp>

#include <ossia/dataflow/value_port.hpp>

#include <halp/callback.hpp>
namespace Nodes
{
template <typename State>
struct GenericMathMapping
{
  static void store_output(auto& self, const ossia::value& v)
  {
    switch(v.get_type())
    {
      case ossia::val_type::NONE:
        break;
      case ossia::val_type::FLOAT:
        self.po = *v.target<float>();
        break;
      case ossia::val_type::VEC2F: {
        if constexpr(requires { self.pov; })
        {
          auto& vec = *v.target<ossia::vec2f>();
          self.pov.assign(vec.begin(), vec.end());
        }
        break;
      }
      case ossia::val_type::VEC3F: {
        if constexpr(requires { self.pov; })
        {
          auto& vec = *v.target<ossia::vec3f>();
          self.pov.assign(vec.begin(), vec.end());
        }
        break;
      }
      case ossia::val_type::VEC4F: {
        if constexpr(requires { self.pov; })
        {
          auto& vec = *v.target<ossia::vec4f>();
          self.pov.assign(vec.begin(), vec.end());
        }
        break;
      }
      case ossia::val_type::LIST: {
        if constexpr(requires { self.pov; })
        {
          auto& arr = *v.target<std::vector<ossia::value>>();
          if(!arr.empty())
          {
            self.pov.clear();
            for(auto& v : arr)
              self.pov.push_back(ossia::convert<float>(v));
          }
        }
        break;
      }
      // Only these types are used now as per ossia::math_expression::result()
      default:
        break;
    }
  }

  static void exec_scalar(State& self, value_output_callback& output)
  {
    auto res = self.expr.result();

    self.px = self.x;
    store_output(self, res);

    output(res);
  }

  static void exec_polyphonic(State& self, value_output_callback& output)
  {
    for(auto& e : self.expressions)
    {
      auto res = e.expr.result();

      if constexpr(requires { e.x; })
        e.px = e.x;

      store_output(e, res);
    }
  }

  static void
  exec_array(State& self, value_output_callback& output, bool vector_size_did_change)
  {
    if(self.xv.empty())
      return;

    if(vector_size_did_change)
    {
      self.expr.rebase_vector("xv", self.xv);
      self.expr.recompile();
    }

    auto res = self.expr.result();
    store_output(self, res);

    // Save the previous input
    {
      bool old_prev = self.pxv.size();
      self.pxv.assign(self.xv.begin(), self.xv.end());
      bool new_prev = self.pxv.size();

      if(old_prev != new_prev)
      {
        self.expr.rebase_vector("pxv", self.pxv);
        self.expr.recompile();
      }
    }

    output(std::move(res));
  }

  static void run_scalar(
      const ossia::value& v, value_output_callback& output, const halp::tick_flicks& tk,
      State& self)
  {
    //auto ratio = st.modelToSamples();
    //auto parent_dur = tk.parent_duration * ratio;
    //for(const ossia::timed_value& v : input.get_data())
    //{
    //  int64_t new_time = tk.prev_date.impl * ratio + timestamp;
    //  setMathExpressionTiming(self, new_time, self.last_value_time, parent_dur);
    //  self.last_value_time = new_time;

    // FIXME
    //  setMathExpressionTiming(self, new_time, self.last_value_time, parent_dur);
    switch(v.get_type())
    {
      case ossia::val_type::NONE:
        break;
      case ossia::val_type::IMPULSE:
        break;
      case ossia::val_type::INT:
        self.x = *v.target<int>();
        break;
      case ossia::val_type::FLOAT:
        self.x = *v.target<float>();
        break;
      case ossia::val_type::BOOL:
        self.x = *v.target<bool>() ? 1.f : 0.f;
        break;
      case ossia::val_type::STRING:
        self.x = ossia::convert<float>(v);
        break;
      case ossia::val_type::VEC2F:
        self.x = (*v.target<ossia::vec2f>())[0];
        break;
      case ossia::val_type::VEC3F:
        self.x = (*v.target<ossia::vec3f>())[0];
        break;
      case ossia::val_type::VEC4F:
        self.x = (*v.target<ossia::vec4f>())[0];
        break;
      case ossia::val_type::LIST: {
        auto& arr = *v.target<std::vector<ossia::value>>();
        if(!arr.empty())
          self.x = ossia::convert<float>(arr[0]);
        break;
      }
      case ossia::val_type::MAP: {
        auto& arr = *v.target<ossia::value_map_type>();
        if(!arr.empty())
          self.x = ossia::convert<float>(arr.begin()->second);
        break;
      }
    }

    GenericMathMapping::exec_scalar(self, output);
  }

  static bool resize(const std::string& expr, State& self, int sz)
  {
    if(std::ssize(self.expressions) == sz && expr == self.last_expression)
      return true;

    self.expressions.resize(sz);
    self.count = sz;
    int i = 0;
    for(auto& e : self.expressions)
    {
      e.init(self.cur_time, self.cur_deltatime, self.cur_pos, self.count);
      e.instance = i++;
      if(!e.expr.set_expression(expr))
        return false;
      e.expr.seed_random(
          UINT64_C(0xda3e39cb94b95bdb), UINT64_C(0x853c49e6748fea9b) * (1 + i));
    }
    self.last_expression = expr;
    return true;
  }

  static void run_polyphonic(
      int size, value_output_callback& output, const std::string& expr,
      const halp::tick_flicks& tk, State& self)
  {
    if(size <= 1
       || (expr.find(":=") != std::string::npos && expr.find("po") != std::string::npos))
    {
      size = std::clamp(size, 0, 1024);
      resize(expr, self, size);

      setMathExpressionTiming(
          self, tk.start_in_flicks, self.last_value_time, tk.relative_position);
      self.last_value_time = tk.start_in_flicks;

      GenericMathMapping::exec_polyphonic(self, output);

      std::vector<ossia::value> res;
      res.resize(self.expressions.size());
      self.count = size;
      for(int i = 0; i < self.expressions.size(); i++)
        res[i] = (float)self.expressions[i].po;
      // Combine
      output(std::move(res));
    }
    else
    {
      resize(expr, self, 1);
      setMathExpressionTiming(
          self, tk.start_in_flicks, self.last_value_time, tk.relative_position);
      self.last_value_time = tk.start_in_flicks;

      std::vector<ossia::value> res;
      res.resize(size);
      self.count = size;
      for(int i = 0; i < size; i++)
      {
        auto& e = self.expressions[0];
        e.instance = i;
        res[i] = e.expr.result();

        // po isn't used either store_output(e, res);
      }
      output(std::move(res));
    }
  }

  static void run_polyphonic(
      const ossia::value& value, value_output_callback& output, const std::string& expr,
      const halp::tick_flicks& tk, State& self)
  {
    setMathExpressionTiming(
        self, tk.start_in_flicks, self.last_value_time, tk.relative_position);
    self.last_value_time = tk.start_in_flicks;
    //auto ratio = st.modelToSamples();
    //auto parent_dur = tk.parent_duration.impl * ratio;
    //for(const ossia::timed_value& v : input.get_data())
    //{
    //  auto val = value.target<std::vector<ossia::value>>();
    //  if(!val)
    //    return;

    //  int64_t new_time = tk.prev_date.impl * ratio + timestamp;
    //  setMathExpressionTiming(self, new_time, self.last_value_time, parent_dur);
    //  self.last_value_time = new_time;

    switch(value.get_type())
    {
      case ossia::val_type::NONE:
        break;
      case ossia::val_type::IMPULSE:
        break;
      case ossia::val_type::INT:
        if(!resize(expr, self, 1))
          return;
        self.expressions[0].x = *value.target<int>();
        break;
      case ossia::val_type::FLOAT:
        if(!resize(expr, self, 1))
          return;
        self.expressions[0].x = *value.target<float>();
        break;
      case ossia::val_type::BOOL:
        if(!resize(expr, self, 1))
          return;
        self.expressions[0].x = *value.target<bool>() ? 1.f : 0.f;
        break;
      case ossia::val_type::STRING:
        if(!resize(expr, self, 1))
          return;
        self.expressions[0].x = ossia::convert<float>(value);
        break;
      case ossia::val_type::VEC2F:
        if(!resize(expr, self, 2))
          return;
        self.expressions[0].x = (*value.target<ossia::vec2f>())[0];
        self.expressions[1].x = (*value.target<ossia::vec2f>())[1];
        break;
      case ossia::val_type::VEC3F:
        if(!resize(expr, self, 3))
          return;
        self.expressions[0].x = (*value.target<ossia::vec3f>())[0];
        self.expressions[1].x = (*value.target<ossia::vec3f>())[1];
        self.expressions[2].x = (*value.target<ossia::vec3f>())[2];
        break;
      case ossia::val_type::VEC4F:
        if(!resize(expr, self, 4))
          return;
        self.expressions[0].x = (*value.target<ossia::vec4f>())[0];
        self.expressions[1].x = (*value.target<ossia::vec4f>())[1];
        self.expressions[2].x = (*value.target<ossia::vec4f>())[2];
        self.expressions[3].x = (*value.target<ossia::vec4f>())[3];
        break;
      case ossia::val_type::LIST: {
        auto& arr = *value.target<std::vector<ossia::value>>();
        const auto N = std::clamp((int)std::ssize(arr), 0, 1024);
        if(!resize(expr, self, N))
          return;
        for(int i = 0; i < N; i++)
          self.expressions[i].x = ossia::convert<float>(arr[i]);
        break;
      }
      case ossia::val_type::MAP: {
        auto& arr = *value.target<ossia::value_map_type>();
        const auto N = std::clamp((int)std::ssize(arr), 0, 1024);
        if(!resize(expr, self, N))
          return;
        int i = 0;
        for(auto& [k, v] : arr)
          self.expressions[i++].x = ossia::convert<float>(v);
        break;
      }
    }

    GenericMathMapping::exec_polyphonic(self, output);

    std::vector<ossia::value> res;
    res.resize(self.expressions.size());
    for(int i = 0; i < self.expressions.size(); i++)
      res[i] = (float)self.expressions[i].po;

    // Combine
    output(std::move(res));
  }

  static void run_array(
      const ossia::value& value, value_output_callback& output,
      const halp::tick_flicks& tk, State& self)
  {
    //auto ratio = st.modelToSamples();
    //auto parent_dur = tk.parent_duration.impl * ratio;
    //for(const ossia::timed_value& v : input.get_data())
    //{
    //  int64_t new_time = tk.prev_date.impl * ratio + timestamp;
    //  setMathExpressionTiming(self, new_time, self.last_value_time, parent_dur);
    //  self.last_value_time = new_time;

    auto array_run_scalar = [&](float in) {
      auto old_size = self.xv.size();
      self.xv.assign(1, in);
      auto new_size = 1U;
      GenericMathMapping::exec_array(self, output, old_size != new_size);
    };

    switch(value.get_type())
    {
      case ossia::val_type::NONE:
        break;
      case ossia::val_type::IMPULSE:
        GenericMathMapping::exec_array(self, output, false);
        break;
      case ossia::val_type::INT:
        array_run_scalar(*value.target<int>());
        break;
      case ossia::val_type::FLOAT:
        array_run_scalar(*value.target<float>());
        break;
      case ossia::val_type::BOOL:
        array_run_scalar(*value.target<bool>() ? 1.f : 0.f);
        break;
      case ossia::val_type::STRING:
        array_run_scalar(ossia::convert<float>(value));
        break;
      case ossia::val_type::VEC2F: {
        auto& arr = *value.target<ossia::vec2f>();
        auto old_size = self.xv.size();
        self.xv.assign(arr.begin(), arr.end());
        auto new_size = 2U;
        GenericMathMapping::exec_array(self, output, old_size != new_size);
        break;
      }
      case ossia::val_type::VEC3F: {
        auto& arr = *value.target<ossia::vec3f>();
        auto old_size = self.xv.size();
        self.xv.assign(arr.begin(), arr.end());
        auto new_size = 3U;
        GenericMathMapping::exec_array(self, output, old_size != new_size);
        break;
      }
      case ossia::val_type::VEC4F: {
        auto& arr = *value.target<ossia::vec4f>();
        auto old_size = self.xv.size();
        self.xv.assign(arr.begin(), arr.end());
        auto new_size = 4U;
        GenericMathMapping::exec_array(self, output, old_size != new_size);
        break;
      }
      case ossia::val_type::LIST: {
        auto& arr = *value.target<std::vector<ossia::value>>();
        auto old_size = self.xv.size();
        self.xv.resize(arr.size());
        auto new_size = arr.size();
        for(std::size_t i = 0; i < arr.size(); i++)
        {
          self.xv[i] = ossia::convert<float>(arr[i]);
        }
        GenericMathMapping::exec_array(self, output, old_size != new_size);
        break;
      }
      case ossia::val_type::MAP: {
        auto& arr = *value.target<ossia::value_map_type>();
        auto old_size = self.xv.size();
        self.xv.resize(arr.size());
        auto new_size = arr.size();
        int i = 0;
        for(const auto& [k, v] : arr)
        {
          self.xv[i++] = ossia::convert<float>(v);
        }
        GenericMathMapping::exec_array(self, output, old_size != new_size);
        break;
      }
    }
    //}
  }
};
}
