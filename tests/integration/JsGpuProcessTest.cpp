// The GPU flavour of the Javascript process, at the model level.
//
// JS::ProcessModel::isGpu() (JSProcessModel.cpp:669) decides which executor the
// process gets purely by looking for a TextureInlet or a TextureOutlet among
// the children of the parsed script; JS::Executor::Component then builds either
// a js_node (CPUNode) or a gpu_exec_node (GPUNode) from that answer, and
// throws when the build has no GPU support. So the ports a script declares are
// what routes it, and editing the script re-routes it in place.
//
// What is asserted here is that classification and the ports it produces: a
// texture outlet must reach the model as a Gfx::TextureOutlet, a texture inlet
// as a Gfx::TextureInlet, and switching a live process between the two shapes
// must rebuild the ports rather than leave stale ones behind.
//
// NOT covered here: GPUNode.cpp itself -- the QQuickWindow render, the Util and
// Device objects it registers on its own engine (GPUNode.cpp:951-968), and
// reading a rendered texture back by value. That needs a live render list and
// a real RHI backend; see the report.
//
// One TEST_CASE, several processes in one document: JS::ProcessModel::rootPath()
// caches the Library settings in a function-local static, so only the first
// application of a process may create a Javascript process
// (JsRootPathStaticTest.cpp).

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <JS/JSProcessModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QPointF>

#include <set>

#include <catch2/catch_test_macros.hpp>

#if defined(SCORE_HAS_GPU_JS)
#include <Gfx/TexturePort.hpp>
#endif

namespace
{
JS::ProcessModel& addJsProcess(score::Document& doc, const score::ApplicationContext& app)
{
  auto& interval
      = static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate())
            .baseInterval();

  auto& factories = app.interfaces<Process::ProcessFactoryList>();
  const auto js_key = UuidKey<Process::ProcessModel>::fromString(
      QStringLiteral("846a5de5-47f9-46c5-a898-013cb20951d0"));
  auto* factory = factories.get(js_key);
  REQUIRE(factory != nullptr);

  std::set<const Process::ProcessModel*> before;
  for(auto& p : interval.processes)
    before.insert(&p);

  CommandDispatcher<> disp{doc.context().commandStack};
  disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
      interval, factory->concreteKey(), factory->customConstructionData(), QPointF{});

  JS::ProcessModel* js = nullptr;
  for(auto& p : interval.processes)
    if(!before.contains(&p))
      js = qobject_cast<JS::ProcessModel*>(&p);
  REQUIRE(js != nullptr);
  return *js;
}

const char* cpu_script = R"_(import Score
Script {
  ValueInlet { id: vin; objectName: "vin" }
  ValueOutlet { id: vout; objectName: "vout" }
  tick: function(token, state) { }
})_";

const char* texture_out_script = R"_(import Score
import QtQuick
Script {
  TextureOutlet { id: tout; objectName: "tex out" }
  tick: function(token, state) { }
})_";

const char* texture_in_script = R"_(import Score
import QtQuick
Script {
  TextureInlet { id: tin; objectName: "tex in" }
  ValueOutlet { id: vout; objectName: "vout" }
  tick: function(token, state) { }
})_";
}

TEST_CASE("A Javascript process is routed to the GPU by the ports it declares",
          "[integration][js][gui][gfx]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    // ---- a script with no texture port is a CPU process --------------------
    {
      auto& js = addJsProcess(*doc, ctx);
      REQUIRE(js.setProgram(JS::QmlSource{cpu_script, {}}).valid);
      CHECK_FALSE(js.isGpu());
      REQUIRE(js.inlets().size() == 1);
      REQUIRE(js.outlets().size() == 1);
      CHECK(js.outlets()[0]->name() == QStringLiteral("vout"));
    }

#if defined(SCORE_HAS_GPU_JS)
    // ---- a texture outlet makes it a GPU process ---------------------------
    {
      auto& js = addJsProcess(*doc, ctx);
      REQUIRE(js.setProgram(JS::QmlSource{texture_out_script, {}}).valid);
      CHECK(js.isGpu());
      CHECK(js.inlets().empty());
      REQUIRE(js.outlets().size() == 1);
      CHECK(qobject_cast<Gfx::TextureOutlet*>(js.outlets()[0]) != nullptr);
      CHECK(js.outlets()[0]->name() == QStringLiteral("tex out"));
    }

    // ---- a texture inlet does too, and the other ports still come through --
    {
      auto& js = addJsProcess(*doc, ctx);
      REQUIRE(js.setProgram(JS::QmlSource{texture_in_script, {}}).valid);
      CHECK(js.isGpu());
      REQUIRE(js.inlets().size() == 1);
      CHECK(qobject_cast<Gfx::TextureInlet*>(js.inlets()[0]) != nullptr);
      REQUIRE(js.outlets().size() == 1);
      CHECK(js.outlets()[0]->name() == QStringLiteral("vout"));
    }

    // ---- editing the script re-routes the process in place -----------------
    {
      auto& js = addJsProcess(*doc, ctx);
      REQUIRE(js.setProgram(JS::QmlSource{cpu_script, {}}).valid);
      REQUIRE_FALSE(js.isGpu());

      REQUIRE(js.setProgram(JS::QmlSource{texture_out_script, {}}).valid);
      CHECK(js.isGpu());
      CHECK(js.inlets().empty());
      REQUIRE(js.outlets().size() == 1);
      CHECK(qobject_cast<Gfx::TextureOutlet*>(js.outlets()[0]) != nullptr);

      REQUIRE(js.setProgram(JS::QmlSource{cpu_script, {}}).valid);
      CHECK_FALSE(js.isGpu());
      REQUIRE(js.inlets().size() == 1);
      REQUIRE(js.outlets().size() == 1);
      CHECK(qobject_cast<Gfx::TextureOutlet*>(js.outlets()[0]) == nullptr);
    }
#endif

    // ---- a script that does not parse leaves the process as it was ---------
    {
      auto& js = addJsProcess(*doc, ctx);
      REQUIRE(js.setProgram(JS::QmlSource{cpu_script, {}}).valid);
      const auto in = js.inlets().size();
      const auto out = js.outlets().size();

      CHECK_FALSE(js.setProgram(JS::QmlSource{"import Score\nScript { ((( }", {}}).valid);
      CHECK(js.inlets().size() == in);
      CHECK(js.outlets().size() == out);
      CHECK_FALSE(js.isGpu());
    }
  });
}
