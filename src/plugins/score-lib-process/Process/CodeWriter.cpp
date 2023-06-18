#include "CodeWriter.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/network/value/value_conversion.hpp>

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
  return {};
}

std::string AvndCodeWriter::initializer() const noexcept
{
  std::string init_list;
  for(const Process::Inlet* in : this->self.inlets())
  {
    if(auto c = qobject_cast<const Process::ControlInlet*>(in))
    {
      float val = ossia::convert<float>(c->value());
      auto name = c->name();

      if(auto it = name.indexOf('('); it != -1)
        name = name.mid(0, it);
      name.replace(" ", "_");
      name.replace(".", "_");
      init_list += fmt::format(".{} = {{ {} }}, ", name.toStdString(), val);
    }
  }
  return fmt::format(".inputs = {{ {} }}", init_list);
}

std::string AvndCodeWriter::accessInlet(const Id<Port>& id) const noexcept
{
  int index = ossia::index_in_container(this->self.inlets(), id);
  return fmt::format(
      "(avnd::input_introspection<decltype({})>::get<{}>({}.inputs)."
      "value)",
      variable, index, variable);
}

std::string AvndCodeWriter::accessOutlet(const Id<Port>& id) const noexcept
{
  int index = ossia::index_in_container(this->self.outlets(), id);
  return fmt::format(
      "(avnd::output_introspection<decltype({})>::get<{}>({}.outputs)."
      "value)",
      variable, index, variable);
}

std::string AvndCodeWriter::execute() const noexcept
{
  return fmt::format("avnd_call({});", variable);
}

}
