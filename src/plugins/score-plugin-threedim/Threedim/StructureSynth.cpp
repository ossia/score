#include "StructureSynth.hpp"

#include <Threedim/TinyObj.hpp>
#include <ssynth/Model/Builder.h>
#include <ssynth/Model/Rendering/ObjRenderer.h>
#include <ssynth/Parser/EisenParser.h>
#include <ssynth/Parser/Preprocessor.h>
#include <ssynth/Parser/Tokenizer.h>

#include <QDebug>
#include <QString>

#include <cmath>
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
  if (auto meshes = Threedim::ObjFromString(input, buf); !meshes.empty())
  {
    // The output type is a fixed positions+normals layout, but the OBJ we
    // just generated is not guaranteed to carry normals: RuleSet creates
    // TriangleRule on the fly for "triangle[p1;p2;p3]" scripts and
    // ObjRenderer::drawTriangle emits its face with no `vn` at all, so
    // ObjFromString then returns a position-only buffer. Derive the counts
    // and offsets from what the loader actually produced instead of
    // hard-coding the box/sphere layout, and synthesize flat per-face
    // normals when the source had none.
    int64_t total_vertices = 0;
    for (const auto& m : meshes)
      total_vertices += m.vertices;

    // ObjFromString writes one contiguous normals block for all shapes,
    // starting at the first mesh's normal_offset (in floats).
    int64_t normal_offset = meshes.front().normal_offset;
    if (!meshes.front().normals)
    {
      // Positions occupy [pos_offset, pos_offset + 3 * total) as one
      // contiguous block; append a matching flat-normal block.
      const int64_t pos_offset = meshes.front().pos_offset;
      normal_offset = int64_t(buf.size());
      buf.resize(buf.size() + total_vertices * 3);
      const float* pos = buf.data() + pos_offset;
      float* norm = buf.data() + normal_offset;
      for (int64_t t = 0; t < total_vertices / 3; t++)
      {
        const float* p0 = pos + 9 * t;
        const float* p1 = p0 + 3;
        const float* p2 = p0 + 6;
        const float ux = p1[0] - p0[0], uy = p1[1] - p0[1], uz = p1[2] - p0[2];
        const float vx = p2[0] - p0[0], vy = p2[1] - p0[1], vz = p2[2] - p0[2];
        float nx = uy * vz - uz * vy;
        float ny = uz * vx - ux * vz;
        float nz = ux * vy - uy * vx;
        const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 0.f)
        {
          nx /= len;
          ny /= len;
          nz /= len;
        }
        for (int v = 0; v < 3; v++)
        {
          *norm++ = nx;
          *norm++ = ny;
          *norm++ = nz;
        }
      }
    }

    return [b = std::move(buf), total_vertices, normal_offset](StrucSynth& s) mutable
    {
      std::swap(b, s.m_vertexData);
      s.outputs.geometry.mesh.buffers.main_buffer.elements = s.m_vertexData.data();
      s.outputs.geometry.mesh.buffers.main_buffer.element_count = s.m_vertexData.size();
      s.outputs.geometry.mesh.buffers.main_buffer.dirty = true;

      s.outputs.geometry.mesh.input.input1.byte_offset
          = sizeof(float) * normal_offset;
      s.outputs.geometry.mesh.vertices = total_vertices;
      s.outputs.geometry.dirty_mesh = true;
    };
  }
  else
  {
    return {};
  }
}

}
