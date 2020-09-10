#include "ShaderProgram.hpp"
#include <QShaderBaker>
#include <QRegularExpression>
#include <QFile>
#include <Gfx/Graph/shadercache.hpp>
#include <ossia/detail/flat_map.hpp>
namespace Gfx
{

namespace
{
void updateToGlsl45(ShaderProgram& program)
{
  static const QRegularExpression out_expr{R"_(^out\s+(\w+)\s+(\w+)\s*;)_", QRegularExpression::MultilineOption};
  static const QRegularExpression in_expr{R"_(^in\s+(\w+)\s+(\w+)\s*;)_", QRegularExpression::MultilineOption};

  ossia::flat_map<QString, int> attributes_locations_map;

  // First fixup the vertex shader and look for all the attributes
  {
    // location 0 is taken by fragColor - we start at 1.
    int cur_location = 1;

    auto match_idx = program.vertex.indexOf(out_expr);
    while(match_idx != -1)
    {
      const QStringRef partialString = program.vertex.midRef(match_idx);
      const auto& match = out_expr.match(partialString);
      const int len = match.capturedLength(0);
      attributes_locations_map[match.captured(2)] = cur_location;

      program.vertex.insert(match_idx, QString("layout(location = %1) ").arg(cur_location));
      cur_location++;

      match_idx = program.vertex.indexOf(out_expr, match_idx + len);
    }
  }

  // Then move on to the fragment shader, and reuse the same locations.
  {
    auto match_idx = program.fragment.indexOf(in_expr);
    while(match_idx != -1)
    {
      const QStringRef partialString = program.fragment.midRef(match_idx);
      const auto& match = in_expr.match(partialString);
      const int len = match.capturedLength(0);

      const int loc = attributes_locations_map[match.captured(2)];

      program.fragment.insert(match_idx, QString("layout(location = %1) ").arg(loc));

      match_idx = program.fragment.indexOf(in_expr, match_idx + len);
    }
  }
}
}

ProgramCache& ProgramCache::instance() noexcept
{
  static ProgramCache cache;
  return cache;
}

std::pair<std::optional<ProcessedProgram>, QString> ProgramCache::get(const ShaderProgram& program) noexcept
{
  auto it = programs.find(program);
  if(it != programs.end())
    return {it->second, QString{}};

  try
  {
    // Parse ISF and get GLSL shaders
    isf::parser parser{program.vertex.toStdString(), program.fragment.toStdString()};

    auto isfVert = QString::fromStdString(parser.vertex());
    if(isfVert.isEmpty())
    {
      return {std::nullopt, "Not a valid ISF vertex shader"};
    }

    auto isfFrag = QString::fromStdString(parser.fragment());
    if(isfFrag.isEmpty())
    {
      return {std::nullopt, "Not a valid ISF fragment shader"};
    }

    if (isfVert != program.vertex || isfFrag != program.fragment)
    {
      ProcessedProgram processed{{std::move(isfVert), std::move(isfFrag)}, parser.data(), {}, {}};

      // Add layout, location, etc
      updateToGlsl45(processed);

      // Create QShader objects
      auto [vertexS, vertexError] = ShaderCache::get(processed.vertex.toUtf8(), QShader::VertexStage);
      if(!vertexError.isEmpty())
      {
        qDebug() << vertexError;
        return {std::nullopt, "Vertex shader error: " + vertexError};
      }

      auto [fragmentS, fragmentError] = ShaderCache::get(processed.fragment.toUtf8(), QShader::FragmentStage);
      if(!fragmentError.isEmpty())
      {
        qDebug() << fragmentError;
        return {std::nullopt, "Fragment shader error: " + fragmentError};
      }

      if (vertexS.isValid() && fragmentS.isValid())
      {
        // We can store our shader in the cache
        processed.compiledVertex = std::move(vertexS);
        processed.compiledFragment = std::move(fragmentS);

        programs[program] = processed;
        return {processed, {}};
      }
    }
    else
    {
      return {std::nullopt, "Not a valid ISF shader"};
    }
  }
  catch (const std::runtime_error& error)
  {
    return {std::nullopt, QString("ISF error: %1").arg(error.what())};
  }
  catch (...)
  {
    return {std::nullopt, "Unknown error"};
  }

  return {std::nullopt, "Unknown error"};
}

const QString defaultISFVertex = QStringLiteral(
R"_(void main(void)	{
  isf_vertShaderInit();
}
)_");

ShaderProgram programFromFragmentShaderPath(const QString& fsFilename, QByteArray fsData)
{
  // ISF works by storing a vertex shader next to the fragment shader.
  QString vertexName = fsFilename;
  vertexName.replace(".frag", ".vert");
  vertexName.replace(".fs", ".vs");

  QString vertexData = defaultISFVertex;
  if(vertexName != fsFilename)
  {
    if(QFile vertexFile{vertexName};
       vertexFile.exists() && vertexFile.open(QIODevice::ReadOnly))
    {
      vertexData = vertexFile.readAll();
    }
  }

  if(fsData.isEmpty())
  {
    if(QFile fsFile{fsFilename};
       fsFile.exists() && fsFile.open(QIODevice::ReadOnly))
    {
      fsData = fsFile.readAll();
    }
  }

  return {vertexData, fsData};
}
}
