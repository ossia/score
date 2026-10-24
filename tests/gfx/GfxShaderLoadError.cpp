// A shader process whose shader does not parse says so in the message log,
// naming the process.
//
// Loading a document runs each shader process's setProgram / setScript. A
// shader that no longer parses left the process without its ports and
// rendering nothing; the parser's error went to errorMessage, which only an
// open script editor shows, or to an unnamed debug line (Filter, VSA, raw
// raster), or nowhere (CSF, geometry filter). Each now logs a warning naming
// the process and the parser's error, which the Message log panel shows. (The
// raw raster process, in score-plugin-threedim, reports through the same
// helper; its symbols are not linkable from here.)
//
// Registration:
//   score_add_gfx_process_test(shader_load_error GfxShaderLoadError.cpp)

#include "GfxProcessDoc.hpp"

#include <Gfx/CSF/Process.hpp>
#include <Gfx/Filter/Process.hpp>
#include <Gfx/GeometryFilter/Process.hpp>
#include <Gfx/ShaderProgram.hpp>
#include <Gfx/VSA/Process.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace score::test;
using namespace score::test::gfxproc;

namespace
{
QStringList g_warnings;
void captureWarnings(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
  if(type == QtWarningMsg || type == QtCriticalMsg)
    g_warnings.push_back(msg);
}

// A header that is not JSON: every shader family's parser refuses it.
const QString broken = QStringLiteral("/*{ \"ISFVSN\": \"2\", \"INPUTS\": [ { \"NAME\": } ] }*/\nvoid main() {}\n");

template <typename F>
QStringList warningsDuring(F&& f)
{
  g_warnings.clear();
  auto previous = qInstallMessageHandler(captureWarnings);
  f();
  qInstallMessageHandler(previous);
  return g_warnings;
}

bool namesProcess(const QStringList& warnings, const Process::ProcessModel& p)
{
  for(const auto& w : warnings)
    if(w.contains(p.metadata().getName()))
      return true;
  return false;
}
}

TEST_CASE("a shader process that cannot parse its shader logs a warning naming it", "[gfx][process]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    struct Case
    {
      const char* uuid;
      const char* what;
    };
    for(const Case c : {
            Case{"74ca45ff-92c9-44a0-8f1a-754dea05ee1b", "ISF"},
            Case{"ea13ed06-d21c-4c84-8d0f-83ce0027b81c", "VSA"},
            Case{"a5bbffe0-93d2-4e70-995c-cf46c2c43520", "CSF"},
            Case{"27d3cc85-a4b0-4924-8fde-71c337b40f59", "geometry filter"}})
    {
      CAPTURE(c.what);
      auto* proc = add_process(ctx, *doc, c.uuid);
      REQUIRE(proc != nullptr);
      const auto warnings = warningsDuring([&] {
        if(auto* m = dynamic_cast<Gfx::Filter::Model*>(proc))
          (void)m->setProgram(Gfx::ShaderSource{QString{}, broken});
        else if(auto* m = dynamic_cast<Gfx::VSA::Model*>(proc))
          (void)m->setVertex(broken);
        else if(auto* m = dynamic_cast<Gfx::CSF::Model*>(proc))
          (void)m->setScript(broken);
        else if(auto* m = dynamic_cast<Gfx::GeometryFilter::Model*>(proc))
          (void)m->setScript(broken);
        else
          FAIL("unexpected process type");
      });
      INFO(warnings.join("\n").toStdString());
      CHECK(namesProcess(warnings, *proc));
    }
  });
}
