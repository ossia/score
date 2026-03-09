#pragma once

#include <Process/CodeWriter.hpp>

#include <ossia/detail/algorithms.hpp>

#include <boost/core/demangle.hpp>

#include <avnd/concepts/dynamic_ports.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/introspection/output.hpp>
#include <fmt/format.h>

#include <utility>
#include <vector>

namespace Crousti
{

template <typename Info>
struct CodeWriter : Process::AvndCodeWriter
{
  using Process::AvndCodeWriter::AvndCodeWriter;

  // (struct_field_index, port_count) for each dynamic port field
  std::vector<std::pair<int, int>> dynamic_inlet_counts;
  std::vector<std::pair<int, int>> dynamic_outlet_counts;

  std::string typeName() const noexcept override
  {
    return boost::core::demangle(typeid(Info).name());
  }

  std::string accessInlet(const Id<Process::Port>& id) const noexcept override
  {
    if constexpr(avnd::dynamic_ports_input_introspection<Info>::size == 0)
    {
      return Process::AvndCodeWriter::accessInlet(id);
    }
    else
    {
      int raw_index = ossia::index_in_container(this->self.inlets(), id);

      // Compute prefix (extra ports before struct fields)
      int prefix = 0;
      if constexpr(avnd::audio_argument_processor<Info>)
        prefix++;
      else if constexpr(avnd::tag_cv<Info>)
        prefix++;
      prefix += avnd::messages_introspection<Info>::size;

      int adjusted = raw_index - prefix;

      int found_field = -1;
      int found_sub = -1;
      bool found_dynamic = false;
      int model_pos = 0;

      auto get_dynamic_count = [this](int field_idx) -> int {
        for(auto& [idx, count] : dynamic_inlet_counts)
          if(idx == field_idx)
            return count;
        return 0;
      };

      avnd::input_introspection<Info>::for_all(
          [&]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P>) {
        if(found_field >= 0)
          return;
        int num = 1;
        if constexpr(avnd::dynamic_ports_port<P>)
          num = get_dynamic_count(Idx);
        if(adjusted >= model_pos && adjusted < model_pos + num)
        {
          found_field = Idx;
          found_sub = adjusted - model_pos;
          if constexpr(avnd::dynamic_ports_port<P>)
            found_dynamic = true;
        }
        model_pos += num;
      });

      if(found_field < 0)
        return Process::AvndCodeWriter::accessInlet(id);

      if(found_dynamic)
        return fmt::format(
            "(avnd::input_introspection<{}>::field<{}>({}.inputs).ports[{}])",
            typeName(), found_field, this->variable, found_sub);
      else
        return fmt::format(
            "(avnd::input_introspection<{}>::field<{}>({}.inputs))", typeName(),
            found_field, this->variable);
    }
  }

  std::string accessOutlet(const Id<Process::Port>& id) const noexcept override
  {
    if constexpr(avnd::dynamic_ports_output_introspection<Info>::size == 0)
    {
      return Process::AvndCodeWriter::accessOutlet(id);
    }
    else
    {
      int raw_index = ossia::index_in_container(this->self.outlets(), id);

      int prefix = 0;
      if constexpr(avnd::audio_argument_processor<Info>)
        prefix++;
      else if constexpr(avnd::tag_cv<Info>)
      {
        using operator_ret = typename avnd::function_reflection_o<Info>::return_type;
        if constexpr(!std::is_void_v<operator_ret>)
          prefix++;
      }

      int adjusted = raw_index - prefix;

      int found_field = -1;
      int found_sub = -1;
      bool found_dynamic = false;
      int model_pos = 0;

      auto get_dynamic_count = [this](int field_idx) -> int {
        for(auto& [idx, count] : dynamic_outlet_counts)
          if(idx == field_idx)
            return count;
        return 0;
      };

      avnd::output_introspection<Info>::for_all(
          [&]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P>) {
        if(found_field >= 0)
          return;
        int num = 1;
        if constexpr(avnd::dynamic_ports_port<P>)
          num = get_dynamic_count(Idx);
        if(adjusted >= model_pos && adjusted < model_pos + num)
        {
          found_field = Idx;
          found_sub = adjusted - model_pos;
          if constexpr(avnd::dynamic_ports_port<P>)
            found_dynamic = true;
        }
        model_pos += num;
      });

      if(found_field < 0)
        return Process::AvndCodeWriter::accessOutlet(id);

      if(found_dynamic)
        return fmt::format(
            "(avnd::output_introspection<{}>::field<{}>({}.outputs).ports[{}])",
            typeName(), found_field, this->variable, found_sub);
      else
        return fmt::format(
            "(avnd_get_output<{}>({}, {}_cbs))",
            found_field, this->variable, this->variable);
    }
  }

  std::string postInitialize() const noexcept override
  {
    std::string result;
    for(auto& [field_idx, count] : dynamic_inlet_counts)
    {
      result += fmt::format(
          "avnd::input_introspection<{}>::field<{}>({}.inputs).ports.resize({});\n",
          typeName(), field_idx, this->variable, count);
    }
    for(auto& [field_idx, count] : dynamic_outlet_counts)
    {
      result += fmt::format(
          "avnd::output_introspection<{}>::field<{}>({}.outputs).ports.resize({});\n",
          typeName(), field_idx, this->variable, count);
    }
    return result;
  }
};
}
