#include <Fx/MathGenerator.hpp>

#include <QList>
#include <QString>

#include <string>

namespace Nodes
{

std::string exprtk_to_cpp(std::string exprtk) noexcept
{
  auto split = QString::fromStdString(exprtk).split('\n');
  if(split.empty())
    return "[] (auto& object) { }";

  std::string pre = R"_(
  [](auto& object) {
    using namespace std;
    static float px = 0;
    static float po = 0;
    static float pa = 0;
    static float pb = 0;
    static float pc = 0;
    float& x = object.inputs.in.value;
    float& o = object.outputs.out.value;
    float& a = object.inputs.a.value;
    float& b = object.inputs.b.value;
    float& c = object.inputs.c.value;
    float& m1 = object.inputs.m1.value;
    float& m2 = object.inputs.m2.value;
    float& m3 = object.inputs.m3.value;
    float t = 0;
    float dt = 0;
    float pos = 0;
    float fs = 44100;
)_";

  std::string post = R"_(
    px = x;
    po = o;

    pa = a;
    pb = b;
    pc = c;
    return o;
  }
)_";

  QString code;

  auto replace_line = [](QString& s) {
    //   => convert var ([a-zA-Z0-9]+) := (.*)  into auto \1 = \2;
    //   => convert ([a-zA-Z0-9]+) := (.*)  into \1 = \2;
    //   => try to convert [0-9] [a-zA-Z] into \1 * \2
    //   => try to convert [a-zA-Z] [0-9]  into \1 * \2
    //   => try to convert [0-9] \(   into \1 * \(
    //   => try to convert \) [0-9]   into \) * \1
    //   => try to convert res := if(cond, A, B) into  [&] { if(cond) { return A; } else { return B; }; }()
    //   => {
    //        float x = /* input */;
    //        static float px = 0;
    //        float m1 = 0, m2 = 0, m3 = 0;
    //        <<CODE>> (except last line)
    //        output = <<LAST LINE OF CODE>>;
    //        px = x;
    //      }

    s.replace("var ", "float ");
    s.replace(":=", "=");
  };
  // First process the main part of the code
  for(int i = 0, N = split.size(); i < N - 1; i++)
  {
    QString s = split[i];
    replace_line(s);

    code += s + ";\n";
  }

  // Process the last line which may look like "x + 1"
  replace_line(split.back());
  code += "o = " + split.back() + ";\n";

  return pre + "\n" + code.toStdString() + "\n" + post;
}

std::string MathMappingCodeWriter::initializer() const noexcept
{

  Process::ControlInlet*
      a{}; // = this->self.findChild<Process::ControlInlet*>("Param (a)");
  Process::ControlInlet*
      b{}; // = this->self.findChild<Process::ControlInlet*>("Param (b)");
  Process::ControlInlet*
      c{}; // = this->self.findChild<Process::ControlInlet*>("Param (c)");

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
  return "ExprtkMapper";
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
