#pragma once
#include <ossia/network/value/value.hpp>

#include <halp/controls.hpp>
#include <halp/dynamic_port.hpp>
#include <halp/meta.hpp>

#include <cmath>

#include <algorithm>

namespace avnd_tools
{

struct Interpolator
{
  halp_meta(name, "Interpolator")
  halp_meta(c_name, "avnd_interpolator")
  halp_meta(author, "ossia team")
  halp_meta(category, "Control/Data processing")
  halp_meta(
      description, "Linear interpolation of multiple values using weight coefficients")
  halp_meta(uuid, "f47ac10b-58cc-4372-a567-0e02b2c3d479")

  struct
  {
    halp::val_port<"Weights", std::vector<float>> weights;
    
    struct : halp::spinbox_i32<"Input count", halp::range{0, 1024, 2}>
    {
      static std::function<void(Interpolator&, int)> on_controller_interaction()
      {
        return [](Interpolator& object, int value) {
          object.inputs.values.request_port_resize(value);
        };
      }
    } controller;

    halp::dynamic_port<halp::val_port<"Value {}", ossia::value>> values;
  } inputs;

  struct
  {
    halp::val_port<"Output", ossia::value> output;
  } outputs;

  template<typename T>
  T interpolate_scalar(const std::vector<T>& values, const std::vector<float>& weights) const
  {
    if (values.empty() || weights.empty())
      return T{};
    
    T result{};
    float total_weight = 0.0f;
    
    const int min_size = std::min(values.size(), weights.size());
    for (int i = 0; i < min_size; ++i)
    {
      result += values[i] * weights[i];
      total_weight += weights[i];
    }
    
    if (total_weight != 0.0f)
      result /= total_weight;
    
    return result;
  }

  template<typename VecType>
  VecType interpolate_vector(const std::vector<VecType>& values, const std::vector<float>& weights) const
  {
    if (values.empty() || weights.empty())
      return VecType{};
    
    VecType result{};
    float total_weight = 0.0f;
    
    const int min_size = std::min(values.size(), weights.size());
    for (int i = 0; i < min_size; ++i)
    {
      for (int j = 0; j < result.size(); ++j)
      {
        result[j] += values[i][j] * weights[i];
      }
      total_weight += weights[i];
    }
    
    if (total_weight != 0.0f)
    {
      for (auto& component : result)
        component /= total_weight;
    }
    
    return result;
  }

  std::vector<ossia::value> interpolate_value_vector(
    const std::vector<std::vector<ossia::value>>& value_vectors, 
    const std::vector<float>& weights) const
  {
    if (value_vectors.empty() || weights.empty())
      return {};
    
    const int min_inputs = std::min(value_vectors.size(), weights.size());
    if (min_inputs == 0)
      return {};
    
    const int max_size = std::max_element(
      value_vectors.begin(), value_vectors.begin() + min_inputs,
      [](const auto& a, const auto& b) { return a.size() < b.size(); }
    )->size();
    
    std::vector<ossia::value> result;
    result.reserve(max_size);
    
    for (int elem_idx = 0; elem_idx < max_size; ++elem_idx)
    {
      std::vector<ossia::value> element_values;
      std::vector<float> element_weights;
      
      for (int input_idx = 0; input_idx < min_inputs; ++input_idx)
      {
        if (elem_idx < value_vectors[input_idx].size())
        {
          element_values.push_back(value_vectors[input_idx][elem_idx]);
          element_weights.push_back(weights[input_idx]);
        }
      }
      
      if (!element_values.empty())
      {
        result.push_back(interpolate_values(element_values, element_weights));
      }
    }
    
    return result;
  }

  ossia::value interpolate_values(const std::vector<ossia::value>& values, const std::vector<float>& weights) const
  {
    if (values.empty() || weights.empty())
      return ossia::value{};

    const int min_size = std::min(values.size(), weights.size());
    if (min_size == 0)
      return ossia::value{};

    if (min_size == 1)
      return values[0];

    auto first_type = values[0].get_type();
    bool all_same_type = std::all_of(values.begin(), values.begin() + min_size,
      [first_type](const ossia::value& v) { return v.get_type() == first_type; });

    if (!all_same_type)
    {
      return values[0];
    }

    switch (first_type)
    {
      case ossia::val_type::FLOAT:
      {
        std::vector<float> float_values;
        for (int i = 0; i < min_size; ++i)
          float_values.push_back(*values[i].target<float>());
        return interpolate_scalar(float_values, weights);
      }
      case ossia::val_type::INT:
      {
        std::vector<int> int_values;
        for (int i = 0; i < min_size; ++i)
          int_values.push_back(*values[i].target<int>());
        float result = interpolate_scalar(
          std::vector<float>(int_values.begin(), int_values.end()), weights);
        return static_cast<int>(std::round(result));
      }
      case ossia::val_type::VEC2F:
      {
        std::vector<ossia::vec2f> vec_values;
        for (int i = 0; i < min_size; ++i)
          vec_values.push_back(*values[i].target<ossia::vec2f>());
        return interpolate_vector(vec_values, weights);
      }
      case ossia::val_type::VEC3F:
      {
        std::vector<ossia::vec3f> vec_values;
        for (int i = 0; i < min_size; ++i)
          vec_values.push_back(*values[i].target<ossia::vec3f>());
        return interpolate_vector(vec_values, weights);
      }
      case ossia::val_type::VEC4F:
      {
        std::vector<ossia::vec4f> vec_values;
        for (int i = 0; i < min_size; ++i)
          vec_values.push_back(*values[i].target<ossia::vec4f>());
        return interpolate_vector(vec_values, weights);
      }
      case ossia::val_type::LIST:
      {
        std::vector<std::vector<ossia::value>> value_vectors;
        for (int i = 0; i < min_size; ++i)
          value_vectors.push_back(*values[i].target<std::vector<ossia::value>>());
        return interpolate_value_vector(value_vectors, weights);
      }
      case ossia::val_type::BOOL:
      {
        std::vector<float> bool_values;
        for (int i = 0; i < min_size; ++i)
          bool_values.push_back(*values[i].target<bool>() ? 1.0f : 0.0f);
        float result = interpolate_scalar(bool_values, weights);
        return result >= 0.5f;
      }
      default:
        return values[0];
    }
  }

  void operator()()
  {
    const auto& weights = inputs.weights.value;
    
    if (weights.empty() || inputs.values.ports.empty())
    {
      outputs.output.value = ossia::value{};
      return;
    }

    std::vector<ossia::value> values;
    for (const auto& port : inputs.values.ports)
    {
      values.push_back(port.value);
    }

    outputs.output.value = interpolate_values(values, weights);
  }
};

}
