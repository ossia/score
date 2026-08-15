// Integration test: where a node lands when score picks its position.
//
// Double-clicking a library entry while a process is selected chains a new
// process after it. The position was computed from Process::ProcessModel::size(),
// which stays null until a Process::NodeItem writes the laid-out size back, so
// the new node landed 40px to the right of the previous one's top-left corner -
// on top of it. These tests cover both halves of the fix: the placement helpers
// never overlap an existing node, and a NodeItem publishes its size.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Dataflow/NodeItem.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessCreation.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>

#include <Library/Panel/LibraryPanelDelegate.hpp>
#include <Library/ProcessTreeView.hpp>
#include <Library/ProcessWidget.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QElapsedTimer>
#include <QGraphicsRectItem>
#include <QGraphicsScene>

#include <catch2/catch_test_macros.hpp>

namespace
{
Scenario::IntervalModel& baseInterval(score::Document& doc)
{
  return static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate())
      .baseInterval();
}

//! Node geometry is settled through short QTimers: let them fire.
void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  do
  {
    QApplication::processEvents(QEventLoop::AllEvents, 5);
  } while(t.elapsed() < ms);
}

QRectF nodeRect(const Process::ProcessModel& p)
{
  return QRectF{p.position(), Scenario::nodeFootprint(p)};
}

//! The library entry every score build has: an automation curve.
Process::ProcessData automationData()
{
  return Process::ProcessData{
      UuidKey<Process::ProcessModel>{"d2a67bd8-5d3f-404e-b6e9-e350cf2a833f"},
      QStringLiteral("Automation"), {}};
}

Process::ProcessModel* createProcess(
    score::Document& doc, Scenario::IntervalModel& itv, const Process::ProcessData& dat,
    QPointF pos)
{
  Scenario::Command::Macro m{
      new Scenario::Command::DropProcessInIntervalMacro, doc.context()};
  auto p = m.createProcessInNewSlot(itv, dat, pos);
  m.commit();
  return p;
}
}

TEST_CASE("Automatic node positions never overlap", "[integration][nodal][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& itv = baseInterval(*doc);

    // The document starts with the root scenario, already placed.
    REQUIRE(itv.processes.size() == 1);
    const auto& root = *itv.processes.begin();

    SECTION("the cascade skips the spots that are taken")
    {
      const auto pos = Scenario::newProcessPosition(itv);
      CHECK(!QRectF(pos, Scenario::nodeFootprint(root)).intersects(nodeRect(root)));
    }

    SECTION("a node chained after another one clears it entirely")
    {
      auto first = createProcess(*doc, itv, automationData(), QPointF{400., 400.});
      REQUIRE(first);
      // As if it had been laid out on screen
      first->setSize({420., 90.});

      const auto pos = Scenario::newProcessPositionAfter(itv, *first);
      CHECK(pos.x() >= first->position().x() + 420.);
      CHECK(pos.y() == first->position().y());
      CHECK(!QRectF(pos, Scenario::nodeFootprint(*first)).intersects(nodeRect(*first)));
    }

    SECTION("a node with no size yet is still not covered")
    {
      auto first = createProcess(*doc, itv, automationData(), QPointF{400., 400.});
      REQUIRE(first);
      first->setSize({});
      REQUIRE(first->size().isEmpty());

      const auto pos = Scenario::newProcessPositionAfter(itv, *first);
      CHECK(pos.x() > first->position().x());
      CHECK(!QRectF(pos, Scenario::nodeFootprint(*first)).intersects(nodeRect(*first)));
    }

    SECTION("chaining several nodes from the same one fans them out")
    {
      auto first = createProcess(*doc, itv, automationData(), QPointF{400., 400.});
      REQUIRE(first);
      first->setSize({200., 80.});

      std::vector<Process::ProcessModel*> chained;
      for(int i = 0; i < 4; i++)
      {
        auto p = createProcess(
            *doc, itv, automationData(), Scenario::newProcessPositionAfter(itv, *first));
        REQUIRE(p);
        p->setSize({200., 80.});
        chained.push_back(p);
      }

      // No two nodes on top of each other
      for(const Process::ProcessModel& a : itv.processes)
        for(const Process::ProcessModel& b : itv.processes)
          if(&a != &b)
          {
            INFO(
                a.metadata().getName().toStdString() << " at " << a.position().x() << ","
                << a.position().y() << " vs " << b.metadata().getName().toStdString()
                << " at " << b.position().x() << "," << b.position().y());
            CHECK(!nodeRect(a).intersects(nodeRect(b)));
          }
    }

    SECTION("a node chained before another one clears it on the left")
    {
      auto next = createProcess(*doc, itv, automationData(), QPointF{900., 400.});
      REQUIRE(next);
      next->setSize({150., 80.});

      auto created = createProcess(*doc, itv, automationData(), QPointF{2000., 2000.});
      REQUIRE(created);
      created->setSize({300., 80.});

      const auto pos = Scenario::newProcessPositionBefore(itv, *next, created);
      CHECK(pos.x() + 300. <= next->position().x());
      created->setPosition(pos);
      CHECK(!nodeRect(*created).intersects(nodeRect(*next)));
    }
  });
}

