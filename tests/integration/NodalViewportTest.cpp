// Integration test: the nodal canvas comes back exactly as it was saved — same
// scale, same point of the canvas under the center of the view — whether the
// document is reloaded from its JSON (.score) or binary form, and whatever
// happened to the nodes in between.
//
// The interval used to persist a pan offset relative to the nodes' bounding
// box (which moves whenever a node does, and which zooming never updated) and
// wrote the scale as the literal 1.0: reloading a zoomed-out document showed
// the empty middle of the graph at 1:1, with no node in sight.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/FullView/NodalIntervalView.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QRegularExpression>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

using Catch::Approx;

namespace
{
void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  do
  {
    QApplication::processEvents(QEventLoop::AllEvents, 5);
  } while(t.elapsed() < ms);
}

Scenario::IntervalModel& baseInterval(score::Document& doc)
{
  return static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate())
      .baseInterval();
}

Scenario::ScenarioDocumentPresenter& presenterOf(score::Document& doc)
{
  auto p = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(
      doc);
  REQUIRE(p != nullptr);
  return *p;
}

Scenario::NodalIntervalView* nodalView(Scenario::ScenarioDocumentPresenter& p)
{
  for(auto item : p.view().scene().items())
    if(auto n = dynamic_cast<Scenario::NodalIntervalView*>(item))
      return n;
  return nullptr;
}

//! The library entry every score build has: an automation curve.
Process::ProcessData automationData()
{
  return Process::ProcessData{
      UuidKey<Process::ProcessModel>{"d2a67bd8-5d3f-404e-b6e9-e350cf2a833f"},
      QStringLiteral("Automation"), {}};
}

Process::ProcessModel*
createProcess(score::Document& doc, Scenario::IntervalModel& itv, QPointF pos)
{
  Scenario::Command::Macro m{
      new Scenario::Command::DropProcessInIntervalMacro, doc.context()};
  auto p = m.createProcessInNewSlot(itv, automationData(), pos);
  m.commit();
  return p;
}

//! What the user sees: the canvas scale, and the canvas point at the center
//! of the visible part of the nodal view (what NodalIntervalView keeps put).
struct Viewport
{
  double scale{};
  QPointF centerOnCanvas{};
};

Viewport viewportOf(Scenario::ScenarioDocumentPresenter& p)
{
  auto nodal = nodalView(p);
  REQUIRE(nodal != nullptr);
  auto& gv = p.view().view();
  auto& container = nodal->nodeContainer();

  const QPointF tl = nodal->mapFromScene(gv.mapToScene(0, 0));
  const QPointF br = nodal->mapFromScene(gv.mapToScene(gv.width(), gv.height()));
  const QRectF visible = QRectF{tl, br}.intersected(nodal->boundingRect());
  return {container.scale(), container.mapFromParent(visible.center())};
}

void checkSameViewport(const Viewport& a, const Viewport& b)
{
  CHECK(b.scale == Approx(a.scale).epsilon(1e-6));
  CHECK(b.centerOnCanvas.x() == Approx(a.centerOnCanvas.x()).margin(1.));
  CHECK(b.centerOnCanvas.y() == Approx(a.centerOnCanvas.y()).margin(1.));
}

QByteArray saveAsJson(score::Document& doc)
{
  JSONReader w;
  doc.saveAsJson(w);
  return w.toByteArray();
}

score::Document* loadJson(const score::GUIApplicationContext& ctx, QByteArray bytes)
{
  auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
  auto doc = ctx.docManager.loadDocument(
      ctx, QStringLiteral("nodal-viewport.score"), std::move(bytes),
      JSONObject::type(), *delegates.begin());
  spin(100);
  return doc;
}

//! A document shown on the nodal canvas, with nodes too far apart to be seen
//! whole at 1:1, zoomed out and panned so that they are.
struct ZoomedOutDocument
{
  score::Document* doc{};
  Scenario::ScenarioDocumentPresenter* presenter{};
  Scenario::NodalIntervalView* nodal{};
  Process::ProcessModel* first{};
  Viewport viewport{};
};

