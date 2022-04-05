#include "ShaderProgram.hpp"

#include <score/application/ApplicationContext.hpp>
#include <Gfx/Graph/ShaderCache.hpp>
#include <Library/LibrarySettings.hpp>

#include <ossia/detail/flat_map.hpp>

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
namespace Gfx
{

namespace
{

QStringList shaderIncludePaths()
{
  // Resolve includes ; for now we have one hardcoded library...
  QStringList shaderIncludePath;

  // FIXME refactor that !
  auto& lib_settings = score::AppContext().settings<Library::Settings::Model>();
  {
  QString lib_path = lib_settings.getPackagesPath() + "/lygia";
  if(QDir{}.exists(lib_path))
    shaderIncludePath.append(lib_path);
  }

  return shaderIncludePath;
}

void updateToGlsl45(ShaderSource& program)
{
  static const QRegularExpression out_expr{
      R"_(^out\s+(\w+)\s+(\w+)\s*(\[([0-9]+)\])?\s*;)_", QRegularExpression::MultilineOption};
  static const QRegularExpression in_expr{
      R"_(^in\s+(\w+)\s+(\w+)\s*(\[([0-9]+)\])?\s*;)_", QRegularExpression::MultilineOption};

  ossia::flat_map<QString, int> attributes_locations_map;

  // First fixup the vertex shader and look for all the attributes
  {
    // location 0 is taken by fragColor - we start at 1.
    int cur_location = 1;

    auto match_idx = program.vertex.indexOf(out_expr);
    while (match_idx != -1)
    {
      const QString partialString = program.vertex.mid(match_idx);
      const auto& match = out_expr.match(partialString);
      const int len = match.capturedLength(0);
      attributes_locations_map[match.captured(2)] = cur_location;

      program.vertex.insert(
          match_idx, QString("layout(location = %1) ").arg(cur_location));

      int locationIncrease = 1;
      if(match.lastCapturedIndex() == 4)
      {
        bool ok = false;
        int arraySize = match.capturedView(4).toInt(&ok);
        if(ok)
        {
          locationIncrease = arraySize;
        }
        else
        {
          arraySize = 1;
        }
      }
      cur_location += locationIncrease;

      match_idx = program.vertex.indexOf(out_expr, match_idx + len);
    }
  }

  // Then move on to the fragment shader, and reuse the same locations.
  {
    auto match_idx = program.fragment.indexOf(in_expr);
    while (match_idx != -1)
    {
      const QString partialString = program.fragment.mid(match_idx);
      const auto& match = in_expr.match(partialString);
      const int len = match.capturedLength(0);

      const int loc = attributes_locations_map[match.captured(2)];

      program.fragment.insert(
          match_idx, QString("layout(location = %1) ").arg(loc));

      match_idx = program.fragment.indexOf(in_expr, match_idx + len);
    }
  }

  // Remove lowp, highp, etc
  program.vertex.remove("lowp ");
  program.vertex.remove("mediump ");
  program.vertex.remove("highp ");
  program.fragment.remove("lowp ");
  program.fragment.remove("mediump ");
  program.fragment.remove("highp ");
}

static bool resolveGLSLIncludes(QByteArray& data, const QStringList& includes, QString rootPath, int iterations);

static std::optional<QByteArray> resolveFile_relative(const QString& name, const QStringList& includes, const QString& rootPath, int iterations)
{
  QFile f{rootPath + "/" + name};
  if(f.open(QIODevice::ReadOnly))
  {
    QByteArray res = f.readAll();
    if(resolveGLSLIncludes(res, includes, QFileInfo{f}.absolutePath(), iterations))
      return res;
    return std::nullopt;
  }
  return {};
}

static std::optional<QByteArray> resolveFile_in_paths(const QString& name, const QStringList& includes, int iterations)
{
  for(auto& path : includes)
  {
    if(auto res = resolveFile_relative(name, includes, path, iterations))
      return res;
  }
  return std::nullopt;
}

static std::optional<QByteArray> resolveFile_quotes(const QString& name, const QStringList& includes, const QString& rootPath, int iterations)
{
  if(auto res = resolveFile_relative(name, includes, rootPath, iterations))
    return res;
  if(auto res = resolveFile_in_paths(name, includes, iterations))
    return res;
  return std::nullopt;
}

static std::optional<QByteArray> resolveFile_brackets(const QString& name, const QStringList& includes, const QString& rootPath, int iterations)
{
  if(auto res = resolveFile_in_paths(name, includes, iterations))
    return res;
  return std::nullopt;
}

static bool resolveGLSLIncludes(QByteArray& data, const QStringList& includes, QString rootPath, int iterations)
{
  iterations++;
  if(iterations > 30) {
    qDebug() << "More than 30 iterations, shader include loop likely. Stopping.";
    return false;
  }
  int idx = data.indexOf("#include");
  if(idx == -1)
    return true;

  int end_line = data.indexOf('\n', idx);
  int len = end_line - idx;
  static QRegularExpression quoted_include{R"_(#include\s*"(.*)")_"};
  auto cap = quoted_include.match(data.mid(idx, len)).capturedTexts();
  if(cap.size() == 2) {
    if(auto f = resolveFile_quotes(cap[1], includes, rootPath, iterations))
    {
      data.replace(idx, len, *f);
    }
    else
    {
      qDebug().noquote() << "Could not resolve: " << cap[0] << " while processing shader";
      return false;
    }
  }
  else
  {
    static QRegularExpression bracket_include{R"_(#include\s*<(.*)>)_"};
    auto cap = bracket_include.match(data.mid(idx, len)).capturedTexts();
    if(cap.size() == 2) {
      if(auto f = resolveFile_brackets(cap[1], includes, rootPath, iterations))
      {
        data.replace(idx, len, *f);
      }
      else
      {
        qDebug().noquote() << "Could not resolve: " << cap[0] << " while processing shader";
        return false;
      }
    }
  }

  return resolveGLSLIncludes(data, includes, rootPath, iterations);
}

}

ProgramCache& ProgramCache::instance() noexcept
{
  static ProgramCache cache;
  return cache;
}

std::pair<std::optional<ProcessedProgram>, QString>
ProgramCache::get(const ShaderSource& program) noexcept
{
  auto it = programs.find(program);
  if (it != programs.end())
    return {it->second, QString{}};

  try
  {
    // Resolve includes
    QByteArray source_frag = program.fragment.toUtf8();
    QByteArray source_vert = program.vertex.toUtf8();
    resolveGLSLIncludes(source_frag, shaderIncludePaths(), {}, 0);
    resolveGLSLIncludes(source_vert, shaderIncludePaths(), {}, 0);

    // Parse ISF and get GLSL shaders
    isf::parser parser{
        source_vert.toStdString(),
        source_frag.toStdString(),
        450,
        isf::parser::ShaderType::ISF};

    auto isfVert = QByteArray::fromStdString(parser.vertex());
    auto isfFrag = QByteArray::fromStdString(parser.fragment());

    if(qEnvironmentVariableIsSet("SCORE_DUMP_SHADERS"))
    {
      qDebug().noquote() << "\n\n ======= VERTEX ======== \n\n" << isfVert;
      qDebug().noquote() << "\n\n ======= FRAGMENT ======== \n\n" << isfFrag;
    }

    if (isfVert.isEmpty())
    {
      return {std::nullopt, "Not a valid ISF vertex shader"};
    }

    if (isfFrag.isEmpty())
    {
      return {std::nullopt, "Not a valid ISF fragment shader"};
    }

    if (isfVert != source_vert || isfFrag != source_frag)
    {
      ProcessedProgram processed{
          ShaderSource{isfVert, isfFrag}, parser.data(), {}, {}};

      // Add layout, location, etc
      updateToGlsl45(processed);

      // Create QShader objects
      auto [vertexS, vertexError]
          = score::gfx::ShaderCache::get(processed.vertex.toUtf8(), QShader::VertexStage);
      if (!vertexError.isEmpty())
      {
        qDebug().noquote() << vertexError;
        qDebug().noquote() << processed.vertex.toUtf8();
        return {std::nullopt, "Vertex shader error: " + vertexError};
      }

      auto [fragmentS, fragmentError] = score::gfx::ShaderCache::get(
          processed.fragment.toUtf8(), QShader::FragmentStage);
      if (!fragmentError.isEmpty())
      {
        qDebug().noquote() << fragmentError;
        qDebug().noquote() << processed.fragment.toUtf8();

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


ShaderSource
programFromFragmentShaderPath(const QString& fsFilename, QByteArray fsData)
{
  // ISF works by storing a vertex shader next to the fragment shader.
  QString vertexName = fsFilename;
  vertexName.replace(".frag", ".vert");
  vertexName.replace(".fs", ".vs");

  // If empty: will be using the ISF's default
  QByteArray vertexData;
  if (vertexName != fsFilename)
  {
    if (QFile vertexFile{vertexName};
        vertexFile.exists() && vertexFile.open(QIODevice::ReadOnly))
    {
      vertexData = vertexFile.readAll();
    }
  }

  if (fsData.isEmpty())
  {
    if (QFile fsFile{fsFilename};
        fsFile.exists() && fsFile.open(QIODevice::ReadOnly))
    {
      fsData = fsFile.readAll();
    }
  }

/*
  // Resolve includes ; for now we have one hardcoded library...
  QStringList shaderIncludePath;

  // FIXME refactor that !
  auto& lib_settings = score::AppContext().settings<Library::Settings::Model>();
  auto lib_path = lib_settings.getPath();
  if(QDir{}.exists(lib_path + "/Media/lygia/lygia-main"))
  {
    shaderIncludePath.append(lib_path + "/Media/lygia/lygia-main");
  }

  QFileInfo file{fsFilename};
  resolveGLSLIncludes(fsData, shaderIncludePath, file.absolutePath(), 0);
  resolveGLSLIncludes(vertexData, shaderIncludePath, file.absolutePath(), 0);
  */
  return {vertexData, fsData};
}
}
