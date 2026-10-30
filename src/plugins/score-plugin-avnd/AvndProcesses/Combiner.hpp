#pragma once
#include <ossia/network/base/osc_address.hpp>

#include <AvndProcesses/AddressTools.hpp>

#include <limits>

namespace avnd_tools
{

struct PatternCombiner : PatternObject
{
  halp_meta(name, "Pattern combiner")
  halp_meta(author, "ossia team")
  halp_meta(category, "Control/Data Processing")
  halp_meta(description, "Apply an operation to all inputs matching a pattern")
  halp_meta(c_name, "avnd_pattern_combine")
  halp_meta(uuid, "18efe965-9acc-4703-9af3-3cef658b301a")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/pattern-combiner.html#pattern-combiner")

  struct
  {
    PatternSelector pattern;

    struct
    {
      halp__enum("Mode", List, List, Average, Sum, Min, Max, Map);
    } mode{};

  } inputs;

  struct
  {
    halp::val_port<"Output", ossia::value> output;
  } outputs;

  std::vector<ossia::value> current_values;

  struct value_size
  {
    int operator()(ossia::impulse) const noexcept { return 1; }
    int operator()(int) const noexcept { return 1; }
    int operator()(float) const noexcept { return 1; }
    int operator()(bool) const noexcept { return 1; }
    int operator()(const std::string&) const noexcept { return 1; }
    int operator()(ossia::vec2f) const noexcept { return 2; }
    int operator()(ossia::vec3f) const noexcept { return 3; }
    int operator()(ossia::vec4f) const noexcept { return 4; }
    int operator()(const std::vector<ossia::value>& v) const noexcept
    {
      return v.size();
    }
    int
    operator()(const std::vector<std::pair<std::string, ossia::value>>& v) const noexcept
    {
      return v.size();
    }
    int operator()() const noexcept { return 0; }
  };

  static std::optional<ossia::val_type>
  all_have_same_type(const std::vector<ossia::value>& val) noexcept
  {
    if(val.empty())
      return std::nullopt;

    auto t0 = val[0].get_type();
    auto n0 = val[0].apply(value_size{});
    for(auto& e : val)
    {
      if(e.get_type() != t0)
        return std::nullopt;
      // Maps are reduced key by key: they do not need to share their keys
      if(t0 != ossia::val_type::MAP && e.apply(value_size{}) != n0)
        return std::nullopt;
    }

    return t0;
  }

  // init: the identity of the operation; n: how many values were mapped
  struct average
  {
    static constexpr double init = 0.;
    static double map(double res, double other) noexcept { return res + other; }
    static double reduce(double res, std::size_t n) noexcept { return res / n; }
  };
  struct sum
  {
    static constexpr double init = 0.;
    static double map(double res, double other) noexcept { return res + other; }
    static double reduce(double res, std::size_t) noexcept { return res; }
  };
  struct minimum
  {
    static constexpr double init = std::numeric_limits<double>::infinity();
    static double map(double res, double other) noexcept { return std::min(res, other); }
    static double reduce(double res, std::size_t) noexcept { return res; }
  };
  struct maximum
  {
    static constexpr double init = -std::numeric_limits<double>::infinity();
    static double map(double res, double other) noexcept { return std::max(res, other); }
    static double reduce(double res, std::size_t) noexcept { return res; }
  };

  void process_various(auto func)
  {
    double res = func.init;
    for(const auto& val : current_values)
    {
      res = func.map(res, ossia::convert<double>(val));
    }
    res = func.reduce(res, current_values.size());
    outputs.output.value = float(res);
  }

