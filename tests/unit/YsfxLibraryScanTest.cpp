// The JSFX library takes the effects of a folder with or without the .jsfx
// extension, as REAPER's Effects folder holds them: a file without one (or
// whose name only has dots of its own, "Auto-Pan v1.1 (Receiver)") is an
// effect when it is text with a desc: line near its top. Includes, presets,
// data, binaries and files under hidden folders are not. Such an effect loads, under
// its file name, and imports an include without an extension.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Library/LibraryInterface.hpp>

#include <score/tools/RecursiveWatch.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <YSFX/ProcessModel.hpp>
#include <catch2/catch_test_macros.hpp>

#include <map>

namespace
{
const QString ysfx_uuid = QStringLiteral("cb6b50d3-82c0-4356-a282-317da8b8022f");

void write(const QTemporaryDir& dir, const QString& name, const QByteArray& text)
{
  const QString path = dir.filePath(name);
  QDir{}.mkpath(QFileInfo{path}.absolutePath());
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(text);
}

Library::LibraryInterface& jsfx_library(const score::GUIApplicationContext& ctx)
{
  for(auto& lib : ctx.interfaces<Library::LibraryInterfaceList>())
    if(lib.acceptedFiles().contains("jsfx"))
      return lib;
  FAIL("no JSFX library handler");
  throw;
}

//! What the library lists under root: file name -> entry name
std::map<QString, QString> scan(Library::LibraryInterface& lib, const QString& root)
{
  std::map<QString, QString> found;
  score::RecursiveWatch w;
  w.setWatchedFolder(root.toStdString());
  for(const QString& ext : lib.acceptedFiles())
  {
    score::RecursiveWatch::Callbacks cbs;
    cbs.added = [&](std::string_view path) {
      if(auto e = lib.scanPath(path))
        found[QDir{root}.relativeFilePath(e->node.data.customData)]
            = e->node.data.prettyName;
    };
    w.registerWatch(ext.toStdString(), std::move(cbs));
  }
  w.scan();
  return found;
}

constexpr auto include_text = R"(@init
included = 42;
)";
}

TEST_CASE("JSFX: the library takes effects without the .jsfx extension", "[ysfx]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    // A header before desc:, as many of REAPER's effects have
    write(
        dir, "Effects/utility/gain",
        QByteArray("// Copyright\n").repeated(40)
            + "desc: Gain without extension\nimport ../lib/helpers\nslider1:0<0,100,1>Value\n"
              "@init\nslider1 = included;\n");
    write(dir, "Effects/Till/Auto-Pan v1.1 (Receiver)", "desc: Dotted name\n");
    write(dir, "Effects/classic.jsfx", "desc: With extension\n");
    write(dir, "Effects/classic.jsfx-inc", "desc: An include\n");
    write(dir, "Effects/classic.jsfx.rpl", "<REAPER_PRESET_LIBRARY\n");
    write(dir, "Effects/lib/helpers", include_text);
    write(dir, "Effects/README", "Some notes, no description line.\n");
    write(dir, "Effects/binary", QByteArray("\x7f" "ELF\0\0\0desc: no\n", 16));
    write(dir, "Effects/.git/objects/ab", "desc: in a hidden folder\n");
    write(dir, "Data/amp/model", "0.1 0.2 0.3\n");
    write(dir, "presets/factory", "<REAPER_PRESET_LIBRARY `JS: gain`\n");

    auto& lib = jsfx_library(ctx);
    const auto found = scan(lib, dir.path());
    for(auto& [file, name] : found)
      UNSCOPED_INFO(file.toStdString() << " -> " << name.toStdString());

    CHECK(found.count("Effects/utility/gain") == 1);
    CHECK(found.count("Effects/Till/Auto-Pan v1.1 (Receiver)") == 1);
    CHECK(found.count("Effects/classic.jsfx") == 1);
    CHECK(found.count("Effects/classic.jsfx-inc") == 0);
    CHECK(found.count("Effects/classic.jsfx.rpl") == 0);
    CHECK(found.count("Effects/lib/helpers") == 0);
    CHECK(found.count("Effects/README") == 0);
    CHECK(found.count("Effects/binary") == 0);
    CHECK(found.count("Effects/.git/objects/ab") == 0);
    CHECK(found.count("Data/amp/model") == 0);
    CHECK(found.count("presets/factory") == 0);
    if(found.count("Effects/utility/gain"))
      CHECK(found.at("Effects/utility/gain") == "gain");
    if(found.count("Effects/Till/Auto-Pan v1.1 (Receiver)"))
      CHECK(found.at("Effects/Till/Auto-Pan v1.1 (Receiver)") == "Auto-Pan v1.1 (Receiver)");
    if(found.count("Effects/classic.jsfx"))
      CHECK(found.at("Effects/classic.jsfx") == "classic");

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = static_cast<YSFX::ProcessModel*>(
        score::test::add_process(*doc, ysfx_uuid, dir.filePath("Effects/utility/gain")));
    REQUIRE(proc);
    REQUIRE(proc->fx);
    CHECK(QString{ysfx_get_name(proc->fx.get())} == "Gain without extension");
    // The include's @init ran before the effect's
    CHECK(ysfx_slider_get_value(proc->fx.get(), 0) == 42.);
  });
}
