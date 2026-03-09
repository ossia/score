#include "ExprtkToCpp.cpp"
#include "ExprtkToCpp.hpp"

#include <Process/Dataflow/ControlWidgets.hpp>
#include <Process/Process.hpp>

#include <Fx/MathGenerator.hpp>

#include <ossia/detail/fmt.hpp>

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
  if(inl.exposed() == "in")
    var = "in";
  else if(inl.exposed() == "a")
    var = "a";
  else if(inl.exposed() == "b")
    var = "b";
  else if(inl.exposed() == "c)")
    var = "c";
  else
  {
    return "<< ERROR: Inlet not found: '" + inl.exposed().toStdString() + "' >>";
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
      exprtk_to_cpp(inl.value().get<std::string>(), true), variable);
}

// Helper: process exprtk expression lines into C++ code
// Returns {body_code, last_line_as_expression}
static std::pair<QString, QString> process_exprtk_expr(const std::string& exprtk)
{
  auto split = QString::fromStdString(exprtk).split('\n');
  if(split.empty())
    return {{}, {}};

  auto replace_line = [](QString& s) {
    s.replace("bool", "boolean");
    s.replace("var ", "exprtk_arithmetic ");
    s.replace(":=", "=");
    s = s.trimmed();
    s.replace(";;", ";");

    if(s.isEmpty() || s == ";")
      return;

    if(s.endsWith(";"))
    {
      s += "\n";
      return;
    }
    if(s.endsWith(";\n"))
      return;
    else if(s.endsWith("\n"))
    {
      if(!s.isEmpty())
        s.remove(s.size() - 1, 1);
      s += ";\n";
    }
    else
    {
      s += ";\n";
    }
  };

  for(auto it = split.begin(); it != split.end();)
  {
    replace_line(*it);
    if(it->isEmpty())
      it = split.erase(it);
    else
      ++it;
  }

  if(split.empty())
    return {{}, {}};

  QString body;
  for(int i = 0, N = split.size(); i < N - 1; i++)
    body += split[i];

  QString lastLine = split.back();
  if(lastLine.endsWith(";\n"))
    lastLine.resize(lastLine.size() - 2);
  else if(lastLine.endsWith(";"))
    lastLine.resize(lastLine.size() - 1);

  return {body, lastLine};
}

// Helper: find the "Expression" lineedit inlet and return its string value
static std::string get_expression(const Process::ProcessModel& self)
{
  auto it = ossia::find_if(
      self.inlets(), [](Process::Inlet* inl) { return inl->name().contains("Expression"); });
  SCORE_ASSERT(it != self.inlets().end());
  Process::LineEdit& inl = *safe_cast<Process::LineEdit*>(*it);
  return inl.value().get<std::string>();
}

////////////////////////////////////////////////////////////////////////////
// ArraygenCodeWriter
////////////////////////////////////////////////////////////////////////////

ArraygenCodeWriter::ArraygenCodeWriter(const Process::ProcessModel& proc) noexcept
    : CodeWriter{proc}
{
}

std::string ArraygenCodeWriter::initializer() const noexcept
{
  for(auto& i : this->self.inlets())
  {
    if(auto cc = qobject_cast<Process::ControlInlet*>(i))
    {
      if(cc->name() == "Size")
      {
        return fmt::format(".inputs = {{.sz = {{ {} }} }}", ossia::convert<int>(cc->value()));
      }
    }
  }
  return "";
}

std::string ArraygenCodeWriter::typeName() const noexcept
{
  return "ExprtkArrayGen";
}

std::string
ArraygenCodeWriter::accessInlet(const Id<Process::Port>& id) const noexcept
{
  const Process::Inlet& inl = *this->self.inlet(id);
  if(inl.exposed() == "sz")
    return fmt::format("({}.inputs.sz.value)", variable);
  return "<< ERROR: Inlet not found: '" + inl.exposed().toStdString() + "' >>";
}

std::string
ArraygenCodeWriter::accessOutlet(const Id<Process::Port>& id) const noexcept
{
  return fmt::format("({}.outputs.out.value)", variable);
}

std::string ArraygenCodeWriter::execute() const noexcept
{
  auto expr = get_expression(this->self);
  auto [body, lastLine] = process_exprtk_expr(expr);
  return fmt::format(
      R"_({{
  [](auto& object) {{
    using namespace std;
    int n = object.inputs.sz.value;
    object.outputs.out.value.resize(n);
    static float po = 0;
    float t = 0, dt = 0, pos = 0;
    for(int i = 0; i < n; i++) {{
      {}
      object.outputs.out.value[i] = {};
      po = object.outputs.out.value[i];
    }}
  }}({});
}})_",
      body.toStdString(), lastLine.toStdString(), variable);
}

////////////////////////////////////////////////////////////////////////////
// ArraymapCodeWriter
////////////////////////////////////////////////////////////////////////////

ArraymapCodeWriter::ArraymapCodeWriter(const Process::ProcessModel& proc) noexcept
    : CodeWriter{proc}
{
}

std::string ArraymapCodeWriter::initializer() const noexcept
{
  return "";
}

std::string ArraymapCodeWriter::typeName() const noexcept
{
  return "ExprtkArrayMap";
}

std::string
ArraymapCodeWriter::accessInlet(const Id<Process::Port>& id) const noexcept
{
  const Process::Inlet& inl = *this->self.inlet(id);
  if(inl.exposed() == "in")
    return fmt::format("({}.inputs.in.value)", variable);
  return "<< ERROR: Inlet not found: '" + inl.exposed().toStdString() + "' >>";
}

std::string
ArraymapCodeWriter::accessOutlet(const Id<Process::Port>& id) const noexcept
{
  return fmt::format("({}.outputs.out.value)", variable);
}

std::string ArraymapCodeWriter::execute() const noexcept
{
  auto expr = get_expression(this->self);
  auto [body, lastLine] = process_exprtk_expr(expr);
  return fmt::format(
      R"_({{
  [](auto& object) {{
    using namespace std;
    if(!object.inputs.in.value) return;
    auto& inv = *object.inputs.in.value;
    int n = inv.size();
    auto& ov = object.outputs.out.value.emplace();
    ov.resize(n);
    static float px = 0, po = 0;
    float t = 0, dt = 0, pos = 0;
    for(int i = 0; i < n; i++) {{
      float x = inv[i];
      {}
      ov[i] = {};
      px = x;
      po = ov[i];
    }}
    object.inputs.in.value = std::nullopt;
  }}({});
}})_",
      body.toStdString(), lastLine.toStdString(), variable);
}

}
