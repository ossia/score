#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Events.hpp>
#include <score_test/Mouse.hpp>
#include <score_test/Project.hpp>

#include <Curve/ApplicationPlugin.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Palette/CurveEditionSettings.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>

#include <Dataflow/CurveInlet.hpp>
#include <Process/Commands/SetControlValue.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/selection/SelectionStack.hpp>

#include <core/document/Document.hpp>

#include <QGraphicsScene>
#include <QGraphicsView>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>

namespace
{
const QString midiEnvelope = QStringLiteral("e58cf441-0106-4862-8abf-c3cee072f861");

ossia::value segment(float x0, float y0, float x1, float y1, float gamma = 1.f)
{
  return std::vector<ossia::value>{ossia::vec2f{x0, y0}, ossia::vec2f{x1, y1}, gamma};
}

//! A peak in the middle: the point to drag is at (0.5, 0.6).
ossia::value peak()
{
  return std::vector<ossia::value>{
      segment(0.f, .2f, .5f, .6f), segment(.5f, .6f, 1.f, .2f)};
}

std::vector<std::pair<double, double>> points(const Curve::Model& model)
{
  std::vector<std::pair<double, double>> res;
  for(auto* seg : model.sortedSegments())
    res.emplace_back(seg->start().x(), seg->start().y());
  if(!model.sortedSegments().empty())
  {
    auto* last = model.sortedSegments().back();
    res.emplace_back(last->end().x(), last->end().y());
  }
  return res;
}

Dataflow::CurveInlet* curveInlet(Process::ProcessModel& process)
{
  for(auto* inlet : process.inlets())
    if(auto* curve = dynamic_cast<Dataflow::CurveInlet*>(inlet))
      return curve;
  return nullptr;
}

//! Drags the middle point of peak() from (0.5, 0.6) to (0.7, 0.9) in a
//! curve editor of `size` whose top-left corner is at `origin` in the scene.
void dragMiddlePoint(QGraphicsView& view, QPointF origin, QSizeF size)
{
  const auto at = [&](double x, double y) {
    return view.mapFromScene(
        origin + QPointF{x * size.width(), (1. - y) * size.height()});
  };
  auto& vp = *view.viewport();
  score::test::mouseEvent(
      vp, QEvent::MouseButtonPress, at(.5, .6), Qt::LeftButton, Qt::LeftButton, {});
  score::test::settle();
  score::test::mouseEvent(
      vp, QEvent::MouseMove, at(.6, .75), Qt::NoButton, Qt::LeftButton, {});
  score::test::settle();
  score::test::mouseEvent(
      vp, QEvent::MouseMove, at(.7, .9), Qt::NoButton, Qt::LeftButton, {});
  score::test::settle();
  score::test::mouseEvent(
      vp, QEvent::MouseButtonRelease, at(.7, .9), Qt::LeftButton, Qt::NoButton, {});
  score::test::settle();
}

void checkDragged(const Dataflow::CurveInlet& inlet)
{
  const auto pts = points(*inlet.m_curve);
  REQUIRE(pts.size() == 3);
  CHECK(pts[1].first == Catch::Approx(.7).margin(.02));
  CHECK(pts[1].second == Catch::Approx(.9).margin(.02));

  // The port value, which the execution reads, follows the model.
  const auto* segs = inlet.value().target<std::vector<ossia::value>>();
  REQUIRE(segs);
  REQUIRE(segs->size() == 2);
  const auto& second = (*segs)[1].target<std::vector<ossia::value>>()->at(0);
  const auto start = *second.target<ossia::vec2f>();
  CHECK(start[0] == Catch::Approx(.7).margin(.02));
  CHECK(start[1] == Catch::Approx(.9).margin(.02));
}
}

