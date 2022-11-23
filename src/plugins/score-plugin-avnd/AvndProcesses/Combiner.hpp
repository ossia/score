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

  void operator()()
  {
    if(!m_path)
      return;

    current_values.clear();
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
        double res{};
        for(const auto& val : current_values)
        {
          res += ossia::convert<double>(val);
        }
        res /= current_values.size();
        outputs.output.value = float(res);
        break;
      }
      case mode::Sum: {
        double res{};
        for(const auto& val : current_values)
        {
          res += ossia::convert<double>(val);
        }
        outputs.output.value = float(res);
        break;
      }
      case mode::Min: {
        double res = ossia::convert<double>(current_values[0]);
        for(std::size_t i = 1; i < current_values.size(); i++)
        {
          res = std::min(res, ossia::convert<double>(current_values[i]));
        }
        outputs.output.value = float(res);
        break;
      }
      case mode::Max: {
        double res = ossia::convert<double>(current_values[0]);
        for(std::size_t i = 1; i < current_values.size(); i++)
        {
          res = std::max(res, ossia::convert<double>(current_values[i]));
        }
        outputs.output.value = float(res);
        break;
      }
    }

    /*
    auto process = [this](const std::vector<ossia::value>& vec) {
      QTimer::singleShot(1, qApp, [roots = this->roots, vec] {
        const auto N = std::min(roots.size(), vec.size());
        for(std::size_t i = 0; i < N; i++)
        {
          if(auto p = roots[i]->get_parameter())
          {
            // Needs to be done in main thread because of the QJSEngine in serial_protocols
            p->push_value(vec[i]);
          }
        }
      });
    };
    if(auto vvec = inputs.input.value.target<std::vector<ossia::value>>())
    {
      process(*vvec);
    }
    else
    {
      process(ossia::convert<std::vector<ossia::value>>(inputs.input.value));
    }
    */
  }
};

}