TEST_CASE("Adding a process after a selected one chains it", "[integration][nodal][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& itv = baseInterval(*doc);

    auto presenter
        = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(
            *doc);
    REQUIRE(presenter != nullptr);

    const auto dat = automationData();
    auto first = createProcess(*doc, itv, dat, QPointF{400., 400.});
    REQUIRE(first);
    REQUIRE(!first->outlets().empty());
    first->setSize({380., 120.});

    const auto before = itv.processes.size();
    Scenario::createProcessAfterPort(
        *presenter, dat, {}, {}, *first, *first->outlets().front());
    REQUIRE(itv.processes.size() == before + 1);

    const Process::ProcessModel* added{};
    for(const Process::ProcessModel& p : itv.processes)
      if(&p != first && p.concreteKey() == dat.key)
        added = &p;
    REQUIRE(added != nullptr);

    // The whole point: it is *after*, not on top of, the selected process.
    CHECK(added->position().x() >= first->position().x() + 380.);
    CHECK(!nodeRect(*added).intersects(nodeRect(*first)));
  });
}

TEST_CASE(
    "Double-clicking the library with a process selected chains after it",
    "[integration][nodal][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto panel = ctx.findPanel<Library::ProcessPanel>();
    if(!panel)
    {
      WARN("no library panel: score_plugin_library not loaded");
      return;
    }

    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& itv = baseInterval(*doc);

    auto presenter
        = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(
            *doc);
    REQUIRE(presenter != nullptr);
    presenter->setNodalMode(true);
    spin(50);

    Library::ProcessData dat;
    static_cast<Process::ProcessData&>(dat) = automationData();

    auto first = createProcess(*doc, itv, dat, QPointF{400., 400.});
    REQUIRE(first);
    spin(200);

    score::SelectionDispatcher{doc->context().selectionStack}.select(*first);
    spin(20);

    const auto before = itv.processes.size();
    // What a double-click in the library panel emits.
    panel->processWidget().processView().doubleClicked(dat);
    spin(50);

    REQUIRE(itv.processes.size() == before + 1);

    const Process::ProcessModel* added{};
    for(const Process::ProcessModel& p : itv.processes)
      if(&p != first && p.concreteKey() == dat.key)
        added = &p;
    REQUIRE(added != nullptr);

    CHECK(added->position().x() >= first->position().x() + first->size().width());
    CHECK(!nodeRect(*added).intersects(nodeRect(*first)));
  });
}

TEST_CASE(
    "A node wider than its stored size is not covered by the next one",
    "[integration][nodal][gui]")
{
  // A process starts out 200x100 in the model; a plug-in node with a bank of
  // controls draws much wider than that. Placing the next node from the stored
  // size dropped it right on top - what the library double-click used to do.
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    const UuidKey<Process::ProcessModel> faustKey{"5354c61a-1649-4f59-b952-5c2f1b79c1bd"};
    if(!ctx.interfaces<Process::ProcessFactoryList>().get(faustKey))
    {
      WARN("no Faust support in this build");
      return;
    }

    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& itv = baseInterval(*doc);

    auto presenter
        = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(
            *doc);
    REQUIRE(presenter != nullptr);
    presenter->setNodalMode(true);
    spin(50);

    QString sliders;
    for(int i = 0; i < 24; i++)
    {
      if(i > 0)
        sliders += " + ";
      sliders += QStringLiteral("hslider(\"p%1\", 0.5, 0, 1, 0.01)").arg(i);
    }
    const Process::ProcessData faust{
        faustKey, QStringLiteral("Faust"),
        QStringLiteral("process = _ * ((%1) / 24);").arg(sliders)};

    auto first = createProcess(*doc, itv, faust, QPointF{400., 400.});
    REQUIRE(first);
    spin(400);

    // The node grew past the 200x100 default while laying its controls out
    INFO("node size: " << first->size().width() << "x" << first->size().height());
    REQUIRE(first->size().width() > 200.);

    const auto pos = Scenario::newProcessPositionAfter(itv, *first);
    CHECK(pos.x() >= first->position().x() + first->size().width());
    CHECK(!QRectF(pos, Scenario::nodeFootprint(*first)).intersects(nodeRect(*first)));
  });
}

TEST_CASE("A node item publishes the size it draws", "[integration][nodal][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& itv = baseInterval(*doc);

    auto presenter
        = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(
            *doc);
    REQUIRE(presenter != nullptr);

    auto proc = createProcess(*doc, itv, automationData(), QPointF{400., 400.});
    REQUIRE(proc);
    proc->setSize({});
    REQUIRE(proc->size().isEmpty());

    // Layer presenters expect to be built inside a scene.
    QGraphicsScene scene;
    auto root = new QGraphicsRectItem;
    scene.addItem(root);

    auto item = new Process::NodeItem{
        *proc, presenter->context(), itv.duration.defaultDuration(), root};
    spin(100);

    // The placement helpers read this back.
    CHECK(proc->size().width() > 0.);
    CHECK(proc->size().height() > 0.);

    delete item;
    spin(20);
  });
}