TEST_CASE(
    "A curve port item edits its curve with a drag, in a fresh application",
    "[curve][dataflow][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* process = score::test::add_process(*doc, midiEnvelope, {});
    if(!process)
      SKIP("Midi Envelope is not built in this configuration");
    auto* inlet = curveInlet(*process);
    REQUIRE(inlet);

    // The state of a fresh application: no automation has picked a tool.
    auto& settings
        = ctx.guiApplicationPlugin<Curve::ApplicationPlugin>().editionSettings();
    settings.setTool(Curve::Tool::Disabled);

    CommandDispatcher<>{doc->context().commandStack}.submit(
        new Process::SetControlValue{*inlet, peak()});
    const auto before = inlet->value();
    const auto commands = doc->commandStack().currentIndex();

    QGraphicsScene scene;
    QGraphicsView view{&scene};
    std::unique_ptr<QGraphicsItem> item{WidgetFactory::CurveInletItems::make_item(
        *inlet, *inlet, doc->context(), nullptr, inlet)};
    scene.addItem(item.get());
    const QSizeF size{300., 200.};
    Dataflow::setCurveItemRect(*item, {QPointF{}, size});
    view.setSceneRect(-20., -20., 340., 240.);
    view.resize(380, 280);
    REQUIRE(score::test::showAndActivate(view));
    score::test::settle();

    dragMiddlePoint(view, item->scenePos(), size);
    checkDragged(*inlet);
    CHECK(doc->commandStack().currentIndex() == commands + 1);

    doc->commandStack().undo();
    CHECK(inlet->value() == before);
    CHECK(points(*inlet->m_curve)[1].first == Catch::Approx(.5));
  });
}

TEST_CASE(
    "The inspector curve editor edits the port without selecting in the document",
    "[curve][dataflow][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* process = score::test::add_process(*doc, midiEnvelope, {});
    if(!process)
      SKIP("Midi Envelope is not built in this configuration");
    auto* inlet = curveInlet(*process);
    REQUIRE(inlet);
    ctx.guiApplicationPlugin<Curve::ApplicationPlugin>().editionSettings().setTool(
        Curve::Tool::Disabled);
    CommandDispatcher<>{doc->context().commandStack}.submit(
        new Process::SetControlValue{*inlet, peak()});

    std::unique_ptr<QWidget> widget{WidgetFactory::CurveInletItems::make_widget(
        *inlet, doc->context(), nullptr, inlet)};
    auto* view = qobject_cast<QGraphicsView*>(widget.get());
    REQUIRE(view);
    view->resize(308, 128);
    REQUIRE(score::test::showAndActivate(*view));
    score::test::settle();

    const auto selected = doc->context().selectionStack.currentSelection();
    QGraphicsItem* curve{};
    for(auto* item : view->scene()->items())
      if(!item->parentItem())
        curve = item;
    REQUIRE(curve);
    const QSizeF size = curve->boundingRect().size();
    CHECK(size.width() == Catch::Approx(300.));

    dragMiddlePoint(*view, curve->scenePos(), size);
    checkDragged(*inlet);
    CHECK(doc->context().selectionStack.currentSelection() == selected);
  });
}

TEST_CASE("A curve port ignores values that hold no curve", "[curve][dataflow]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Dataflow::CurveInlet inlet{Id<Process::Port>{1}, nullptr};
    inlet.setValue(peak());
    REQUIRE(points(*inlet.m_curve).size() == 3);

    inlet.setValue(std::vector<ossia::value>{});
    CHECK(points(*inlet.m_curve).size() == 3);

    // Segments that do not advance in x are dropped, the others kept.
    inlet.setValue(std::vector<ossia::value>{
        segment(0.f, 0.f, .5f, .5f), segment(.5f, .5f, .5f, 1.f),
        segment(.5f, 1.f, 1.f, 0.f)});
    const auto& sorted = inlet.m_curve->sortedSegments();
    REQUIRE(sorted.size() == 2);
    CHECK(sorted[0]->end() == Curve::Point{.5, .5});
    CHECK(sorted[1]->start() == Curve::Point{.5, 1.});

    // Only degenerate segments: nothing to show, the curve stays.
    inlet.setValue(std::vector<ossia::value>{segment(.5f, 0.f, .5f, 1.f)});
    CHECK(points(*inlet.m_curve).size() == 3);
  });
}

TEST_CASE(
    "A curve port outside of a document takes its value, as a preset does",
    "[curve][dataflow][preset]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Dataflow::CurveInlet inlet{Id<Process::Port>{1}, nullptr};
    inlet.setValue(std::vector<ossia::value>{
        segment(0.f, 0.f, .25f, 1.f, 2.f), segment(.25f, 1.f, 1.f, .5f)});

    const auto& sorted = inlet.m_curve->sortedSegments();
    REQUIRE(sorted.size() == 2);
    auto* first = qobject_cast<const Curve::PowerSegment*>(sorted[0]);
    REQUIRE(first);
    CHECK(first->gamma == Catch::Approx(2.));
    CHECK(sorted[1]->end().y() == Catch::Approx(.5));
  });
}
