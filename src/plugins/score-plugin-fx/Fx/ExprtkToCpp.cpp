#include "ExprtkToCpp.hpp"
#include <QString>
#include <QList>
#include <boost/algorithm/string/replace.hpp>
namespace Nodes
{
static struct
{
  const std::string pre = R"_(
  [](auto& object) {
    using namespace std;
    static %IVECT% px%IVECF% = {};
    static %OVECT% po%OVECF% = {};
    static float pa = 0;
    static float pb = 0;
    static float pc = 0;
    auto& x%IVECF% = object.inputs.in.value;
    auto& o%OVECF% = object.outputs.out.value;
    auto& a = object.inputs.a.value;
    auto& b = object.inputs.b.value;
    auto& c = object.inputs.c.value;
    auto& m1 = object.inputs.m1.value;
    auto& m2 = object.inputs.m2.value;
    auto& m3 = object.inputs.m3.value;
    float t = 0;
    float dt = 0;
    float pos = 0;
    float fs = 44100;
)_";

  const std::string post = R"_(
    value_adapt(px%IVECF%, x%IVECF% %IVECA%);
    value_adapt(po%OVECF%, o%OVECF% %OVECA%);

    pa = a;
    pb = b;
    pc = c;
)_";

} exprtk_to_cpp_impl;


std::string exprtk_to_cpp(std::string exprtk) noexcept
{
  const bool in_vector = exprtk.find("xv[") != std::string::npos;
  const bool ret_vector = exprtk.find("return") != std::string::npos;

  auto split = QString::fromStdString(exprtk).split('\n');
  if(split.empty())
    return "[] (auto& object) { }";

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
    {
      return;
    }
    else if(s.endsWith("\n"))
    {
      s.removeLast();
      s += ";\n";
    }
    else
    {
      s += ";\n";
    }
  };

  // First process the main part of the code
  for(auto it = split.begin(); it != split.end(); )
  {
    QString& s = *it;
    replace_line(s);
    if(s.isEmpty())
    {
      it = split.erase(it);
    }
    else
    {
      ++it;
    }
  }

  for(int i = 0, N = split.size(); i < N - 1; i++)
  {
    code += split[i];
  }


  // Process the last line which may look like "x + 1"
  auto pre = exprtk_to_cpp_impl.pre;
  auto post = exprtk_to_cpp_impl.post;
  if(in_vector)
  {
    boost::replace_all(pre, "%IVECT%", "std::vector<float>");
    boost::replace_all(pre, "%IVECF%", "v");
    boost::replace_all(pre, "%IVECA%", "");
    boost::replace_all(post, "%IVECT%", "std::vector<float>");
    boost::replace_all(post, "%IVECF%", "v");
    boost::replace_all(post, "%IVECA%", "");
  }
  else
  {
    boost::replace_all(pre, "%IVECT%", "float");
    boost::replace_all(pre, "%IVECF%", "");
    boost::replace_all(pre, "%IVECA%", ".v");
    boost::replace_all(post, "%IVECT%", "float");
    boost::replace_all(post, "%IVECF%", "");
    boost::replace_all(post, "%IVECA%", ".v");
  }

  if(ret_vector)
  {
    boost::replace_all(pre, "%OVECT%", "std::vector<float>");
    boost::replace_all(pre, "%OVECF%", "v");
    boost::replace_all(pre, "%OVECA%", "");
    boost::replace_all(post, "%OVECT%", "std::vector<float>");
    boost::replace_all(post, "%OVECF%", "v");
    boost::replace_all(post, "%OVECA%", "");
  }
  else
  {
    boost::replace_all(pre, "%OVECT%", "float");
    boost::replace_all(pre, "%OVECF%", "");
    boost::replace_all(pre, "%OVECA%", ".v");
    boost::replace_all(post, "%OVECT%", "float");
    boost::replace_all(post, "%OVECF%", "");
    boost::replace_all(post, "%OVECA%", ".v");
  }

  if(!ret_vector)
  {
    split.back().resize(split.back().size() - 2);
    code += "value_adapt(o, " + split.back() + ");\n";
  }
  else
  {
    // FIXME
    //SCORE_TODO;

    code += "value_adapt(ov, 0);\n";
  }

  return pre + "\n" + code.toStdString() + "\n"
         + post + "\n}\n";
}

}
