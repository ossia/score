#include "CodeWriter.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/network/value/value_conversion.hpp>
#include <ossia/network/domain/domain.hpp>

#include <fmt/format.h>
namespace Process
{

CodeWriter::CodeWriter(const ProcessModel& p) noexcept
    : self{p}
{
}

CodeWriter::~CodeWriter() { }

std::string DummyCodeWriter::initializer() const noexcept
{
  return "";
}

std::string DummyCodeWriter::typeName() const noexcept
{
  return "Dummy";
}

std::string DummyCodeWriter::accessInlet(const Id<Port>& id) const noexcept
{
  return "Dummy::variable";
}

std::string DummyCodeWriter::accessOutlet(const Id<Port>& id) const noexcept
{
  return "Dummy::variable";
}

std::string DummyCodeWriter::execute() const noexcept
{
  qDebug() << "!! Generating Dummy code writing for: " << typeid(self).name();
  return {};
}

struct PortValueToInitString
{
  std::string res;
  void operator()(const auto& domain, const ossia::impulse& value)
  {
    res = "halp::impulse{}";
  }
  void operator()(const auto& domain, const float& value)
  {
    res = std::to_string(value);
  }
  void operator()(const auto& domain, const ossia::vec2f& value)
  {
    res = fmt::format("std::array<float, 2>{{ {}, {} }}", value[0], value[1]);
  }
  void operator()(const auto& domain, const ossia::vec3f& value)
  {
    res = fmt::format(
        "std::array<float, 3>{{ {}, {} , {} }}", value[0], value[1], value[2]);
  }
  void operator()(const auto& domain, const ossia::vec4f& value)
  {
    res = fmt::format(
        "std::array<float, 4>{{ {}, {} , {}, {} }}", value[0], value[1], value[2],
        value[3]);
  }
  void operator()(const auto& domain, const int& value) { res = std::to_string(value); }
  void operator()(const auto& domain, const bool& value)
  {
    res = value ? "true" : "false";
  }
  void operator()(const auto& domain, const std::string& value)
  {
    res.reserve(value.size() + 2);
    res += "\"";
    res += value;
    res += "\"";
  }
  void operator()(const auto& domain, const std::vector<ossia::value>& value)
  {
    res += "{";
    for(auto& v : value)
    {
      //FIXME
      res += ossia::convert<float>(v);
      res += ", ";
    }
    res += "}";
  }
  void operator()(const auto& domain, const ossia::value_map_type& value)
  {
    res += "{";
    for(auto& [k, v] : value)
    {
      //FIXME
      res += "{";
      res += "\"" + k + "\"";
      res += ", ";
      res += ossia::convert<float>(v);
      res += "}";
      res += ", ";
    }
    res += "}";
  }

  void
  operator()(const ossia::domain_base<std::string>& domain, const std::string& value)
  {
    auto i = ossia::index_in_container(domain.values, value);
    if(i != -1)
      res = std::to_string(i);
    else
    {
      res.reserve(value.size() + 2);
      res += "\"";
      res += value;
      res += "\"";
    }
  }
  void operator()(const auto& domain, const auto& value) = delete;
};

std::string AvndCodeWriter::initializer() const noexcept
{
  std::string init_list;
  for(const Process::Inlet* in : this->self.inlets())
  {
    if(auto c = qobject_cast<const Process::ControlInlet*>(in))
    {
      PortValueToInitString str;
      if(auto dom = c->domain().get())
        ossia::apply(str, dom.v, c->value().v);
      else
        str.res = "";
      // qDebug() << ossia::value_to_pretty_string(c->value()) << " => " << str.res;
      auto name = c->exposed();

      if(auto it = name.indexOf('('); it != -1)
        name = name.mid(0, it);
      name.replace(" ", "_");
      name.replace(".", "_");
      init_list += fmt::format(".{} = {{ {} }}, ", name.toStdString(), str.res);
    }
  }
  return fmt::format(".inputs = {{ {} }}", init_list);
}

std::string AvndCodeWriter::accessInlet(const Id<Port>& id) const noexcept
{
  int index = ossia::index_in_container(this->self.inlets(), id);
  return fmt::format(
      "(avnd::input_introspection<{}>::field<{}>({}.inputs))", typeName(), index,
      variable);
}

std::string AvndCodeWriter::accessOutlet(const Id<Port>& id) const noexcept
{
  int index = ossia::index_in_container(this->self.outlets(), id);
  return fmt::format(
      "(avnd::output_introspection<{}>::field<{}>({}.outputs))", typeName(), index,
      variable);
}

std::string AvndCodeWriter::execute() const noexcept
{
  return fmt::format(
      "avnd_clean_outputs({0}); avnd_call({0}); avnd_clean_inputs({0}); ", variable);
}

}
