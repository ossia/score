// A JSFX script may skip slider numbers (slider1, slider3, slider4): what comes
// in through the port of a slider's control, from a cable or an automation,
// sets that slider and no other, in a process just made as in one loaded,
// pasted or brought back by an undo.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Project.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Execution/ProcessComponent.hpp>

#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <Scenario/Commands/Interval/InsertContentInInterval.hpp>
#include <Scenario/Commands/Interval/RemoveProcessFromInterval.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/selection/SelectionStack.hpp>

#include <core/command/CommandStack.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <YSFX/ProcessModel.hpp>
#include <algorithm>
#include <map>
#include <catch2/catch_test_macros.hpp>

namespace
{
const QString ysfx_uuid = QStringLiteral("cb6b50d3-82c0-4356-a282-317da8b8022f");

constexpr auto script = R"(desc:slider gaps
slider1:0<0,100,1>First
slider3:0<0,100,1>Third
slider4:0<0,100,1>Fourth
out_pin:out
@sample
spl0 = 0;
)";

std::vector<YSFX::ProcessModel*> ysfx_processes(score::Document& doc)
{
  std::vector<YSFX::ProcessModel*> res;
  for(auto& p : score::test::base_interval(doc).processes)
    if(auto* y = qobject_cast<YSFX::ProcessModel*>(&p))
      res.push_back(y);
  return res;
}

//! Plays the document and writes a value through the port of each control:
//! slider n of the script is the control with id 3 + n.
void check_ports(score::Document& doc, YSFX::ProcessModel& proc)
{
  REQUIRE(proc.fx);
  auto& plug = doc.context().plugin<Execution::DocumentPlugin>();
  plug.reload(true, score::test::base_interval(doc));
  score::test::run_exec(plug);
  REQUIRE(plug.baseScenario());
  auto& procs = plug.baseScenario()->baseInterval().processes();
  auto it = procs.find(proc.id());
  REQUIRE(it != procs.end());
  REQUIRE(it->second->node);
  auto& node = *it->second->node;
  auto& state = *plug.contextData()->execState;

  const auto port_of = [&](int n) -> ossia::value_port* {
    auto* inlet = proc.inlet(Id<Process::Port>(3 + n));
    if(!inlet)
      return nullptr;
    auto& inlets = plug.contextData()->setupContext.inlets;
    auto found = inlets.find(inlet);
    REQUIRE(found != inlets.end());
    return found->second.second->target<ossia::value_port>();
  };
  const auto tick = [&] {
    const int frames = state.bufferSize;
    ossia::token_request tk;
    tk.prev_date = ossia::time_value{0};
    tk.date = ossia::time_value{frames * 1000};
    tk.start_sample = 0;
    tk.length_sample = frames;
    node.run(tk, ossia::exec_state_facade{&state});
    for(auto* in : node.root_inputs())
      if(auto* v = in->target<ossia::value_port>())
        v->get_data().clear();
  };
  const auto slider = [&](int n) { return ysfx_slider_get_value(proc.fx.get(), n - 1); };
  for(int n : {1, 2, 3, 4})
    ysfx_slider_set_value(proc.fx.get(), n - 1, 0, false);

  std::vector<int> written;
  for(int n : {3, 4, 1})
  {
    auto* port = port_of(n);
    if(!port)
      continue;
    port->write_value(float(10 * n), 0);
    tick();
    written.push_back(n);
    for(int other : {1, 2, 3, 4})
    {
      INFO("written on the port of slider " << n << ", slider " << other);
      const bool set = std::find(written.begin(), written.end(), other) != written.end();
      CHECK(slider(other) == (set ? 10. * other : 0.));
    }
  }

  plug.clear();
}

YSFX::ProcessModel* add_gaps(score::Document& doc, const QString& path)
{
  return static_cast<YSFX::ProcessModel*>(score::test::add_process(doc, ysfx_uuid, path));
}
}

TEST_CASE(
    "a JSFX slider's port drives that slider when the script skips numbers",
    "[integration][ysfx]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("gaps.jsfx");
    score::test::write_file(path, script);

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = add_gaps(*doc, path);
    if(!proc || !proc->fx)
      SKIP("YSFX not built");

    SECTION("just made") { check_ports(*doc, *proc); }

    SECTION("loaded")
    {
      for(auto* loaded : {score::test::reload_via_json(ctx, *doc),
                          score::test::reload_via_bytes(ctx, *doc)})
      {
        REQUIRE(loaded);
        auto procs = ysfx_processes(*loaded);
        REQUIRE(procs.size() == 1);
        check_ports(*loaded, *procs[0]);
      }
    }

    SECTION("removed then brought back by an undo, and a redo of the undo")
    {
      auto& itv = score::test::base_interval(*doc);
      CommandDispatcher<>{doc->context().commandStack}.submit(
          new Scenario::Command::RemoveProcessFromInterval{itv, proc->id()});
      REQUIRE(ysfx_processes(*doc).empty());
      doc->commandStack().undo();
      auto procs = ysfx_processes(*doc);
      REQUIRE(procs.size() == 1);
      check_ports(*doc, *procs[0]);

      doc->commandStack().redo();
      doc->commandStack().undo();
      procs = ysfx_processes(*doc);
      REQUIRE(procs.size() == 1);
      check_ports(*doc, *procs[0]);
    }

    SECTION("copied and pasted")
    {
      auto& sctx = doc->context();
      sctx.selectionStack.pushNewSelection(Selection{proc});
      JSONReader r;
      REQUIRE(Scenario::copySelectedProcesses(r, sctx));
      auto json = readJson(r.toByteArray());
      CommandDispatcher<>{sctx.commandStack}.submit(
          new Scenario::Command::PasteProcessesInInterval{
              json, score::test::base_interval(*doc), ExpandMode::GrowShrink, QPointF{}});
      auto procs = ysfx_processes(*doc);
      REQUIRE(procs.size() == 2);
      for(auto* p : procs)
        check_ports(*doc, *p);
    }
  });
}

