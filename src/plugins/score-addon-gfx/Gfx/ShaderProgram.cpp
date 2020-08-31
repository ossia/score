#include "ShaderProgram.hpp"
#include <QShaderBaker>
#include <QRegularExpression>
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

std::optional<ProcessedProgram> ProgramCache::get(
    const ShaderProgram& program) noexcept
{
  auto it = programs.find(program);
  if(it != programs.end())
    return it->second;

  try
  {
    // TODO we could split the cache between
    // vertex and fragment, as the vertex one will likely be the same 99% of the time...

    // Parse ISF and get GLSL shaders
    isf::parser parser{program.vertex.toStdString(), program.fragment.toStdString()};
    auto isfVert = QString::fromStdString(parser.vertex());
    auto isfFrag = QString::fromStdString(parser.fragment());

    if (isfVert != program.vertex || isfFrag != program.fragment)
    {
      ProcessedProgram processed{{std::move(isfVert), std::move(isfFrag)}, parser.data(), {}, {}};

      // Add layout, location, etc
      updateToGlsl45(processed);

      // Compile the shaders to spirv, etc.
      static QShaderBaker& b = [] () -> QShaderBaker& {
          static QShaderBaker b;
          b.setGeneratedShaders({
                                  {QShader::SpirvShader, 100},
                                  {QShader::GlslShader, 330},
                                  {QShader::HlslShader, QShaderVersion(50)},
                                  {QShader::MslShader, QShaderVersion(12)},
                                });
          b.setGeneratedShaderVariants(
            {QShader::Variant{}, QShader::Variant{}, QShader::Variant{}, QShader::Variant{}});
          return b;
      }();

      b.setSourceString(processed.vertex.toUtf8(), QShader::VertexStage);
      QShader vertexS = b.bake();
      if(!b.errorMessage().isEmpty())
      {
        qDebug() << b.errorMessage();
      }

      b.setSourceString(processed.fragment.toUtf8(), QShader::FragmentStage);
      QShader fragmentS = b.bake();
      if (!b.errorMessage().isEmpty())
      {
        qDebug() << b.errorMessage();
      }

      if (vertexS.isValid() && fragmentS.isValid())
      {
        // We can store our shader in the cache
        processed.compiledVertex = std::move(vertexS);
        processed.compiledFragment = std::move(fragmentS);

        programs[program] = processed;
        return processed;
      }
    }
  } catch (...) { /* TODO: show error to the user */ }

  return std::nullopt;
}

}
