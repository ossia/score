// A .scp file dropped on the timeline, in the scenario or on a box, is read
// like the library reads it: its <PRESET>: paths name the files next to it.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Preset.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>
#include <Scenario/Process/ScenarioView.hpp>

#include <score/tools/FilePath.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentPresenter.hpp>

#include <QGraphicsScene>
#include <QGraphicsSceneDragDropEvent>
#include <QGuiApplication>
#include <QMimeData>
#include <QTemporaryDir>
#include <QUrl>

#include <catch2/catch_test_macros.hpp>

namespace
{
// Looper (audio), whose "Loop file" is a sound file port.
const auto looper = UuidKey<Process::ProcessModel>::fromString(
    QStringLiteral("a0ad4227-ac3d-448b-a19b-19581ed4e2c6"));
// Scenario::DropPresetInInterval
const auto interval_preset_drop = UuidKey<Scenario::IntervalDropHandler>::fromString(
    QStringLiteral("93d1dd9d-5923-4bc2-8c52-cbe0677a3202"));

struct Fixture
{
  score::Document* doc{};
  Scenario::IntervalModel* root{};
  Scenario::ProcessModel* scenario{};
  QTemporaryDir tmp;
  QString kit;
  QString file;

  explicit Fixture(const score::GUIApplicationContext& ctx)
  {
    doc = score::test::new_document(ctx);
    REQUIRE(doc);
    root = &score::test::base_interval(*doc);
    scenario = &static_cast<Scenario::ProcessModel&>(*root->processes.begin());

    REQUIRE(tmp.isValid());
    kit = score::test::canonical(tmp.path()) + "/kit";
    score::test::write_wav(kit + "/kick.wav", 0.05);
    Process::Preset p;
    p.name = "Kick";
    p.key.key = looper;
    p.data = QByteArray(R"([[20,{"String":"<PRESET>:kick.wav"},"Loop file"]])");
    file = kit + "/Kick.scp";
    score::test::write_file(file, p.toJson());
  }
};

std::vector<const Process::ProcessModel*> loopers(const Scenario::IntervalModel& itv)
{
  std::vector<const Process::ProcessModel*> res;
  for(auto& p : itv.processes)
    if(p.concreteKey() == looper)
      res.push_back(&p);
  return res;
}

QString loopFile(const Process::ProcessModel& proc, const score::DocumentContext& ctx)
{
  for(auto* in : proc.inlets())
    if(auto* ctrl = qobject_cast<Process::ControlInlet*>(in); ctrl && ctrl->name() == "Loop file")
      return score::locateFilePath(score::test::control_string(*ctrl), ctx);
  return {};
}

Scenario::ScenarioPresenter& rootScenarioPresenter(
    const score::GUIApplicationContext& ctx, score::Document& doc,
    const Scenario::ProcessModel& scenario)
{
  QApplication::processEvents();
  static_cast<Scenario::ScenarioDocumentPresenter&>(
      *doc.presenter()->presenterDelegate())
      .focusFrontProcess();
  auto* pres = ctx.guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>()
                   .focusedPresenter();
  REQUIRE(pres);
  REQUIRE(&pres->model() == &scenario);
  return *pres;
}
}

TEST_CASE(
    "A preset file dropped in the scenario finds the files next to it",
    "[integration][scenario][drop][preset][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    if(!ctx.interfaces<Process::ProcessFactoryList>().get(looper))
      SKIP("Fx not built");
    Fixture f{ctx};
    auto& pres = rootScenarioPresenter(ctx, *f.doc, *f.scenario);

    std::vector<const Scenario::IntervalModel*> before;
    for(auto& itv : f.scenario->intervals)
      before.push_back(&itv);

    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(f.file)});
    auto& view = pres.view();
    QGraphicsSceneDragDropEvent ev{QEvent::GraphicsSceneDrop};
    ev.setPos({300., 60.});
    ev.setMimeData(&mime);
    ev.setModifiers(QGuiApplication::keyboardModifiers());
    view.scene()->sendEvent(&view, &ev);
    QApplication::processEvents();

    const Process::ProcessModel* dropped{};
    for(auto& itv : f.scenario->intervals)
      if(std::find(before.begin(), before.end(), &itv) == before.end())
        for(auto* p : loopers(itv))
          dropped = p;
    REQUIRE(dropped);
    CHECK(
        loopFile(*dropped, f.doc->context()).toStdString()
        == QString{f.kit + "/kick.wav"}.toStdString());
  });
}

TEST_CASE(
    "A preset file dropped on a box finds the files next to it",
    "[integration][scenario][drop][preset][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    if(!ctx.interfaces<Process::ProcessFactoryList>().get(looper))
      SKIP("Fx not built");
    Fixture f{ctx};

    Scenario::IntervalDropHandler* handler{};
    for(auto& h : ctx.interfaces<Scenario::IntervalDropHandlerList>())
      if(h.concreteKey() == interval_preset_drop)
        handler = &h;
    REQUIRE(handler);

    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(f.file)});
    REQUIRE(handler->drop(f.doc->context(), *f.root, QPointF{}, mime));
    const auto dropped = loopers(*f.root);
    REQUIRE(dropped.size() == 1);
    CHECK(
        loopFile(*dropped.front(), f.doc->context()).toStdString()
        == QString{f.kit + "/kick.wav"}.toStdString());
  });
}
