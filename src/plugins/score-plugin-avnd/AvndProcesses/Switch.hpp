#pragma once
#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/dataflow/value_port.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/string_list.hpp>
#include <rapidjson/document.h>

#include <cmath>

#include <vector>

namespace ao
{
struct Switch
{
  halp_meta(name, "Switch")
  halp_meta(c_name, "avnd_switch")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "ossia score")
  halp_meta(
      description,
      "Route each event to the first matching JSON scalar case. Strings and booleans "
      "match without coercion; numbers compare exactly after promotion to double; null "
      "matches impulse. "
      "Invalid literals never match. Row identities keep cables through reordering.")
  halp_meta(uuid, "51083c8f-aea0-4617-b026-34fd2793c818")

  struct
  {
    struct
    {
      halp_meta(name, "Input")
      ossia::value_port* value{};
      static constexpr bool is_event() { return true; }
    } input;
    struct : halp::string_list<"Cases">
    {
      halp_meta(
          description,
          "JSON scalars: quoted strings, numbers, true, false or null. "
          "The first matching row receives the event; null matches impulse.")
      void update(Switch& self) { self.compile(); }
      static std::function<void(Switch&, const halp::string_list_value&)>
      on_controller_interaction()
      {
        return [](Switch& self, const halp::string_list_value& rows) {
          self.outputs.cases.set_rows(rows);
        };
      }
    } cases;
  } inputs;

  struct case_port
  {
    halp_meta(name, "Case {}")
    ossia::value_port* value{};
    static constexpr bool is_event() { return true; }
  };
  struct
  {
    struct
    {
      halp_meta(name, "Unmatched")
      ossia::value_port* value{};
      static constexpr bool is_event() { return true; }
    } unmatched;
    halp::keyed_dynamic_port<case_port> cases;
  } outputs;

  struct Literal
  {
    enum Kind
    {
      invalid,
      null,
      boolean,
      number,
      string
    } kind{invalid};
    double numeric{};
    bool flag{};
    std::string text;

    bool matches(const ossia::value& value) const
    {
      switch(kind)
      {
        case null:
          return value.target<ossia::impulse>();
        case boolean:
          if(auto v = value.target<bool>())
            return *v == flag;
          return false;
        case number:
          if(auto v = value.target<int>())
            return double(*v) == numeric;
          if(auto v = value.target<float>())
            return double(*v) == numeric;
          return false;
        case string:
          if(auto v = value.target<std::string>())
            return *v == text;
          return false;
        default:
          return false;
      }
    }
  };
  std::vector<Literal> literals;

  using tick = ossia::token_request;
  ossia::exec_state_facade ossia_state;

  void compile()
  {
    literals.clear();
    literals.reserve(inputs.cases.value.size());
    for(const auto& [id, text] : inputs.cases.value)
    {
      auto& literal = literals.emplace_back();
      rapidjson::Document json;
      if(text.find('\0') != std::string::npos)
        continue;
      json.Parse<rapidjson::kParseValidateEncodingFlag>(text.data(), text.size());
      if(json.HasParseError())
        continue;
      if(json.IsNull())
        literal.kind = Literal::null;
      else if(json.IsBool())
      {
        literal.kind = Literal::boolean;
        literal.flag = json.GetBool();
      }
      else if(json.IsNumber())
      {
        literal.kind = Literal::number;
        literal.numeric = json.GetDouble();
      }
      else if(json.IsString())
      {
        literal.kind = Literal::string;
        literal.text.assign(json.GetString(), json.GetStringLength());
      }
    }
  }

  void operator()(const tick& t)
  {
    if(!inputs.input.value)
      return;
    const auto [start, frames] = ossia_state.timings(t);
    for(const auto& event : inputs.input.value->get_data())
    {
      if(event.timestamp < start || event.timestamp >= start + frames)
        continue;
      auto output = outputs.unmatched.value;
      for(std::size_t i = 0; i < literals.size() && i < outputs.cases.ports.size(); ++i)
        if(literals[i].matches(event.value))
        {
          output = outputs.cases.ports[i].value;
          break;
        }
      if(output)
        output->write_value(event.value, event.timestamp);
    }
  }
};
}
