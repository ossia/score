#pragma once
#include <AvndProcesses/AddressTools.hpp>

namespace avnd_tools
{

struct PatternCombiner : PatternObject
{
  halp_meta(name, "Pattern combiner")
  halp_meta(author, "ossia team")
  halp_meta(category, "Control/Data processing")
  halp_meta(description, "Apply an operation to all inputs matching a pattern")
  halp_meta(c_name, "avnd_pattern_combine")
  halp_meta(uuid, "18efe965-9acc-4703-9af3-3cef658b301a")

  struct
  {
    PatternSelector pattern;

    struct
    {
      halp__enum("Mode", List, List, Average, Sum, Min, Max);
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
      if(e.apply(value_size{}) != n0)
        return std::nullopt;
    }

    return t0;
  }

  struct average
  {
    PatternCombiner& self;
    static double map(double res, double other) noexcept { return res = res + other; }
    double reduce(double res) const noexcept
    {
      return res = res / self.current_values.size();
    }
  };
  struct sum
  {
    PatternCombiner& self;
    static double map(double res, double other) noexcept { return res = res + other; }
    static double reduce(double res) noexcept { return res; }
  };
  struct minimum
  {
    PatternCombiner& self;
    static double map(double res, double other) noexcept
    {
      return res = std::min(res, other);
    }
    static double reduce(double res) noexcept { return res; }
  };
  struct maximum
  {
    PatternCombiner& self;
    static double map(double res, double other) noexcept
    {
      return res = std::max(res, other);
    }
    static double reduce(double res) noexcept { return res; }
  };

  void process_various(auto func)
  {
    double res{};
    for(const auto& val : current_values)
    {
      res = func.map(res, ossia::convert<double>(val));
    }
    res = func.reduce(res);
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
      float res{};
      for(const auto& val : self.current_values)
      {
        res = func.map(res, float(*val.target<int>()));
      }
      res = func.reduce(res);
      self.outputs.output.value = res;
    }
    void operator()(float) const noexcept
    {
      float res{};
      for(const auto& val : self.current_values)
      {
        res = func.map(res, *val.target<float>());
      }
      res = func.reduce(res);
      self.outputs.output.value = res;
    }
    void operator()(bool) const noexcept
    {
      float res{};
      for(const auto& val : self.current_values)
      {
        res = func.map(res, (*val.target<bool>() ? 1.f : 0.f));
      }
      res = func.reduce(res);
      self.outputs.output.value = res;
    }
    void operator()(const std::string&) const noexcept { }
    void operator()(ossia::vec2f) const noexcept
    {
      ossia::vec2f res{};
      for(const auto& val : self.current_values)
      {
        const auto& in = *val.target<ossia::vec2f>();
        res[0] = func.map(res[0], in[0]);
        res[1] = func.map(res[1], in[1]);
      }
      res[0] = func.reduce(res[0]);
      res[1] = func.reduce(res[1]);
      self.outputs.output.value = res;
    }
    void operator()(ossia::vec3f) const noexcept
    {
      ossia::vec3f res{};
      for(const auto& val : self.current_values)
      {
        const auto& in = *val.target<ossia::vec3f>();
        res[0] = func.map(res[0], in[0]);
        res[1] = func.map(res[1], in[1]);
        res[2] = func.map(res[2], in[2]);
      }
      res[0] = func.reduce(res[0]);
      res[1] = func.reduce(res[1]);
      res[2] = func.reduce(res[2]);
      self.outputs.output.value = res;
    }
    void operator()(ossia::vec4f) const noexcept
    {
      ossia::vec4f res{};
      for(const auto& val : self.current_values)
      {
        const auto& in = *val.target<ossia::vec4f>();
        res[0] = func.map(res[0], in[0]);
        res[1] = func.map(res[1], in[1]);
        res[2] = func.map(res[2], in[2]);
        res[3] = func.map(res[3], in[3]);
      }
      res[0] = func.reduce(res[0]);
      res[1] = func.reduce(res[1]);
      res[2] = func.reduce(res[2]);
      res[3] = func.reduce(res[3]);
      self.outputs.output.value = res;
    }
    void operator()(const std::vector<ossia::value>& v) const noexcept
    {
      boost::container::small_vector<float, 250> res;
      const int N = v.size();
      res.resize(N);
      for(const auto& val : self.current_values)
      {
        const auto& in = *val.target<std::vector<ossia::value>>();
        for(int i = 0; i < N; i++)
        {
          res[i] = func.map(res[i], ossia::convert<float>(in[i]));
        }
      }

      std::vector<ossia::value> rres;
      for(int i = 0; i < N; i++)
        rres[i] = (float)func.reduce(res[i]);

      self.outputs.output.value = std::move(rres);
    }
    void
    operator()(const std::vector<std::pair<std::string, ossia::value>>& v) const noexcept
    {
    }
    void operator()() const noexcept { }
  };

  void process_fixed(auto func)
  {
    this->current_values[0].apply(do_process_fixed{func, *this});
  }

  void operator()()
  {
    if(!m_path)
      return;

    current_values.clear();
    if(inputs.pattern.devices_dirty)
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
      case mode::Average: {
        if(auto t = all_have_same_type(current_values))
          process_fixed(average{*this});
        else
          process_various(average{*this});
        break;
      }
      case mode::Sum: {
        if(auto t = all_have_same_type(current_values))
          process_fixed(sum{*this});
        else
          process_various(sum{*this});
        break;
      }
      case mode::Min: {
        if(auto t = all_have_same_type(current_values))
          process_fixed(minimum{*this});
        else
          process_various(minimum{*this});
        break;
      }
      case mode::Max: {
        if(auto t = all_have_same_type(current_values))
          process_fixed(maximum{*this});
        else
          process_various(maximum{*this});
        break;
      }
    }
  }
};

}