ZoomedOutDocument makeZoomedOutDocument(const score::GUIApplicationContext& ctx)
{
  ZoomedOutDocument d;
  d.doc = score::test::new_document(ctx);
  REQUIRE(d.doc != nullptr);
  d.presenter = &presenterOf(*d.doc);
  d.presenter->setNodalMode(true);
  spin(50);

  auto& itv = baseInterval(*d.doc);
  d.first = createProcess(*d.doc, itv, QPointF{0., 0.});
  createProcess(*d.doc, itv, QPointF{1600., 0.});
  createProcess(*d.doc, itv, QPointF{0., 1200.});
  spin(200);

  d.nodal = nodalView(*d.presenter);
  REQUIRE(d.nodal != nullptr);
  d.nodal->zoomTo(-4.); // 1.2^-4
  d.nodal->panBy(QPointF{-300., 80.});
  spin(20);
  d.viewport = viewportOf(*d.presenter);
  return d;
}
}

TEST_CASE(
    "Zooming and panning the nodal canvas is what the interval stores",
    "[integration][nodal][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto d = makeZoomedOutDocument(ctx);
    auto& itv = baseInterval(*d.doc);

    CHECK(d.viewport.scale == Approx(std::pow(1.2, -4.)));
    CHECK(itv.nodalScale() == Approx(d.viewport.scale));
    REQUIRE(itv.nodalCenter().has_value());
    CHECK(itv.nodalCenter()->x() == Approx(d.viewport.centerOnCanvas.x()).margin(1.));
    CHECK(itv.nodalCenter()->y() == Approx(d.viewport.centerOnCanvas.y()).margin(1.));

    // Moving a node afterwards moves neither the view nor what is stored:
    // the viewport is pinned to the canvas, not to the nodes' bounding box.
    const QPointF center = *itv.nodalCenter();
    const QPointF pos = d.nodal->nodeContainer().pos();
    d.first->setPosition(d.first->position() + QPointF{3000., -2500.});
    spin(50);
    d.nodal->recenterRelativeToView();
    CHECK(*itv.nodalCenter() == center);
    CHECK(d.nodal->nodeContainer().pos() == pos);
    checkSameViewport(d.viewport, viewportOf(*d.presenter));

    // And so does laying the view out again
    auto& gv = d.presenter->view().view();
    gv.resize(gv.width() - 150, gv.height() - 90);
    spin(20);
    d.nodal->recenterRelativeToView();
    checkSameViewport(d.viewport, viewportOf(*d.presenter));
  });
}

TEST_CASE("The nodal viewport survives a binary reload", "[integration][nodal][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto d = makeZoomedOutDocument(ctx);

    auto reloaded = score::test::reload_via_bytes(ctx, *d.doc);
    REQUIRE(reloaded != nullptr);
    spin(100);
    auto& p = presenterOf(*reloaded);
    auto& itv = baseInterval(*reloaded);
    CHECK(itv.viewMode() == Scenario::IntervalModel::ViewMode::Nodal);
    CHECK(itv.nodalScale() == Approx(d.viewport.scale));
    REQUIRE(itv.nodalCenter().has_value());
    checkSameViewport(d.viewport, viewportOf(p));
  });
}

TEST_CASE("The nodal viewport survives a .score reload", "[integration][nodal][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto d = makeZoomedOutDocument(ctx);

    const QByteArray bytes = saveAsJson(*d.doc);
    CHECK(bytes.contains("\"NodalCenter\":"));
    const auto scaleMatch
        = QRegularExpression{R"("NodalScale":(-?[0-9.eE+-]+))"}.match(bytes);
    REQUIRE(scaleMatch.hasMatch());
    CHECK(scaleMatch.captured(1).toDouble() == Approx(d.viewport.scale));

    auto reloaded = loadJson(ctx, bytes);
    REQUIRE(reloaded != nullptr);
    auto& p = presenterOf(*reloaded);
    auto& itv = baseInterval(*reloaded);
    CHECK(itv.viewMode() == Scenario::IntervalModel::ViewMode::Nodal);
    CHECK(itv.nodalScale() == Approx(d.viewport.scale));
    REQUIRE(itv.nodalCenter().has_value());
    checkSameViewport(d.viewport, viewportOf(p));
  });
}

