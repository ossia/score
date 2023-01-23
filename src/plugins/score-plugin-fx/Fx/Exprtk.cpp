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

  return "{\n" + pre + "\n" + code.toStdString() + "\n" + post + "\n}\n";
}

}
