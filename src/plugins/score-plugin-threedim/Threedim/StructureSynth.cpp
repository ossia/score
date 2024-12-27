#include "StructureSynth.hpp"

#include <Threedim/TinyObj.hpp>
#include <ssynth/Model/Builder.h>
#include <ssynth/Model/Rendering/ObjRenderer.h>
#include <ssynth/Parser/EisenParser.h>
#include <ssynth/Parser/Preprocessor.h>
#include <ssynth/Parser/Tokenizer.h>

#include <QDebug>
#include <QString>

#include <iostream>

namespace Threedim
{
static auto CreateObj(const QString& input)
try
{
  /*
  QString input = R"_(set maxdepth 2000
{ a 0.9 hue 30 } R1

rule R1 w 10 {
{ x 1  rz 3 ry 5  } R1
{ s 1 1 0.1 sat 0.9 } box
}

rule R1 w 10 {
{ x 1  rz -3 ry 5  } R1
{ s 1 1 0.1 } box
}
)_";
*/
  ssynth::Parser::Preprocessor p;
  auto preprocessed = p.Process(input);

  ssynth::Parser::Tokenizer t{std::move(preprocessed)};
  ssynth::Parser::EisenParser e{t};

  auto ruleset = std::unique_ptr<ssynth::Model::RuleSet>{e.parseRuleset()};
  ruleset->resolveNames();
  ruleset->dumpInfo();

  ssynth::Model::Rendering::ObjRenderer obj{10, 10, true, false};
  ssynth::Model::Builder b(&obj, ruleset.get(), true);
  b.build();

  QByteArray data;
  {
    QTextStream ts(&data);
    obj.writeToStream(ts);
    ts.flush();
  }

  return data.toStdString();
}
catch (const std::exception& e)
{
  qDebug() << e.what();
  return std::string{};
}
catch (...)
{
  return std::string{};
}

void StrucSynth::operator()() { }

std::function<void(StrucSynth&)> StrucSynth::worker::work(std::string_view in)
{
  if (in.empty())
    return {};

  auto input = CreateObj(QString::fromUtf8(in.data(), in.size()));
  if (input.empty())
    return {};

  Threedim::float_vec buf;
  if (auto mesh = Threedim::ObjFromString(input, buf); !mesh.empty())
  {
    return [b = std::move(buf)](StrucSynth& s) mutable
    {
      std::swap(b, s.m_vertexData);
      s.outputs.geometry.mesh.buffers.main_buffer.data = s.m_vertexData.data();
      s.outputs.geometry.mesh.buffers.main_buffer.size = s.m_vertexData.size();
      s.outputs.geometry.mesh.buffers.main_buffer.dirty = true;

      s.outputs.geometry.mesh.input.input1.offset
          = sizeof(float) * (s.m_vertexData.size() / 2);
      s.outputs.geometry.mesh.vertices = s.m_vertexData.size() / (2 * 3);
      s.outputs.geometry.dirty_mesh = true;
    };
  }
  else
  {
    return {};
  }
}

}
