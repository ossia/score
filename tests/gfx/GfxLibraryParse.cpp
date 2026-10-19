// Every shader of the user library parses, the way score loads it: includes
// resolved, then libisf for the file's family (MODE). Reports each one that
// does not. Skips without SCORE_TEST_LIBRARY_ROOT.
//
// Both the shader files and the shaders embedded in the library's .score
// documents, where users keep most of theirs: a saved document carries the
// text of each shader process, and loads it through the same parser.
//
// Packages are often symlinks (a checkout linked into the library): the walk
// follows them, and a walk that finds nothing fails rather than passing.
#include <Gfx/ShaderProgram.hpp>

#include <isf.hpp>
#include <score_test/App.hpp>

#include "GfxUserLibrary.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <map>
#include <stdexcept>
#include <string>

namespace
{
using T = isf::parser::ShaderType;

QByteArray resolve(QByteArray text, const QString& origin)
{
  auto [src, err] = Gfx::preprocessShaderIncludes(std::move(text), origin);
  if(!err.isEmpty())
    throw std::runtime_error{"include: " + err.toStdString()};
  return src;
}

QByteArray load(const QString& path)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
    return {};
  return resolve(f.readAll(), path);
}

// A file in the Unknown family is an ISF when it has a JSON header comment;
// anything else (a GLSL include, a snippet) is not a shader of its own.
bool hasIsfHeader(const QByteArray& text)
{
  const auto open = text.indexOf("/*");
  if(open < 0)
    return false;
  for(qsizetype i = open + 2; i < text.size(); i++)
  {
    const char c = text[i];
    if(c == '{')
      return true;
    if(c != ' ' && c != '\t' && c != '\n' && c != '\r')
      return false;
  }
  return false;
}

struct Result
{
  int parsed{};
  std::map<std::string, std::string> failures;
};

void report(const Result& r, const char* what)
{
  if(const auto out = qEnvironmentVariable("LIBRARY_PARSE_OUT"); !out.isEmpty())
  {
    QFile f{out};
    if(f.open(QIODevice::Append))
      for(const auto& [path, err] : r.failures)
        f.write(QByteArray::fromStdString(path + "\t" + err).replace('\n', ' ') + "\n");
  }
  for(const auto& [path, err] : r.failures)
    UNSCOPED_INFO(path << "\n    " << err);
  INFO(what << ": parsed " << r.parsed << ", failed " << r.failures.size());
  CHECK(r.parsed > 0);
  CHECK(r.failures.empty());
}

QStringList libraryFiles(const QString& root, const QStringList& patterns)
{
  QStringList files;
  QDirIterator it{
      root + "/packages", patterns, QDir::Files,
      QDirIterator::Subdirectories | QDirIterator::FollowSymlinks};
  while(it.hasNext())
    files.push_back(it.next());
  files.sort();
  return files;
}

// The shader processes a document can hold, by their process UUID, and the
// JSON keys holding their text (see each process's JSONReader::read).
void parseEmbedded(
    const QJsonValue& v, const QString& doc, const QString& where, Result& r)
{
  if(v.isArray())
  {
    const auto a = v.toArray();
    for(qsizetype i = 0; i < a.size(); i++)
      parseEmbedded(a[i], doc, where, r);
    return;
  }
  if(!v.isObject())
    return;
  const auto o = v.toObject();
  const QString uuid = o.value("uuid").toString();
  const auto text = [&](const char* key) {
    return resolve(o.value(key).toString().toUtf8(), doc).toStdString();
  };
  const std::string name
      = where.toStdString() + " "
        + o.value("Metadata").toObject().value("ScriptingName").toString().toStdString();
  auto attempt = [&](auto&& parse) {
    try
    {
      parse();
      r.parsed++;
    }
    catch(const std::exception& e)
    {
      r.failures[name + " (" + uuid.toStdString() + ")"] = e.what();
    }
  };
  if(uuid == "74ca45ff-92c9-44a0-8f1a-754dea05ee1b" && o.contains("Fragment")) // ISF
    attempt([&] { isf::parser{text("Vertex"), text("Fragment"), 450, T::ISF}; });
  else if(uuid == "dbfc2101-40d7-4807-8804-571e88992e7e" && o.contains("Fragment")) // raw raster
    attempt([&] {
      isf::parser{text("Vertex"), text("Fragment"), 450, T::RawRasterPipeline};
    });
  else if(uuid == "ea13ed06-d21c-4c84-8d0f-83ce0027b81c" && o.contains("Vertex")) // VSA
    attempt([&] { isf::parser{text("Vertex"), {}, 450, T::VertexShaderArt}; });
  else if(uuid == "a5bbffe0-93d2-4e70-995c-cf46c2c43520" && o.contains("Compute")) // CSF
    attempt([&] { isf::parser{{}, text("Compute"), 450, T::CSF}; });
  else if(uuid == "27d3cc85-a4b0-4924-8fde-71c337b40f59" && o.contains("Script")) // geometry filter
    attempt([&] { isf::parser{text("Script"), {}, 450, T::GeometryFilter}; });

  for(auto it = o.begin(); it != o.end(); ++it)
    if(it->isObject() || it->isArray())
      parseEmbedded(*it, doc, where, r);
}
}

