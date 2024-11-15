#include "ExprtkToCpp.cpp"
#include "ExprtkToCpp.hpp"

#include <Process/Dataflow/ControlWidgets.hpp>
#include <Process/Process.hpp>

#include <Fx/MathGenerator.hpp>

#include <boost/algorithm/string/replace.hpp>

#include <QList>
#include <QString>

#include <string>

namespace Nodes
{
MathMappingCodeWriter::MathMappingCodeWriter(const Process::ProcessModel& proc) noexcept
    : CodeWriter{proc}
{
  auto it = ossia::find_if(this->self.inlets(), [](Process::Inlet* inl) {
    return inl->name().contains("Expression");
  });
  SCORE_ASSERT(it != this->self.inlets().end());
  Process::LineEdit& inl = *safe_cast<Process::LineEdit*>(*it);
  const auto& exprtk = inl.value().get<std::string>();

  in_vector = exprtk.find("xv[") != std::string::npos;
  ret_vector = exprtk.find("return") != std::string::npos;
}

std::string MathMappingCodeWriter::initializer() const noexcept
{
  Process::ControlInlet* a{};
  Process::ControlInlet* b{};
  Process::ControlInlet* c{};

  for(auto& i : this->self.inlets())
  {
    if(auto cc = qobject_cast<Process::ControlInlet*>(i))
    {
      if(cc->name() == "Param (a)")
        a = cc;
      else if(cc->name() == "Param (b)")
        b = cc;
      else if(cc->name() == "Param (c)")
        c = cc;
    }
  }
  if(a && b && c)
  {
    return fmt::format(
        ".inputs = {{.a = {{ {} }}, .b = {{ {} }}, .c = {{ {} }} }}",
        ossia::convert<float>(a->value()), ossia::convert<float>(b->value()),
        ossia::convert<float>(c->value()));
  }
  else
  {
    return "";
  }
}

std::string MathMappingCodeWriter::typeName() const noexcept
{
  if(in_vector && ret_vector)
    return "ExprtkMapper_iV_oV";
  else if(in_vector && !ret_vector)
    return "ExprtkMapper_iV_oS";
  else if(!in_vector && ret_vector)
    return "ExprtkMapper_iS_oV";
  else
    return "ExprtkMapper_iS_oS";
}

std::string
MathMappingCodeWriter::accessInlet(const Id<Process::Port>& id) const noexcept
{
  // FIXME we should not have the LineEdit input
  const Process::Inlet& inl = *this->self.inlet(id);
  std::string var;
  if(inl.name() == "in")
    var = "in";
  else if(inl.name() == "Param (a)")
    var = "a";
  else if(inl.name() == "Param (b)")
    var = "b";
  else if(inl.name() == "Param (c)")
    var = "c";
  else
  {
    return "ERROR: " + inl.name().toStdString();
  }

  return fmt::format("({}.inputs.{}.value)", variable, var);
}

std::string
MathMappingCodeWriter::accessOutlet(const Id<Process::Port>& id) const noexcept
{
  return fmt::format("({}.outputs.out.value)", variable);
}

std::string MathMappingCodeWriter::execute() const noexcept
{
  auto it = ossia::find_if(this->self.inlets(), [](Process::Inlet* inl) {
    return inl->name().contains("Expression");
  });
  SCORE_ASSERT(it != this->self.inlets().end());
  Process::LineEdit& inl = *safe_cast<Process::LineEdit*>(*it);
  return fmt::format(
      R"_({{
    {}({});
}})_",
      exprtk_to_cpp(inl.value().get<std::string>()), variable);
}

}
