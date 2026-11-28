#include "GfxProcessDoc.hpp"

#include <Gfx/Filter/Process.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <QTemporaryDir>

#include <algorithm>

using namespace score::test;
using namespace score::test::gfxproc;

namespace
{
constexpr auto UUID_FILTER = "74ca45ff-92c9-44a0-8f1a-754dea05ee1b";
constexpr auto HEADER = R"_({ "ISFVSN": "2.0", "INPUTS": [ { "NAME": "level", "TYPE": "float", "DEFAULT": 0.5 } ] })_";
constexpr auto BODY = "void main() { gl_FragColor = vec4(level); }\n";

bool hasInlet(const Process::ProcessModel& p, const char* name)
{
  const auto names = inlet_names(p);
  return std::find(names.begin(), names.end(), QString::fromLatin1(name)) != names.end();
}
}

TEST_CASE("an ISF process created from its source compiles that source", "[gfx][process][isf]")
{
  const QString source = QString::fromStdString(GENERATE(
      std::string("/*") + HEADER + "*/\n" + BODY,
      std::string("/*\n") + HEADER + "\n*/\n" + BODY,
      std::string("// a licence line\n/*") + HEADER + "*/\n" + BODY,
      std::string("/* ") + HEADER + " */ " + BODY));
  CAPTURE(source.toStdString());

  run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);
    auto* proc = dynamic_cast<Gfx::Filter::Model*>(add_process(ctx, *doc, UUID_FILTER, source));
    REQUIRE(proc != nullptr);
    CHECK_FALSE(proc->program().fragment.isEmpty());
    CHECK(hasInlet(*proc, "level"));
    CHECK(proc->rootPath().isEmpty());
  });
}

TEST_CASE("an ISF process created from a path loads that file", "[gfx][process][isf]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString path = dir.path() + "/inline.fs";
  {
    QFile f{path};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(QByteArray("/*") + HEADER + "*/\n" + BODY);
  }

  run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);
    auto* proc = dynamic_cast<Gfx::Filter::Model*>(add_process(ctx, *doc, UUID_FILTER, path));
    REQUIRE(proc != nullptr);
    CHECK(proc->rootPath() == path);
    CHECK(hasInlet(*proc, "level"));
  });
}
