// Cables between the nodes of nodal slots in the timeline: every cable ends on
// its ports, whatever the scroll position, the zoom and the height of the
// slots, and scrolling up or down never moves a node nor the cables going to
// it.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/CableItem.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateStateMacro.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QScrollBar>

#include <catch2/catch_test_macros.hpp>

#include <map>

namespace
{
const QString lfo_uuid = QStringLiteral("0b1b1816-c33e-4796-a16d-5aab27fe600f");

void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  do
  {
    QApplication::processEvents(QEventLoop::AllEvents, 5);
  } while(t.elapsed() < ms);
}

std::vector<Dataflow::CableItem*> shownCables(QGraphicsScene& scene)
{
  std::vector<Dataflow::CableItem*> res;
  for(auto item : scene.items())
    if(item->type() == Dataflow::CableItem::Type && item->isVisible())
    {
      auto c = static_cast<Dataflow::CableItem*>(item);
      if(c->source() && c->target())
        res.push_back(c);
    }
  return res;
}

//! Whether the port is inside each of its ancestors; the cable of one that is
//! not is drawn as a stub from its other end.
bool unclipped(const QGraphicsItem& port)
{
  for(auto parent = port.parentItem(); parent; parent = parent->parentItem())
  {
    const QRectF r = parent->boundingRect();
    if(r.width() == 0. && r.height() == 0.)
      continue;
    if(!r.contains(port.mapToItem(parent, QPointF{5., 5.})))
      return false;
  }
  return true;
}

//! Ends of the shown cables which are not where their port is.
QStringList misplacedEnds(QGraphicsScene& scene)
{
  QStringList res;
  for(auto* cable : shownCables(scene))
  {
    QGraphicsItem& item = *cable;
    for(auto* port : {cable->source(), cable->target()})
    {
      if(!port->isVisible() || !unclipped(*port))
        continue;
      const QPointF c = port->sceneCenter();
      if(!item.contains(item.mapFromScene(c)))
      {
        const QRectF r = item.mapRectToScene(item.boundingRect());
        res.push_back(QStringLiteral("port %1 at (%2, %3), cable over (%4, %5, %6 x %7)")
                          .arg(port->port().name())
                          .arg(c.x())
                          .arg(c.y())
                          .arg(r.x())
                          .arg(r.y())
                          .arg(r.width())
                          .arg(r.height()));
      }
    }
  }
  return res;
}

void checkEnds(QGraphicsScene& scene, const QString& step)
{
  spin(30);
  const auto bad = misplacedEnds(scene);
  INFO(step.toStdString() << ": " << bad.join("; ").toStdString());
  CHECK(bad.empty());
}

std::map<const Process::Port*, QPointF> portPositions(QGraphicsScene& scene)
{
  std::map<const Process::Port*, QPointF> res;
  for(auto* cable : shownCables(scene))
    for(auto* port : {cable->source(), cable->target()})
      res[&port->port()] = port->sceneCenter();
  return res;
}

Process::ProcessModel*
addLfo(score::Document& doc, const Scenario::IntervalModel& itv)
{
  auto& factories = doc.context().app.interfaces<Process::ProcessFactoryList>();
  auto* factory = factories.get(UuidKey<Process::ProcessModel>::fromString(lfo_uuid));
  if(!factory)
    return nullptr;
  auto* cmd = new Scenario::Command::AddOnlyProcessToInterval{
      itv, factory->concreteKey(), QString{}, QPointF{}};
  CommandDispatcher<>{doc.context().commandStack}.submit(cmd);
  auto it = itv.processes.find(cmd->processId());
  return it != itv.processes.end() ? &*it : nullptr;
}
}

TEST_CASE(
    "Timeline cables between nodal slots stay on their ports",
    "[integration][dataflow][cables][nodal][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& model
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate());
    auto& root = model.baseInterval();
    auto& scenario = static_cast<Scenario::ProcessModel&>(*root.processes.begin());
    // Room enough under the boxes to scroll each of them out of sight.
    root.setSlotHeight({0, Scenario::Slot::FullView}, 2400);

    const Scenario::IntervalModel* top{};
    const Scenario::IntervalModel* bottom{};
    {
      Scenario::Command::Macro m{
          new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      top = &m.createBox(scenario, TimeVal::fromMsecs(0), TimeVal::fromMsecs(20000), 0.02);
      bottom = &m.createBox(
          scenario, TimeVal::fromMsecs(2000), TimeVal::fromMsecs(22000), 0.45);
      m.showRack(*top);
      m.showRack(*bottom);
      m.commit();
    }
    auto* src = addLfo(*doc, *top);
    auto* snk = addLfo(*doc, *bottom);
    if(!src || !snk)
      SKIP("LFO not built");
    REQUIRE(!src->outlets().empty());
    REQUIRE(!snk->inlets().empty());
    {
      Scenario::Command::Macro m{
          new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      m.createCable(
          model, *src->outlets().front(), *snk->inlets().front(),
          Process::CableType::ImmediateGlutton);
      m.commit();
    }

    auto* presenter
        = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(
            *doc);
    REQUIRE(presenter);
    auto& gv = presenter->view().view();
    auto& scene = presenter->view().scene();
    gv.window()->resize(1200, 800);
    gv.window()->show();
    spin(300);

    REQUIRE(shownCables(scene).size() == 1);
    checkEnds(scene, "shown");

    auto* vbar = gv.verticalScrollBar();
    REQUIRE(vbar->maximum() - vbar->minimum() > 600);

    SECTION("scrolling up and down moves neither the nodes nor the cables")
    {
      vbar->setValue(vbar->minimum());
      spin(30);
      const auto ref = portPositions(scene);
      REQUIRE(ref.size() == 2);
      for(int v = vbar->minimum(); v <= vbar->maximum(); v += 37)
      {
        vbar->setValue(v);
        const QString step = QStringLiteral("scrolled to %1").arg(v);
        checkEnds(scene, step);
        for(auto& [port, pos] : portPositions(scene))
        {
          INFO(step.toStdString() << ", port " << port->name().toStdString());
          CHECK(pos == ref.at(port));
        }
      }
    }

    SECTION("scrolling sideways, zooming and resizing slots keep the cables on their ports")
    {
      auto* hbar = gv.horizontalScrollBar();
      for(int v = hbar->minimum(); v <= hbar->maximum(); v += 53)
      {
        hbar->setValue(v);
        checkEnds(scene, QStringLiteral("scrolled sideways to %1").arg(v));
      }
      hbar->setValue(hbar->minimum());

      for(double z : {-30., -30., 60., 40., -20.})
      {
        gv.verticalZoom(QPointF{0., z}, QPointF{});
        checkEnds(scene, QStringLiteral("vertical zoom %1").arg(z));
      }
      for(double z : {20., 20., -40., -30., 10.})
      {
        gv.horizontalZoom(
            QPointF{0., z}, gv.mapToScene(gv.viewport()->rect().center()));
        checkEnds(scene, QStringLiteral("horizontal zoom %1").arg(z));
      }

      for(auto* itv : {top, bottom})
      {
        auto& i = const_cast<Scenario::IntervalModel&>(*itv);
        for(double h : {60., 320., 140.})
        {
          i.setSlotHeight({0, Scenario::Slot::SmallView}, h);
          checkEnds(scene, QStringLiteral("slot height %1").arg(h));
        }
      }
    }
  });
}