namespace
{
//! The document as saved before the nodal center existed: only NodalOffset
//! (a pan relative to the centered nodes), if any, and NodalScale.
QByteArray withoutNodalCenter(score::Document& doc)
{
  QString json = QString::fromUtf8(saveAsJson(doc));
  REQUIRE(json.contains("\"NodalCenter\":"));
  json.replace(QRegularExpression{R"("NodalCenter":\[[^\]]*\],)"}, QString{});
  REQUIRE(!json.contains("\"NodalCenter\":"));
  return json.toUtf8();
}

void checkCenteredOnNodes(
    Scenario::ScenarioDocumentPresenter& p, Scenario::IntervalModel& itv)
{
  auto nodal = nodalView(p);
  REQUIRE(nodal != nullptr);
  REQUIRE(itv.nodalCenter().has_value());
  const QPointF nodesCenter = nodal->enclosingRect().center();
  CHECK(itv.nodalCenter()->x() == Approx(nodesCenter.x()).margin(1.));
  CHECK(itv.nodalCenter()->y() == Approx(nodesCenter.y()).margin(1.));
  const auto vp = viewportOf(p);
  CHECK(vp.centerOnCanvas.x() == Approx(nodesCenter.x()).margin(1.));
  CHECK(vp.centerOnCanvas.y() == Approx(nodesCenter.y()).margin(1.));
}
}

TEST_CASE(
    "Documents saved before the nodal center existed open centered on their nodes",
    "[integration][nodal][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto d = makeZoomedOutDocument(ctx);
    const QByteArray legacy = withoutNodalCenter(*d.doc);

    auto reloaded = loadJson(ctx, legacy);
    REQUIRE(reloaded != nullptr);
    auto& p = presenterOf(*reloaded);
    auto& itv = baseInterval(*reloaded);

    // The saved zoom is kept, the view picked the nodes' center and pinned it
    CHECK(itv.nodalScale() == Approx(d.viewport.scale));
    checkCenteredOnNodes(p, itv);
  });
}

TEST_CASE(
    "Documents which never saved their zoom open with all their nodes in sight",
    "[integration][nodal][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    // Until now the scale was always written as 1.0: a document zoomed out to
    // show a large graph came back at 1:1, centered on the empty middle of it.
    auto d = makeZoomedOutDocument(ctx);
    QByteArray legacy = withoutNodalCenter(*d.doc);
    legacy.replace(
        QRegularExpression{R"("NodalScale":[^,]*,)"}.match(legacy).captured(0).toUtf8(),
        QByteArray{});
    REQUIRE(!legacy.contains("\"NodalScale\":"));

    auto reloaded = loadJson(ctx, legacy);
    REQUIRE(reloaded != nullptr);
    auto& p = presenterOf(*reloaded);
    auto& itv = baseInterval(*reloaded);
    auto nodal = nodalView(p);
    REQUIRE(nodal != nullptr);

    // Fitted in the view, not 1:1
    const QRectF item = nodal->boundingRect();
    const QRectF nodes = nodal->enclosingRect();
    const double fit = std::clamp(
        std::min(item.width() / nodes.width(), item.height() / nodes.height()), 0.01,
        1.0);
    REQUIRE(fit < 1.0);
    CHECK(itv.nodalScale() == Approx(fit));
    CHECK(nodal->nodeContainer().scale() == Approx(fit));
    checkCenteredOnNodes(p, itv);

    // Every node is on screen
    auto& gv = p.view().view();
    const QRectF onScreen = gv.mapToScene(gv.viewport()->rect()).boundingRect();
    const QRectF shownNodes = nodal->nodeContainer().mapRectToScene(nodes);
    CHECK(onScreen.adjusted(-4., -4., 4., 4.).contains(shownNodes));
  });
}