TEST_CASE(
    "a JSFX control saved without some of the script's sliders drives the slider it "
    "names",
    "[integration][ysfx]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("gaps.jsfx");
    score::test::write_file(path, script);

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = add_gaps(*doc, path);
    if(!proc || !proc->fx)
      SKIP("YSFX not built");

    // A document whose process lost the control of slider3, as one whose
    // port could not be read: the controls left are those of slider1 and
    // slider4.
    auto json = QJsonDocument::fromJson(score::test::save_as_json(*doc)).object();
    bool removed = false;
    const auto strip = [&](auto&& self, QJsonValue v) -> QJsonValue {
      if(v.isArray())
      {
        QJsonArray a = v.toArray();
        for(int i = 0; i < a.size(); i++)
          a[i] = self(self, a[i]);
        return a;
      }
      if(!v.isObject())
        return v;
      QJsonObject o = v.toObject();
      if(o.contains("Script") && o.value("Inlets").isArray())
      {
        QJsonArray inlets;
        for(const auto& inlet : o.value("Inlets").toArray())
        {
          if(inlet.toObject().value("id").toInt() == 3 + 3)
            removed = true;
          else
            inlets.push_back(inlet);
        }
        o["Inlets"] = inlets;
        return o;
      }
      for(auto it = o.begin(); it != o.end(); ++it)
        it.value() = self(self, it.value());
      return o;
    };
    json = strip(strip, json).toObject();
    REQUIRE(removed);

    auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
    auto* loaded = ctx.docManager.loadDocument(
        ctx, QStringLiteral("without slider3"), QJsonDocument{json}.toJson(),
        JSONObject::type(), *delegates.begin());
    REQUIRE(loaded);
    auto procs = ysfx_processes(*loaded);
    REQUIRE(procs.size() == 1);
    REQUIRE(procs[0]->inlet(Id<Process::Port>(3 + 3)) == nullptr);
    check_ports(*loaded, *procs[0]);
  });
}

TEST_CASE(
    "a JSFX slider past the 64th that @block changes is shown on its control",
    "[integration][ysfx]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("many.jsfx");
    score::test::write_file(path, R"(desc:many sliders
slider1:0<0,100,1>First
slider70:0<0,100,1>Seventieth
slider200:0<0,100,1>Two hundredth
out_pin:out
@block
slider1 = 1; sliderchange(slider1);
slider70 = 70; sliderchange(slider70);
slider200 = 200; slider_automate(slider200);
@sample
spl0 = 0;
)");

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = add_gaps(*doc, path);
    if(!proc || !proc->fx)
      SKIP("YSFX not built");

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*doc));
    score::test::run_exec(plug);
    REQUIRE(plug.baseScenario());
    auto& procs = plug.baseScenario()->baseInterval().processes();
    auto it = procs.find(proc->id());
    REQUIRE(it != procs.end());
    REQUIRE(it->second->node);
    auto& state = *plug.contextData()->execState;

    std::map<int, double> shown;
    for(int n : {1, 70, 200})
    {
      auto* c = qobject_cast<Process::ControlInlet*>(proc->inlet(Id<Process::Port>(3 + n)));
      REQUIRE(c);
      QObject::connect(
          c, &Process::ControlInlet::executionValueChanged, c,
          [&shown, n](const ossia::value& v) { shown[n] = ossia::convert<double>(v); });
    }

    ossia::token_request tk;
    tk.prev_date = ossia::time_value{0};
    tk.date = ossia::time_value{state.bufferSize * 1000};
    tk.start_sample = 0;
    tk.length_sample = state.bufferSize;
    it->second->node->run(tk, ossia::exec_state_facade{&state});
    QMetaObject::invokeMethod(&doc->context().coarseUpdateTimer, "timeout");

    CHECK(shown[1] == 1.);
    CHECK(shown[70] == 70.);
    CHECK(shown[200] == 200.);
    plug.clear();
  });
}