  template <typename Op>
  struct do_process_fixed
  {
    Op func;
    PatternCombiner& self;
    void operator()(ossia::impulse) const noexcept { }
    void operator()(int) const noexcept
    {
      float res = func.init;
      for(const auto& val : self.current_values)
      {
        res = func.map(res, float(*val.template target<int>()));
      }
      res = func.reduce(res, self.current_values.size());
      self.outputs.output.value = res;
    }
    void operator()(float) const noexcept
    {
      float res = func.init;
      for(const auto& val : self.current_values)
      {
        res = func.map(res, *val.template target<float>());
      }
      res = func.reduce(res, self.current_values.size());
      self.outputs.output.value = res;
    }
    void operator()(bool) const noexcept
    {
      float res = func.init;
      for(const auto& val : self.current_values)
      {
        res = func.map(res, (*val.template target<bool>() ? 1.f : 0.f));
      }
      res = func.reduce(res, self.current_values.size());
      self.outputs.output.value = res;
    }
    void operator()(const std::string&) const noexcept { }
    void operator()(ossia::vec2f) const noexcept
    {
      ossia::vec2f res;
      res.fill(func.init);
      for(const auto& val : self.current_values)
      {
        const auto& in = *val.template target<ossia::vec2f>();
        res[0] = func.map(res[0], in[0]);
        res[1] = func.map(res[1], in[1]);
      }
      res[0] = func.reduce(res[0], self.current_values.size());
      res[1] = func.reduce(res[1], self.current_values.size());
      self.outputs.output.value = res;
    }
    void operator()(ossia::vec3f) const noexcept
    {
      ossia::vec3f res;
      res.fill(func.init);
      for(const auto& val : self.current_values)
      {
        const auto& in = *val.template target<ossia::vec3f>();
        res[0] = func.map(res[0], in[0]);
        res[1] = func.map(res[1], in[1]);
        res[2] = func.map(res[2], in[2]);
      }
      res[0] = func.reduce(res[0], self.current_values.size());
      res[1] = func.reduce(res[1], self.current_values.size());
      res[2] = func.reduce(res[2], self.current_values.size());
      self.outputs.output.value = res;
    }
    void operator()(ossia::vec4f) const noexcept
    {
      ossia::vec4f res;
      res.fill(func.init);
      for(const auto& val : self.current_values)
      {
        const auto& in = *val.template target<ossia::vec4f>();
        res[0] = func.map(res[0], in[0]);
        res[1] = func.map(res[1], in[1]);
        res[2] = func.map(res[2], in[2]);
        res[3] = func.map(res[3], in[3]);
      }
      res[0] = func.reduce(res[0], self.current_values.size());
      res[1] = func.reduce(res[1], self.current_values.size());
      res[2] = func.reduce(res[2], self.current_values.size());
      res[3] = func.reduce(res[3], self.current_values.size());
      self.outputs.output.value = res;
    }
    void operator()(const std::vector<ossia::value>& v) const noexcept
    {
      boost::container::small_vector<float, 250> res;
      const int N = v.size();
      res.assign(N, func.init);
      for(const auto& val : self.current_values)
      {
        const auto& in = *val.template target<std::vector<ossia::value>>();
        for(int i = 0; i < N; i++)
        {
          res[i] = func.map(res[i], ossia::convert<float>(in[i]));
        }
      }

      std::vector<ossia::value> rres;
      rres.reserve(N);
      for(int i = 0; i < N; i++)
        rres.emplace_back((float)func.reduce(res[i], self.current_values.size()));

      self.outputs.output.value = std::move(rres);
    }
    void operator()(const ossia::value_map_type&) const noexcept
    {
      // A key missing from some inputs is reduced over the inputs that have it
      ossia::value_map_type rres;
      boost::container::small_vector<double, 64> res;
      boost::container::small_vector<std::size_t, 64> count;
      for(const auto& val : self.current_values)
      {
        for(const auto& [k, v] : *val.template target<ossia::value_map_type>())
        {
          auto it = ossia::find_if(rres, [&k](const auto& e) { return e.first == k; });
          const std::size_t i = it - rres.begin();
          if(it == rres.end())
          {
            rres.emplace_back(k, ossia::value{});
            res.push_back(func.init);
            count.push_back(0);
          }
          res[i] = func.map(res[i], ossia::convert<double>(v));
          count[i]++;
        }
      }

      std::size_t i = 0;
      for(auto& [k, v] : rres)
      {
        v = (float)func.reduce(res[i], count[i]);
        i++;
      }

      self.outputs.output.value = std::move(rres);
    }
    void operator()() const noexcept { }
  };

  template <typename T>
  void process_fixed(T func)
  {
    this->current_values[0].apply(do_process_fixed<T>{func, *this});
  }

  void operator()()
  {
    if(m_paths.empty())
      return;

    current_values.clear();
    if(inputs.pattern.observer.dirty)
      inputs.pattern.reprocess();
    for(auto in : this->roots)
    {
      if(auto p = in->get_parameter())
        current_values.push_back(p->value());
    }

    if(current_values.empty())
    {
      return;
    }

    using mode = decltype(inputs.mode)::enum_type;
    switch(inputs.mode)
    {
      case mode::List:
        outputs.output.value = current_values;
        break;
      case mode::Map: {
        ossia::value_map_type res;
        res.reserve(current_values.size());
        for(auto in : this->roots)
        {
          if(auto p = in->get_parameter())
            res.emplace_back(ossia::net::address_string_from_node(*in), p->value());
        }
        outputs.output.value = std::move(res);
        break;
      }
      case mode::Average: {
        if(auto t = all_have_same_type(current_values))
          process_fixed(average{});
        else
          process_various(average{});
        break;
      }
      case mode::Sum: {
        if(auto t = all_have_same_type(current_values))
          process_fixed(sum{});
        else
          process_various(sum{});
        break;
      }
      case mode::Min: {
        if(auto t = all_have_same_type(current_values))
          process_fixed(minimum{});
        else
          process_various(minimum{});
        break;
      }
      case mode::Max: {
        if(auto t = all_have_same_type(current_values))
          process_fixed(maximum{});
        else
          process_various(maximum{});
        break;
      }
    }
  }
};

}