TEST_CASE("every library shader parses", "[gfx][isf][library]")
{
  const QString root = score::test::gfx::library::root();
  if(root.isEmpty())
    SKIP("SCORE_TEST_LIBRARY_ROOT is not set");

  Result r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    score::test::gfx::library::RootGuard guard{ctx};
    for(const QString& path : libraryFiles(
            root, {"*.fs", "*.frag", "*.vs", "*.vert", "*.cs", "*.csf", "*.glsl"}))
    {
      const auto family = Gfx::shaderFileFamily(path);
      const QString suffix = QFileInfo{path}.suffix();
      const bool vertexFile = suffix == "vs" || suffix == "vert";
      if(qEnvironmentVariableIsSet("LIBRARY_PARSE_TRACE"))
        fprintf(stderr, "PARSE %s\n", qPrintable(path)), fflush(stderr);
      try
      {
        switch(family)
        {
          case Gfx::ShaderFamily::Compute:
            isf::parser{{}, load(path).toStdString(), 450, T::CSF};
            break;
          case Gfx::ShaderFamily::VertexShaderArt:
            isf::parser{load(path).toStdString(), {}, 450, T::VertexShaderArt};
            break;
          case Gfx::ShaderFamily::RawRaster: {
            if(vertexFile)
              continue;
            const QString vs = Gfx::vertexShaderSibling(path);
            isf::parser{
                vs.isEmpty() ? std::string{} : load(vs).toStdString(),
                load(path).toStdString(), 450, T::RawRasterPipeline};
            break;
          }
          case Gfx::ShaderFamily::GeometryFilter:
            isf::parser{load(path).toStdString(), {}, 450, T::GeometryFilter};
            break;
          case Gfx::ShaderFamily::Unknown: {
            if(vertexFile || suffix == "glsl")
              continue; // a vertex half, or a GLSL include
            const QByteArray fs = load(path);
            if(!hasIsfHeader(fs))
              continue; // a GLSL snippet, not an ISF
            const QString vs = Gfx::vertexShaderSibling(path);
            isf::parser{
                vs.isEmpty() ? std::string{} : load(vs).toStdString(), fs.toStdString(),
                450, T::ISF};
            break;
          }
        }
        r.parsed++;
      }
      catch(const std::exception& e)
      {
        r.failures[path.toStdString()] = e.what();
      }
    }
  });
  report(r, "shader files");
}

TEST_CASE("every shader embedded in a library document parses", "[gfx][isf][library]")
{
  const QString root = score::test::gfx::library::root();
  if(root.isEmpty())
    SKIP("SCORE_TEST_LIBRARY_ROOT is not set");

  Result r;
  int documents = 0;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    score::test::gfx::library::RootGuard guard{ctx};
    for(const QString& path : libraryFiles(root, {"*.score"}))
    {
      QFile f{path};
      if(!f.open(QIODevice::ReadOnly))
        continue;
      const auto doc = QJsonDocument::fromJson(f.readAll());
      if(doc.isNull())
        continue; // not a JSON document (a binary save)
      documents++;
      parseEmbedded(doc.object(), path, path, r);
    }
  });
  INFO(documents << " documents");
  report(r, "embedded shaders");
}
