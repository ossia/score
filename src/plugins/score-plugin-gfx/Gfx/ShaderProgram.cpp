#include "ShaderProgram.hpp"

#include <Gfx/Graph/ShaderCache.hpp>
#include <Gfx/Settings/Model.hpp>
#include <Library/LibrarySettings.hpp>

#include <score/application/ApplicationContext.hpp>

#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/hash_map.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace Gfx
{

namespace
{

QStringList shaderIncludePaths()
{
  QStringList shaderIncludePath;

  // Default path: the library packages dir so users' own GLSL snippets
  // drop in without ceremony. Additional search roots are expected to be
  // supplied via a user-facing include-paths GUI (not yet wired up) —
  // no static registration mechanism lives here anymore.
  auto& lib_settings = score::AppContext().settings<Library::Settings::Model>();
  const QString lib_path = lib_settings.getPackagesPath();
  if(QDir{}.exists(lib_path))
  {
    shaderIncludePath.append(lib_path);

    // Also register every first-level subdirectory of `packages/` so
    // shader libraries shipping as standalone packages (openpbr/,
    // lygia/, MaterialX/, …) can be `#include`d by their bare header
    // name from any user shader without the consumer having to know
    // the install layout. Internal cross-includes inside a library
    // keep working via the origin-dir-first lookup in
    // tryResolveQuoted.
    //
    // Collision policy: if two libraries ship the same header
    // basename, the one earlier in QDir iteration order wins. In
    // practice shader libs prefix their headers (`openpbr_*.h`) so
    // collisions are vanishingly unlikely.
    QDir packagesDir{lib_path};
    const auto subdirs = packagesDir.entryList(
        QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for(const auto& sub : subdirs)
      shaderIncludePath.append(packagesDir.filePath(sub));
  }

  return shaderIncludePath;
}

void updateToGlsl45(ShaderSource& program)
{
  static const QRegularExpression out_expr{
      R"_(^out\s+(\w+)\s+(\w+)\s*(\[([0-9]+)\])?\s*;)_",
      QRegularExpression::MultilineOption};
  static const QRegularExpression in_expr{
      R"_(^in\s+(\w+)\s+(\w+)\s*(\[([0-9]+)\])?\s*;)_",
      QRegularExpression::MultilineOption};

  ossia::flat_map<QString, int> attributes_locations_map;

  // First fixup the vertex shader and look for all the attributes
  {
    // location 0 is taken by fragColor - we start at 1.
    int cur_location = 1;

    auto match_idx = program.vertex.indexOf(out_expr);
    while(match_idx != -1)
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
    while(match_idx != -1)
    {
      const QString partialString = program.fragment.mid(match_idx);
      const auto& match = in_expr.match(partialString);
      const int len = match.capturedLength(0);

      const int loc = attributes_locations_map[match.captured(2)];

      program.fragment.insert(match_idx, QString("layout(location = %1) ").arg(loc));

      match_idx = program.fragment.indexOf(in_expr, match_idx + len);
    }
  }

  // Remove lowp, highp, etc
  program.vertex.remove("precision highp float;");
  program.vertex.remove("precision mediump float;");
  program.vertex.remove("precision lowp float;");
  program.vertex.remove("lowp ");
  program.vertex.remove("mediump ");
  program.vertex.remove("highp ");
  program.fragment.remove("precision highp float;");
  program.fragment.remove("precision mediump float;");
  program.fragment.remove("precision lowp float;");
  program.fragment.remove("lowp ");
  program.fragment.remove("mediump ");
  program.fragment.remove("highp ");
}

// Resolver state shared across recursive include expansion.
//
// `searchPaths` holds roots applied to both quoted and bracketed
// includes. `originDir` is the directory the current buffer was loaded
// from; it becomes the first place quoted includes are looked up and is
// pushed/popped as we descend into included files so relative headers
// resolve against their own sibling dir, not the top-level shader's.
// `visited` holds canonicalised paths already expanded in the current
// chain — revisiting one is a cycle.
struct IncludeContext
{
  QStringList searchPaths;
  QString originDir;
  ossia::hash_set<QString> visited;
  int depth = 0;
  int maxDepth = 16;
  QString error;             // first fatal error encountered
  QStringList missing;       // unresolved headers, for diagnostics
};

static void removeIncludesInComments(QByteArray& data);
static QByteArray resolveIncludes(QByteArray data, IncludeContext& ctx);

static std::optional<QString> tryResolveQuoted(
    const QString& header, const IncludeContext& ctx)
{
  // Quoted: origin dir first, then search paths.
  if(!ctx.originDir.isEmpty())
  {
    const QString candidate = ctx.originDir + QLatin1Char('/') + header;
    if(QFileInfo::exists(candidate))
      return QFileInfo{candidate}.canonicalFilePath();
  }
  for(const auto& path : ctx.searchPaths)
  {
    const QString candidate = path + QLatin1Char('/') + header;
    if(QFileInfo::exists(candidate))
      return QFileInfo{candidate}.canonicalFilePath();
  }
  return std::nullopt;
}

static std::optional<QString> tryResolveBracketed(
    const QString& header, const IncludeContext& ctx)
{
  // Bracketed: search paths only (no origin-dir lookup).
  for(const auto& path : ctx.searchPaths)
  {
    const QString candidate = path + QLatin1Char('/') + header;
    if(QFileInfo::exists(candidate))
      return QFileInfo{candidate}.canonicalFilePath();
  }
  return std::nullopt;
}

// Expand one resolved include file into `ctx`-tracked source, emitting
// `#line` markers so glslang error messages point at the included file.
// On cycle / depth / unreadable-file failure, sets ctx.error and returns
// an empty byte array (caller must abort).
static QByteArray expandFile(
    const QString& canonicalPath, IncludeContext& ctx, int parentLine,
    const QString& parentPath)
{
  if(ctx.depth >= ctx.maxDepth)
  {
    ctx.error = QStringLiteral("Shader include depth limit (%1) exceeded at '%2'")
                    .arg(ctx.maxDepth)
                    .arg(canonicalPath);
    return {};
  }
  if(ctx.visited.contains(canonicalPath))
  {
    ctx.error
        = QStringLiteral("Shader include cycle detected: '%1' re-entered")
              .arg(canonicalPath);
    return {};
  }

  QFile f{canonicalPath};
  if(!f.open(QIODevice::ReadOnly))
  {
    ctx.error
        = QStringLiteral("Shader include: failed to read '%1'").arg(canonicalPath);
    return {};
  }
  QByteArray body = f.readAll();

  // Recurse with a pushed origin dir so relative includes in this file
  // resolve against its own sibling dir. Save/restore on return.
  const QString savedOriginDir = ctx.originDir;
  ctx.originDir = QFileInfo{canonicalPath}.absolutePath();
  ctx.visited.insert(canonicalPath);
  ctx.depth++;

  QByteArray expanded = resolveIncludes(std::move(body), ctx);

  ctx.depth--;
  ctx.visited.erase(canonicalPath);
  ctx.originDir = savedOriginDir;

  if(!ctx.error.isEmpty())
    return {};

  // Frame with #line markers: enter the included file at line 1, return
  // to the parent at the line just after the #include directive. We pass
  // filenames through as string tokens — glslang accepts that form.
  QByteArray framed;
  framed.reserve(expanded.size() + 256);
  framed.append("#line 1 \"");
  framed.append(canonicalPath.toUtf8());
  framed.append("\"\n");
  framed.append(expanded);
  if(!framed.endsWith('\n'))
    framed.append('\n');
  framed.append("#line ");
  framed.append(QByteArray::number(parentLine + 1));
  framed.append(" \"");
  framed.append(parentPath.toUtf8());
  framed.append("\"\n");
  return framed;
}

// Single-pass textual expansion. Walks from top to bottom, replacing
// each `#include` line with the (already-expanded) body of the target.
// Comments are neutralised before the scan so `#include` inside // or /*
// doesn't trigger.
static QByteArray resolveIncludes(QByteArray data, IncludeContext& ctx)
{
  removeIncludesInComments(data);

  // Anchor to start-of-line (optional leading whitespace only) so an
  // `#include "..."` substring inside an #error string or a string-
  // literal payload doesn't get misidentified as a directive. The
  // openpbr headers exercise this: `#error "... Add #include
  // <glm/glm.hpp> ..."` would otherwise trip a "<glm/glm.hpp> not found"
  // hard error even though no actual GLSL include is needed.
  static const QRegularExpression quoted{
      R"_(^\s*#include\s*"([^"]+)")_",
      QRegularExpression::MultilineOption};
  static const QRegularExpression bracket{
      R"_(^\s*#include\s*<([^>]+)>)_",
      QRegularExpression::MultilineOption};

  QByteArray out;
  out.reserve(data.size());

  // Lightweight "current file" tag for the parent-line #line marker;
  // when the outer buffer came from disk, originDir points to the file's
  // dir but we don't have the filename itself — fall back to "<shader>"
  // for in-memory / unknown roots.
  const QString parentPath
      = ctx.originDir.isEmpty() ? QStringLiteral("<shader>") : ctx.originDir;

  int cursor = 0;
  int line = 1;
  while(cursor < data.size())
  {
    const int eol = data.indexOf('\n', cursor);
    const int lineEnd = eol == -1 ? data.size() : eol;
    const QByteArray lineBytes = data.mid(cursor, lineEnd - cursor);

    // Only scan lines that look like include directives at all.
    const int hashIdx = lineBytes.indexOf('#');
    if(hashIdx != -1 && lineBytes.indexOf("include", hashIdx) != -1)
    {
      const QString lineStr = QString::fromUtf8(lineBytes);
      if(auto m = quoted.match(lineStr); m.hasMatch())
      {
        const QString header = m.captured(1);
        if(auto resolved = tryResolveQuoted(header, ctx))
        {
          QByteArray body = expandFile(*resolved, ctx, line, parentPath);
          if(!ctx.error.isEmpty())
            return {};
          out.append(body);
          cursor = lineEnd + (eol == -1 ? 0 : 1);
          line++;
          continue;
        }
        ctx.missing.push_back(header);
        ctx.error = QStringLiteral(
                        "Shader include not found: \"%1\" (searched: %2)")
                        .arg(header)
                        .arg(ctx.originDir.isEmpty()
                                 ? ctx.searchPaths.join(", ")
                                 : (ctx.originDir + QStringLiteral(", ")
                                    + ctx.searchPaths.join(", ")));
        return {};
      }
      if(auto m = bracket.match(lineStr); m.hasMatch())
      {
        const QString header = m.captured(1);
        if(auto resolved = tryResolveBracketed(header, ctx))
        {
          QByteArray body = expandFile(*resolved, ctx, line, parentPath);
          if(!ctx.error.isEmpty())
            return {};
          out.append(body);
          cursor = lineEnd + (eol == -1 ? 0 : 1);
          line++;
          continue;
        }
        // Bracketed include not found: NON-fatal. Emit the line verbatim
        // and let the downstream preprocessor (glslang/QShaderBaker)
        // handle gating. This is what makes openpbr work without an
        // `#if`-aware resolver: openpbr_interop.h pulls in
        // `openpbr_interop_cpp.h` (gated by `#if defined(__cplusplus)`),
        // which itself includes `<cstdint>` / `<cassert>`. We don't
        // honour the `#if`, so we textually inline the C++ branch's
        // contents — but glslang DOES honour the `#if`, sees that
        // `__cplusplus` is undefined for shader compilation, and skips
        // the entire C++ branch (including the orphan `<cstdint>`
        // line) at preprocess time. Tracking in `missing` keeps the
        // diagnostic visible if the user wants to debug.
        ctx.missing.push_back(header);
        // fall through to the verbatim-line append below
      }
    }

    out.append(lineBytes);
    if(eol != -1)
      out.append('\n');
    cursor = lineEnd + (eol == -1 ? 0 : 1);
    line++;
  }

  return out;
}

static void removeIncludesInComments(QByteArray& data)
{
  // very basic implementation as there does not seem to be any easily integratable one
  if(data.size() < 2)
    return;
  bool in_long_comment = false;
  bool in_line_comment = false;
  bool in_string = false;
  auto pos = data.begin();
  while(pos < data.end() - 2)
  {
    if(in_long_comment)
    {
      if(*pos == '#')
        *pos = ' ';

      if(*pos == '*' && *(pos + 1) == '/')
      {
        // *pos = MARKER;
        pos++;
        // *pos = MARKER;
        in_long_comment = false;
      }
      else
      {
        // *pos = MARKER;
      }
    }
    else if(in_line_comment)
    {
      if(*pos == '#')
        *pos = ' ';

      if(*pos == '\n')
      {
        in_line_comment = false;
      }
      else
      {
        // *pos = MARKER;
      }
    }
    else if(in_string)
    {
      // could happen in string though but well
      if(*pos == '"')
      {
        int num_backslashes_before = 0;
        auto p = pos - 1;
        while(p >= data.begin() && *p == '\\')
          num_backslashes_before++;

        if(num_backslashes_before % 2 == 0)
          in_string = false;
      }
    }
    else
    {
      if(*pos == '/')
      {
        if(*(pos + 1) == '*')
        {
          in_long_comment = true;
          // *pos = MARKER;
          pos++;
          // *pos = MARKER;
        }
        else if(*(pos + 1) == '/')
        {
          in_line_comment = true;
          // *pos = MARKER;
          pos++;
          // *pos = MARKER;
        }
      }
      else if(*pos == '"')
        in_string = true;
    }

    pos++;
  }
}

}

ProgramCache& ProgramCache::instance() noexcept
{
  static ProgramCache cache;
  return cache;
}

std::pair<std::optional<ProcessedProgram>, QString>
ProgramCache::get(const ShaderSource& program, const QString& originPath) noexcept
{
  // Derive the origin dir once — it's both the cache-key disambiguator
  // (two shaders with identical text but different origin dirs resolve
  // different sibling includes and must not collide) and the first
  // search root for quoted #include resolution.
  const QString originDir
      = originPath.isEmpty() ? QString{} : QFileInfo{originPath}.absolutePath();
  const ProgramCacheKey cacheKey{program, originDir};

  auto it = programs.find(cacheKey);
  if(it != programs.end())
    return {it->second, QString{}};

  try
  {
    // Resolve includes. Empty originDir → in-memory source, falls back
    // to the search paths only.
    IncludeContext ctx;
    ctx.searchPaths = shaderIncludePaths();
    ctx.originDir = originDir;

    QByteArray source_frag = resolveIncludes(program.fragment.toUtf8(), ctx);
    if(!ctx.error.isEmpty())
      return {std::nullopt, QStringLiteral("Fragment: ") + ctx.error};

    // Reset per-file state (visited chain, depth, errors); keep search
    // paths and origin dir across the two shader stages.
    ctx.visited.clear();
    ctx.depth = 0;
    ctx.error.clear();
    ctx.missing.clear();

    QByteArray source_vert = resolveIncludes(program.vertex.toUtf8(), ctx);
    if(!ctx.error.isEmpty())
      return {std::nullopt, QStringLiteral("Vertex: ") + ctx.error};

    switch(program.type)
    {
      default:
      case ProcessedProgram::ProgramType::ISF:
      case ProcessedProgram::ProgramType::
          VertexShaderArt: // FIXME it gets parsed as ISF?
      {
        // Parse ISF and get GLSL shaders
        isf::parser parser{
            source_vert.toStdString(), source_frag.toStdString(), 450, program.type};

        auto isfVert = QByteArray::fromStdString(parser.vertex());
        auto isfFrag = QByteArray::fromStdString(parser.fragment());

        if(qEnvironmentVariableIsSet("SCORE_DUMP_SHADERS"))
        {
          qDebug().noquote() << "\n\n ======= VERTEX ======== \n\n" << isfVert;
          qDebug().noquote() << "\n\n ======= FRAGMENT ======== \n\n" << isfFrag;
        }

        if(isfVert.isEmpty())
        {
          return {std::nullopt, "Not a valid ISF vertex shader"};
        }

        if(isfFrag.isEmpty())
        {
          return {std::nullopt, "Not a valid ISF fragment shader"};
        }

        if(isfVert != source_vert || isfFrag != source_frag
           || program.type == ProcessedProgram::ProgramType::RawRasterPipeline)
        {
          ProcessedProgram processed{
              ShaderSource{program.type, isfVert, isfFrag}, parser.data()};

          // Add layout, location, etc
          updateToGlsl45(processed);

          auto& settings = score::AppContext().settings<Gfx::Settings::Model>();
          const auto api = settings.graphicsApiEnum();

          // Create QShader objects
          auto [vertexS, vertexError] = score::gfx::ShaderCache::get(
              api, Gfx::Settings::shaderVersionForAPI(api), processed.vertex.toUtf8(),
              QShader::VertexStage);
          if(!vertexError.isEmpty())
          {
            qDebug().noquote() << vertexError;
            qDebug().noquote() << processed.vertex.toUtf8();
            return {std::nullopt, "Vertex shader error: " + vertexError};
          }

          auto [fragmentS, fragmentError] = score::gfx::ShaderCache::get(
              api, Gfx::Settings::shaderVersionForAPI(api), processed.fragment.toUtf8(),
              QShader::FragmentStage);
          if(!fragmentError.isEmpty())
          {
            qDebug().noquote() << fragmentError;
            qDebug().noquote() << processed.fragment.toUtf8();

            return {std::nullopt, "Fragment shader error: " + fragmentError};
          }

          if(vertexS.isValid() && fragmentS.isValid())
          {
            programs[cacheKey] = processed;
            return {processed, {}};
          }
        }
        else
        {
          return {std::nullopt, "Not a valid ISF shader"};
        }
        break;
      }
    }
  }
  catch(const std::runtime_error& error)
  {
    return {std::nullopt, QString("ISF error: %1").arg(error.what())};
  }
  catch(...)
  {
    return {std::nullopt, "Unknown error"};
  }

  return {std::nullopt, "Unknown error"};
}

ShaderSource
programFromISFFragmentShaderPath(const QString& fsFilename, QByteArray fsData)
{
  // ISF works by storing a vertex shader next to the fragment shader.
  // Score recognises both the long (.frag/.vert) and short (.fs/.vs)
  // extension conventions; pairings are tried independently of the FS
  // file's own naming so a `foo.frag` next to `foo.vs` (or `foo.fs` next
  // to `foo.vert`) also resolves. Without this, the .vs sibling is
  // silently ignored and the descriptor falls back to the ISF default
  // vertex shader — which doesn't know about user-declared
  // VERTEX_INPUTS, so the consumer renders nothing.
  const QString candidates[] = {
      QString(fsFilename).replace(".frag", ".vert").replace(".fs", ".vs"),
      QString(fsFilename).replace(".frag", ".vs"),
      QString(fsFilename).replace(".fs", ".vert"),
  };

  // If empty: will be using the ISF's default
  QByteArray vertexData;
  for(const QString& vertexName : candidates)
  {
    if(vertexName == fsFilename)
      continue;
    if(QFile vertexFile{vertexName};
       vertexFile.exists() && vertexFile.open(QIODevice::ReadOnly))
    {
      vertexData = vertexFile.readAll();
      break;
    }
  }

  if(fsData.isEmpty())
  {
    if(QFile fsFile{fsFilename}; fsFile.exists() && fsFile.open(QIODevice::ReadOnly))
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
  return {ShaderSource::ProgramType::ISF, vertexData, fsData};
}
ShaderSource
programFromVSAVertexShaderPath(const QString& vertexFilename, QByteArray vertexData)
{
  if(vertexData.isEmpty())
  {
    if(QFile file{vertexFilename}; file.exists() && file.open(QIODevice::ReadOnly))
    {
      vertexData = file.readAll();
    }
  }

  return {ShaderSource::ProgramType::VertexShaderArt, vertexData, ""};
}

std::pair<QByteArray, QString>
preprocessShaderIncludes(QByteArray source, const QString& originPath) noexcept
{
  IncludeContext ctx;
  ctx.searchPaths = shaderIncludePaths();
  if(!originPath.isEmpty())
    ctx.originDir = QFileInfo{originPath}.absolutePath();

  QByteArray expanded = resolveIncludes(std::move(source), ctx);
  if(!ctx.error.isEmpty())
    return {{}, ctx.error};
  return {std::move(expanded), {}};
}
}
