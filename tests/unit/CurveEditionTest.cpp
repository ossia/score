// Curve edition as the GUI drives it: mouse positions fed to Curve::ToolPalette
// on a real Presenter and View. Scenarios that can abort run in a forked child
// (ForkProbe), which also checks the model's invariants.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/ForkProbe.hpp>

#include <Curve/Commands/MovePoint.hpp>
#include <Curve/Commands/SetSegmentParameters.hpp>
#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/CurveConversion.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Envelope.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/CurveView.hpp>
#include <Curve/Palette/CommandObjects/CreatePointCommandObject.hpp>
#include <Curve/Palette/CommandObjects/MovePointCommandObject.hpp>
#include <Curve/Palette/CommandObjects/SetSegmentParametersCommandObject.hpp>
#include <Curve/Palette/CurveEditionSettings.hpp>
#include <Curve/Palette/CurvePaletteBaseStates.hpp>
#include <Curve/Palette/CurvePalette.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <Curve/Segment/CurveSegmentView.hpp>
#include <Curve/Settings/CurveSettingsModel.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>
#include <Curve/Segment/PointArray/PointArraySegment.hpp>
#include <Curve/Segment/EasingSegment.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Automation/AutomationColors.hpp>
#include <Automation/AutomationModel.hpp>
#include <Automation/State/AutomationState.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>

#include <ossia/dataflow/nodes/automation.hpp>
#include <ossia/detail/thread.hpp>
#include <ossia/editor/curve/curve.hpp>
#include <ossia/editor/curve/curve_segment/linear.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <score/model/ObjectEditor.hpp>
#include <score/model/Skin.hpp>
#include <score/selection/Selection.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/command/CommandStackSerialization.hpp>
#include <core/document/DocumentBackupManager.hpp>
#include <core/document/DocumentModel.hpp>

#include <QDataStream>
#include <QFile>

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QImage>
#include <QPixmap>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QWindow>
#include <qpa/qwindowsysteminterface.h>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <ossia/detail/hash_map.hpp>
#include <ossia/math/safe_math.hpp>

#include <chrono>
#include <limits>
#include <map>
#include <cmath>
#include <random>
#include <cstdio>
#include <sstream>

using Catch::Approx;

namespace
{
using Points = std::vector<QPointF>;

void settle()
{
  for(int i = 0; i < 3; i++)
  {
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();
  }
}

Scenario::IntervalModel& baseInterval(score::Document& doc)
{
  return static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate())
      .baseInterval();
}

//! Every invariant the rest of the plug-in relies on, as a readable list.
//! Linear: it also checks curves of a hundred thousand segments.
std::string curveError(const Curve::Model& m)
{
  std::ostringstream err;
  const auto segs = Curve::orderedSegments(m);
  ossia::hash_map<int32_t, const Curve::SegmentData*> by_id;
  for(auto& s : segs)
    by_id.emplace(s.id.val(), &s);
  auto find = [&](const Id<Curve::SegmentModel>& id) -> const Curve::SegmentData* {
    auto it = by_id.find(id.val());
    return it != by_id.end() ? it->second : nullptr;
  };
  auto finite = [](QPointF p) {
    return ossia::safe_isfinite(p.x()) && ossia::safe_isfinite(p.y());
  };

  // The points at the start and at the end of each segment.
  ossia::hash_map<int32_t, std::vector<const Curve::PointModel*>> starts, ends;
  for(auto pt : m.points())
  {
    if(pt->following())
      starts[pt->following()->val()].push_back(pt);
    if(pt->previous())
      ends[pt->previous()->val()].push_back(pt);
  }

  int heads = 0;
  for(auto& s : segs)
  {
    const auto id = s.id.val();
    if(!finite(s.start) || !finite(s.end))
      err << "segment " << id << " has a non-finite coordinate; ";
    if(s.start.x() > s.end.x())
      err << "segment " << id << " runs backwards; ";
    if(!s.previous)
      heads++;
    if(s.previous)
    {
      auto p = find(*s.previous);
      if(!p)
        err << "segment " << id << " has a dangling previous; ";
      else if(p->following != s.id)
        err << "segment " << id << "'s previous does not link back; ";
      else if(p->end != s.start)
        err << "segment " << id << " does not start where its previous ends; ";
    }
    if(s.following)
    {
      auto f = find(*s.following);
      if(!f)
        err << "segment " << id << " has a dangling following; ";
      else if(f->previous != s.id)
        err << "segment " << id << "'s following does not link back; ";
    }

    const auto& st = starts[id];
    const auto& en = ends[id];
    if(st.size() != 1 || en.size() != 1)
      err << "segment " << id << " has " << st.size() << " start and " << en.size()
          << " end points; ";
    for(auto pt : st)
      if(pt->pos() != s.start)
        err << "the start point of segment " << id << " is misplaced; ";
    for(auto pt : en)
      if(pt->pos() != s.end)
        err << "the end point of segment " << id << " is misplaced; ";
  }

  auto describe = [](const Curve::SegmentData& s) {
    std::ostringstream o;
    o.precision(17);
    o << s.id.val() << " [" << s.start.x() << ", " << s.end.x() << "]";
    return o.str();
  };
  // Points compare fuzzily (QPointF): so does this.
  for(std::size_t i = 0; i + 1 < segs.size(); i++)
    if(segs[i + 1].start.x() < segs[i].end.x() - 1e-9)
      err << "segments " << describe(segs[i]) << " and " << describe(segs[i + 1])
          << " overlap; ";

  for(auto pt : m.points())
  {
    if(!pt->previous() && !pt->following())
      err << "a point belongs to no segment; ";
    if(pt->previous() && !find(*pt->previous()))
      err << "a point refers to a removed previous segment; ";
    if(pt->following() && !find(*pt->following()))
      err << "a point refers to a removed following segment; ";
  }
  if(m.points().size() != segs.size() + heads)
    err << m.points().size() << " points for " << segs.size() << " segments; ";

  return err.str();
}

const Curve::PointModel* pointAt(const Curve::Model& m, QPointF p)
{
  for(auto pt : m.points())
    if(std::abs(pt->pos().x() - p.x()) < 1e-6 && std::abs(pt->pos().y() - p.y()) < 1e-6)
      return pt;
  return nullptr;
}

bool hasPointAtX(const Curve::Model& m, double x)
{
  for(auto pt : m.points())
    if(std::abs(pt->pos().x() - x) < 1e-6)
      return true;
  return false;
}

//! Runs a scenario in a forked child. Fails if the child crashes or asserts,
//! or if the scenario returns a description of what it found broken.
//! Same segments, ends, links and types, in the same order.
bool sameCurve(
    const std::vector<Curve::SegmentData>& a, const std::vector<Curve::SegmentData>& b)
{
  return std::equal(
      a.begin(), a.end(), b.begin(), b.end(),
      [](const Curve::SegmentData& x, const Curve::SegmentData& y) {
    return x.id == y.id && x.start == y.start && x.end == y.end
           && x.previous == y.previous && x.following == y.following
           && x.type == y.type;
  });
}

template <typename F>
bool editSurvives(F&& scenario)
{
#if defined(THREEDIM_HAS_FORK)
  return threedim_test::survives([&] {
    const std::string err = scenario();
    if(!err.empty())
    {
      std::fprintf(stderr, "[curve] %s\n", err.c_str());
      std::fflush(stderr);
      ::_exit(3);
    }
  });
#else
  // No fork: in process, where a crash takes the whole test binary down.
  const std::string err = scenario();
  if(!err.empty())
    std::fprintf(stderr, "[curve] %s\n", err.c_str());
  return err.empty();
#endif
}

//! A document holding one automation, as created from the library.
struct CurveDoc
{
  score::Document* doc{};
  Automation::ProcessModel* autom{};

  explicit CurveDoc(const score::GUIApplicationContext& ctx)
  {
    doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Scenario::Command::Macro m{
        new Scenario::Command::DropProcessInIntervalMacro, doc->context()};
    autom = &m.createProcessInNewSlot<Automation::ProcessModel>(
        baseInterval(*doc), QString{});
    m.commit();
    settle();
  }

  Curve::Model& curve() const { return autom->curve(); }
  const score::DocumentContext& context() const { return doc->context(); }
  score::CommandStack& stack() const { return doc->commandStack(); }

  //! What the JS API's setCurvePoints, or any other UpdateCurve, produces.
  void setPolyline(const Points& pts, double gamma = 1.) const
  {
    std::vector<Curve::SegmentData> segs;
    for(std::size_t i = 0; i + 1 < pts.size(); i++)
    {
      Curve::SegmentData d;
      d.id = Id<Curve::SegmentModel>{int(100 + i)};
      d.start = pts[i];
      d.end = pts[i + 1];
      if(i > 0)
        d.previous = Id<Curve::SegmentModel>{int(100 + i - 1)};
      if(i + 2 < pts.size())
        d.following = Id<Curve::SegmentModel>{int(100 + i + 1)};
      d.type = Metadata<ConcreteKey_k, Curve::PowerSegment>::get();
      d.specificSegmentData = QVariant::fromValue(Curve::PowerSegmentData{gamma});
      segs.push_back(std::move(d));
    }
    CommandDispatcher<>{context().commandStack}.submit(
        new Curve::UpdateCurve{curve(), std::move(segs)});
    settle();
  }
};

//! The global edition settings, restored on scope exit.
struct EditionSettingsGuard
{
  Curve::EditionSettings& s;
  bool lock = s.lockBetweenPoints();
  bool suppress = s.suppressOnOverlap();
  Curve::Tool tool = s.tool();
  ~EditionSettingsGuard()
  {
    s.setLockBetweenPoints(lock);
    s.setSuppressOnOverlap(suppress);
    s.setTool(tool);
  }
};

//! Fixed colours: pixel checks must not depend on the skin being loaded.
struct TestStyle
{
  QBrush point{QColor{128, 215, 62}}, pointSelected{QColor{233, 208, 89}};
  QBrush segment{QColor{199, 31, 44}}, segmentSelected{QColor{216, 178, 24}};
  QBrush segmentDisabled{QColor{127, 127, 127}};
  Curve::Style style{point, pointSelected, segment, segmentSelected, segmentDisabled};
  TestStyle() { style.update(); }
};

//! A curve layer as CurveProcessPresenter builds it, minus the process view.
struct CurveUi
{
  const CurveDoc& d;
  QRectF rect;
  QGraphicsScene scene;
  QObject focus; // what the paste action is given as the focused object
  TestStyle colors;
  std::unique_ptr<Curve::Presenter> presenter;
  std::unique_ptr<Curve::ToolPalette> palette;

  explicit CurveUi(const CurveDoc& d, QRectF r = {0., 0., 1000., 200.})
      : d{d}
      , rect{r}
  {
    auto view = new Curve::View{nullptr};
    scene.addItem(view);
    presenter = std::make_unique<Curve::Presenter>(
        d.context(), colors.style, d.curve(), view, &focus);
    view->setRect(rect);
    view->setDefaultWidth(rect.width());
    presenter->setRect(rect);
    palette = std::make_unique<Curve::ToolPalette>(d.context(), *presenter);
    settings().setTool(Curve::Tool::Select);
    // As ScenarioDocumentPresenter does for the focused process.
    QObject::connect(
        &d.doc->selectionStack(), &score::SelectionStack::currentSelectionChanged, &focus,
        [&c = d.curve()](const Selection&, const Selection& s) { c.setSelection(s); });
    settle();
  }

  ~CurveUi()
  {
    palette.reset();
    presenter.reset();
    settle();
  }

  Curve::EditionSettings& settings() const { return presenter->editionSettings(); }

  QPointF toScene(QPointF c) const
  {
    return {c.x() * rect.width(), (1. - c.y()) * rect.height()};
  }

  void press(QPointF c)
  {
    palette->on_pressed(toScene(c));
    settle();
  }
  void move(QPointF c)
  {
    palette->on_moved(toScene(c));
    settle();
  }
  void release(QPointF c)
  {
    palette->on_released(toScene(c));
    settle();
  }

  void drag(QPointF from, const Points& path)
  {
    press(from);
    for(auto p : path)
      move(p);
    release(path.back());
  }

  //! What a zoom or a resize of the layer does.
  void relayout(double width)
  {
    rect.setWidth(width);
    presenter->view().setRect(rect);
    presenter->setRect(rect);
    settle();
  }
};

double gammaOf(const Curve::Model& m, int id)
{
  for(const auto& s : m.toCurveData())
    if(s.id.val() == id)
      return s.specificSegmentData.value<Curve::PowerSegmentData>().gamma;
  return -1.;
}

//! Point counts of the pen's in-progress segments, in x order.
std::vector<int> penPointCounts(const Curve::Model& m)
{
  std::vector<int> res;
  for(const auto& s : Curve::orderedSegments(m))
    if(s.type == Metadata<ConcreteKey_k, Curve::PointArraySegment>::get())
    {
      const auto dat = s.specificSegmentData.value<Curve::PointArraySegmentData>();
      res.push_back(dat.points ? int(dat.points->size()) : 0);
    }
  return res;
}

score::ObjectEditor& curveEditor(const score::GUIApplicationContext& ctx)
{
  auto ed = ctx.interfaces<score::ObjectEditorList>().get(
      UuidKey<score::ObjectEditor>{"d2b20e55-296f-49cc-a1c5-1ba1a1122d07"});
  REQUIRE(ed);
  return *ed;
}
}

TEST_CASE("A point drag with the default settings keeps the curve valid", "[curve][edition]")
{
  // Baseline: the path every user takes.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
    REQUIRE(curveError(d.curve()).empty());

    CurveUi ui{d};
    ui.drag({0.5, 1.}, {{0.55, 0.8}, {0.6, 0.3}});
    CHECK(curveError(d.curve()) == "");
    CHECK(pointAt(d.curve(), {0.6, 0.3}));

    d.stack().undo();
    settle();
    CHECK(curveError(d.curve()) == "");
    CHECK(pointAt(d.curve(), {0.5, 1.}));
  });
}

TEST_CASE(
    "Full view: dragging a middle point past the last point with the default settings",
    "[curve][edition][crash]")
{
  // Full view: the move is unbounded, the point can go past the last one.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CHECK(editSurvives([&]() -> std::string {
      CurveDoc d{ctx};
      d.setPolyline({{0., 0.}, {0.33, 0.5}, {0.66, 0.2}, {1., 1.}});
      CurveUi ui{d};
      ui.presenter->setBoundedMove(false);

      ui.drag({0.33, 0.5}, {{0.5, 0.5}, {0.9, 0.5}, {1.2, 0.5}});
      if(auto e = curveError(d.curve()); !e.empty())
        return "after the drag: " + e;
      if(!hasPointAtX(d.curve(), 1.2))
        return "the dragged point did not follow the cursor";

      // What the user does next: grab the point again.
      ui.drag({1.2, 0.5}, {{1.1, 0.4}});
      ui.relayout(1500.);
      return curveError(d.curve());
    }));
  });
}

TEST_CASE(
    "Crossing a point into the last segment with lock and suppress disabled",
    "[curve][edition][crash]")
{
  // The point lands in the last segment while the ones it crosses are erased.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CHECK(editSurvives([&]() -> std::string {
      CurveDoc d{ctx};
      d.setPolyline({{0., 0.}, {0.25, 0.5}, {0.5, 0.2}, {1., 1.}});
      CurveUi ui{d};
      EditionSettingsGuard g{ui.settings()};
      ui.settings().setLockBetweenPoints(false);
      ui.settings().setSuppressOnOverlap(false);

      ui.drag({0.25, 0.5}, {{0.7, 0.5}, {0.72, 0.5}});
      ui.relayout(1500.);
      if(auto e = curveError(d.curve()); !e.empty())
        return e;
      if(!hasPointAtX(d.curve(), 0.72))
        return "the dragged point did not follow the cursor";
      return {};
    }));
  });
}

TEST_CASE(
    "With suppress-on-overlap off, a plain drag of a middle point to the right",
    "[curve][edition][crash]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CHECK(editSurvives([&]() -> std::string {
      CurveDoc d{ctx};
      d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
      CurveUi ui{d};
      EditionSettingsGuard g{ui.settings()};
      ui.settings().setSuppressOnOverlap(false);

      ui.drag({0.5, 1.}, {{0.6, 1.}});
      if(auto e = curveError(d.curve()); !e.empty())
        return e;
      if(!pointAt(d.curve(), {0.6, 1.}))
        return "the dragged point did not follow the cursor";
      return {};
    }));
  });
}

TEST_CASE(
    "Suppress on overlap: dragging the last point left over its neighbour",
    "[curve][edition][crash]")
{
  // The neighbour goes with the segment between them: the clicked point's previous.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CHECK(editSurvives([&]() -> std::string {
      CurveDoc d{ctx};
      d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
      CurveUi ui{d};
      EditionSettingsGuard g{ui.settings()};
      ui.settings().setLockBetweenPoints(false);

      ui.drag({1., 0.}, {{0.8, 0.}, {0.4, 0.2}});
      if(auto e = curveError(d.curve()); !e.empty())
        return e;
      if(!pointAt(d.curve(), {0.4, 0.2}))
        return "the dragged point is stuck where the last accepted move left it";
      if(hasPointAtX(d.curve(), 0.5))
        return "the overlapped point was not suppressed";
      return {};
    }));
  });
}

TEST_CASE(
    "Suppress on overlap: dragging the first point right over its neighbour",
    "[curve][edition][crash]")
{
  // Mirror of the previous case: the clicked point's following segment goes.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CHECK(editSurvives([&]() -> std::string {
      CurveDoc d{ctx};
      d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
      CurveUi ui{d};
      EditionSettingsGuard g{ui.settings()};
      ui.settings().setLockBetweenPoints(false);

      ui.drag({0., 0.}, {{0.2, 0.}, {0.7, 0.3}});
      if(auto e = curveError(d.curve()); !e.empty())
        return e;
      if(!pointAt(d.curve(), {0.7, 0.3}))
        return "the dragged point is stuck where the last accepted move left it";
      return {};
    }));
  });
}

TEST_CASE(
    "A drag whose release was lost survives its layer being closed",
    "[curve][edition][crash]")
{
  // The value tooltip must not outlive the view that showed it.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CHECK(editSurvives([&]() -> std::string {
      CurveDoc d{ctx};
      d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
      {
        CurveUi first{d};
        first.press({0.5, 1.});
        first.move({0.5, 0.8});
        // No release: the layer goes away while the drag is in flight.
      }

      CurveUi second{d};
      second.drag({0.5, 0.8}, {{0.5, 0.6}});
      return curveError(d.curve());
    }));
  });
}

TEST_CASE("Pasting a copied segment at the start of a curve", "[curve][edition][crash]")
{
  // The paste cuts into one of the last two segments.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CHECK(editSurvives([&]() -> std::string {
      CurveDoc d{ctx};
      d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
      CurveUi ui{d};

      QGraphicsView gv{&ui.scene};
      gv.setFrameStyle(QFrame::NoFrame);
      gv.setSceneRect(ui.rect);
      gv.resize(1000, 200);
      gv.show();
      settle();

      Selection sel;
      for(auto& seg : d.curve().segments())
        if(seg.start().x() == 0.)
          sel.append(seg);

      auto& ed = curveEditor(ctx);
      JSONReader r;
      if(!ed.copy(r, sel, d.context()))
        return "copy refused the selected segment";

      QMimeData mime;
      mime.setData("text/plain", r.toByteArray());
      const QPoint global = gv.mapToGlobal(gv.mapFromScene(ui.toScene({0., 0.5})));
      ed.paste(global, &ui.focus, mime, d.context());
      settle();

      if(auto e = curveError(d.curve()); !e.empty())
        return e;
      const auto v = d.curve().valueAt(0.25);
      if(!v || std::abs(*v - 0.5) > 1e-6)
        return "the pasted segment is not where it was pasted";
      if(!d.curve().valueAt(0.9))
        return "the paste left a hole at the end of the curve";
      return {};
    }));
  });
}

TEST_CASE(
    "Undoing a point edit made in the inspector after another curve edit",
    "[curve][edition][undo]")
{
  // MovePoint must find its point again after the curve was rebuilt around
  // it, whatever ids the points got in between.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.5, 0.5}, {1., 0.}});

    // The Y spin box of the point inspector.
    auto mid = pointAt(d.curve(), {0.5, 0.5});
    REQUIRE(mid);
    CommandDispatcher<>{d.context().commandStack}.submit(
        new Curve::MovePoint{d.curve(), mid->id(), {0.5, 0.9}});
    settle();
    REQUIRE(pointAt(d.curve(), {0.5, 0.9}));

    // Any drag afterwards.
    {
      CurveUi ui{d};
      ui.drag({1., 0.}, {{1., 0.2}});
    }
    REQUIRE(pointAt(d.curve(), {1., 0.2}));

    d.stack().undo();
    settle();
    CHECK(pointAt(d.curve(), {1., 0.}));

    d.stack().undo();
    settle();
    CHECK(pointAt(d.curve(), {0.5, 0.5}));
    CHECK(curveError(d.curve()) == "");
  });
}

TEST_CASE(
    "A vertical step (as setCurvePoints makes) can be edited",
    "[curve][edition][crash]")
{
  // A zero-width segment does not overlap the one that starts at its x.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CHECK(editSurvives([&]() -> std::string {
      CurveDoc d{ctx};
      d.setPolyline({{0., 0.}, {0.5, 0.}, {0.5, 1.}, {1., 1.}});
      if(auto e = curveError(d.curve()); !e.empty())
        return "after loading the step: " + e;

      CurveUi ui{d};
      ui.drag({1., 1.}, {{1., 0.5}});
      if(auto e = curveError(d.curve()); !e.empty())
        return e;
      if(!pointAt(d.curve(), {1., 0.5}))
        return "the last point did not move";
      return {};
    }));
  });
}

TEST_CASE(
    "A non-finite cursor position never reaches the model",
    "[curve][edition][crash]")
{
  // A layer with no height: ToolPalette divides by it. Only the first move
  // constructs the command, the others update it.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CHECK(editSurvives([&]() -> std::string {
      CurveDoc d{ctx};
      d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
      CurveUi ui{d, QRectF{0., 0., 1000., 0.}};

      ui.press({0.5, 1.});
      ui.move({0.55, 1.});
      ui.move({0.6, 1.});
      ui.release({0.6, 1.});
      return curveError(d.curve());
    }));
  });
}

TEST_CASE("A pen stroke across points keeps the curve valid", "[curve][edition]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CHECK(editSurvives([&]() -> std::string {
      CurveDoc d{ctx};
      d.setPolyline({{0., 0.}, {0.3, 1.}, {0.6, 0.}, {1., 1.}});
      CurveUi ui{d};
      ui.settings().setTool(Curve::Tool::CreatePen);

      Points stroke;
      for(int i = 0; i <= 40; i++)
      {
        const double x = 0.8 - i * 0.015;
        stroke.push_back({x, 0.5 + 0.3 * std::sin(x * 20.)});
      }
      ui.drag({0.8, 0.5}, stroke);
      return curveError(d.curve());
    }));
  });
}

TEST_CASE("Curve::Model::removeSegment detaches its points", "[curve][model][crash]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CHECK(editSurvives([&]() -> std::string {
      CurveDoc d{ctx};
      d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
      auto& curve = d.curve();
      Curve::SegmentModel* first{};
      for(auto& seg : curve.segments())
        if(!seg.previous())
          first = &seg;
      if(!first)
        return "no first segment";

      curve.removeSegment(first);
      if(curve.segments().size() != 1)
        return "the segment was not removed";
      for(auto& seg : curve.segments())
        seg.setPrevious(std::nullopt);
      return curveError(curve);
    }));
  });
}

TEST_CASE("SegmentData keeps its id through JSON", "[curve][serialization]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Curve::SegmentData d{
        Id<Curve::SegmentModel>{42},
        {0., 0.},
        {1., 1.},
        std::nullopt,
        std::nullopt,
        Metadata<ConcreteKey_k, Curve::PowerSegment>::get(),
        QVariant::fromValue(Curve::PowerSegmentData{2.})};

    JSONReader r;
    r.stream.StartObject();
    r.obj["Segments"] = std::vector<Curve::SegmentData>{d};
    r.stream.EndObject();

    const auto doc = readJson(r.toByteArray());
    const auto back
        = JsonValue{doc["Segments"]}.to<std::vector<Curve::SegmentData>>();
    REQUIRE(back.size() == 1);
    CHECK(back[0].id == d.id);
    CHECK(!back[0].previous);
    CHECK(back[0].specificSegmentData.value<Curve::PowerSegmentData>().gamma
          == Approx(2.));
  });
}

TEST_CASE(
    "A script's undo/redo sequence over a curvature change",
    "[curve][edition][undo]")
{
  // Undo and redo in a row, without the event loop: the replaced segments are
  // still alive under the same ids.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
    REQUIRE(gammaOf(d.curve(), 100) == Approx(1.));

    // Shift-drag on the first segment.
    CommandDispatcher<>{d.context().commandStack}.submit(new Curve::SetSegmentParameters{
        d.curve(), Curve::SegmentParameterMap{{Id<Curve::SegmentModel>{100}, {0.5, 0.}}}});
    settle();
    const double bent = gammaOf(d.curve(), 100);
    REQUIRE(bent != Approx(1.));

    {
      CurveUi ui{d};
      ui.drag({1., 0.}, {{1., 0.2}});
    }
    REQUIRE(gammaOf(d.curve(), 100) == Approx(bent));

    auto& stack = d.stack();
    stack.undo();
    stack.undo();
    stack.redo();
    stack.redo();
    stack.undo();
    stack.undo();
    settle();

    CHECK(gammaOf(d.curve(), 100) == Approx(1.));
    CHECK(curveError(d.curve()) == "");
  });
}

TEST_CASE(
    "An undo between a click on a point and its processing",
    "[curve][edition][crash]")
{
  // The undo deletes the clicked point's view before the tool processes the click.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CHECK(editSurvives([&]() -> std::string {
      CurveDoc d{ctx};
      d.setPolyline({{0., 0.}, {1., 1.}});
      d.setPolyline({{0., 0.}, {0.25, 1.}, {0.5, 0.}, {0.75, 1.}, {1., 0.}});
      CurveUi ui{d};

      ui.palette->on_pressed(ui.toScene({1., 0.}));
      d.stack().undo();
      settle();
      ui.move({1., 0.5});
      ui.release({1., 0.5});
      return curveError(d.curve());
    }));
  });
}

TEST_CASE(
    "After a drag whose release was lost, the next drag moves the right point",
    "[curve][edition]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CHECK(editSurvives([&]() -> std::string {
      CurveDoc d{ctx};
      d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
      CurveUi ui{d};

      ui.press({0.5, 1.});
      ui.move({0.5, 0.8});

      ui.drag({1., 0.}, {{1., 0.3}});
      if(auto e = curveError(d.curve()); !e.empty())
        return e;
      if(!pointAt(d.curve(), {1., 0.3}))
        return "the clicked point did not move";
      if(!pointAt(d.curve(), {0.5, 0.8}))
        return "the point of the lost gesture moved again";
      return {};
    }));
  });
}

TEST_CASE("Saving while a pen stroke is in progress", "[curve][edition][serialization]")
{
  // While the pen is down, the stroke is a PointArraySegment.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CHECK(editSurvives([&]() -> std::string {
      CurveDoc d{ctx};
      d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
      CurveUi ui{d};
      ui.settings().setTool(Curve::Tool::CreatePen);

      ui.press({0.2, 0.4});
      for(int i = 1; i <= 10; i++)
        ui.move({0.2 + i * 0.05, 0.4 + 0.02 * i});

      const auto before = penPointCounts(d.curve());
      if(before.empty())
        return "the pen stroke is not in the curve";

      const auto id = d.autom->id();
      for(auto reloaded :
          {score::test::reload_via_bytes(ctx, *d.doc),
           score::test::reload_via_json(ctx, *d.doc)})
      {
        if(!reloaded)
          return "the document did not reload";
        auto& itv = baseInterval(*reloaded);
        auto it = itv.processes.find(id);
        if(it == itv.processes.end())
          return "the automation is missing after reload";
        auto& curve = static_cast<Automation::ProcessModel&>(*it).curve();
        if(penPointCounts(curve) != before)
          return "the pen stroke's points were not saved";
        if(auto e = curveError(curve); !e.empty())
          return "after reload: " + e;
      }
      return {};
    }));
  });
}

TEST_CASE(
    "A check failing inside UpdateCurve's redo leaves the curve and its views usable",
    "[curve][edition][crash]")
{
  // An invalid curve is refused before anything is cleared: the views stay on
  // live models.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CHECK(editSurvives([&]() -> std::string {
      CurveDoc d{ctx};
      d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
      CurveUi ui{d};

      auto good = d.curve().toCurveData();
      auto broken = good;
      for(auto& s : broken)
        if(!s.previous)
          s.previous = Id<Curve::SegmentModel>{12345};

      SingleOngoingCommandDispatcher<Curve::UpdateCurve> disp{d.context().commandStack};
      disp.submit(d.curve(), good);
      disp.submit(d.curve(), broken);
      settle();
      if(!sameCurve(d.curve().toCurveData(), good))
        return "the refused curve was applied";
      ui.relayout(1500.);
      disp.rollback();
      settle();
      return curveError(d.curve());
    }));
  });
}

TEST_CASE("A curved segment interpolates between both of its ends", "[curve][model]")
{
  // Including with no interpolation steps, and with no width.
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Curve::PowerSegment seg{Id<Curve::SegmentModel>{1}, nullptr};
    seg.setStart({0.2, 0.1});
    seg.setEnd({0.8, 0.9});
    seg.gamma = 3.;

    const Curve::SegmentModel& base = seg;
    for(int n : {0, 1, 2, 10, 75})
    {
      INFO("numInterp = " << n);
      base.updateData(n);
      const auto& pts = base.data();
      CHECK(pts.size() >= 2);
      if(pts.empty())
        continue;
      CHECK(pts.front() == seg.start());
      CHECK(pts.back() == seg.end());
      for(auto p : pts)
        CHECK((ossia::safe_isfinite(p.x()) && ossia::safe_isfinite(p.y())));
    }

    Curve::PowerSegment step{Id<Curve::SegmentModel>{2}, nullptr};
    step.setStart({0.5, 0.});
    step.setEnd({0.5, 1.});
    CHECK(ossia::safe_isfinite(static_cast<const Curve::SegmentModel&>(step).valueAt(0.5)));
  });
}

TEST_CASE("A recording with a single value converts to no segment", "[curve][model][crash]")
{
  // A recording stopped on the millisecond it started: one point, no segment.
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    CHECK(editSurvives([&]() -> std::string {
      Curve::PointArraySegment seg{Id<Curve::SegmentModel>{1}, nullptr};
      seg.setStart({0., 0.});
      seg.setEnd({1., 0.});
      seg.addPoint(250., 0.4);
      for(const auto& segs : {seg.toPowerSegments(), seg.toLinearSegments()})
        if(!segs.empty())
          return "one point made " + std::to_string(segs.size()) + " segments";
      return {};
    }));
  });
}

TEST_CASE("An UpdateCurve keeps only what it changes", "[curve][edition][performance]")
{
  // The undo stack holds every command: one that kept the whole curve before
  // and after would cost twice the curve per edit.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    Points pts;
    for(int i = 0; i <= 1000; i++)
      pts.push_back({i / 1000., (i % 2) ? 1. : 0.});
    d.setPolyline(pts);

    auto data = d.curve().toCurveData();
    for(auto& s : data)
    {
      if(s.end.x() == 0.5)
        s.end.ry() = 0.25;
      if(s.start.x() == 0.5)
        s.start.ry() = 0.25;
    }
    Curve::UpdateCurve cmd{d.curve(), data};
    CHECK(cmd.changes().size() == 2);

    cmd.redo(d.context());
    CHECK(d.curve().valueAt(0.5) == Approx(0.25));
    cmd.undo(d.context());
    CHECK(d.curve().valueAt(0.5) == Approx(0.));
    CHECK(curveError(d.curve()) == "");
  });
}

TEST_CASE(
    "Point views follow their point after the curve was rebuilt",
    "[curve][edition]")
{
  // Presenter::modelReset hands the existing views to other points; a view
  // must then move with its new point, not its first one.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
    CurveUi ui{d};

    // Adds a point: the views are re-dealt.
    ui.palette->createPoint(ui.toScene({0.25, 0.5}));
    settle();
    REQUIRE(d.curve().points().size() == 4);

    ui.drag({0.5, 1.}, {{0.5, 0.7}});
    REQUIRE(pointAt(d.curve(), {0.5, 0.7}));

    for(const auto& view : ui.presenter->points())
    {
      const auto expected = ui.toScene(view.model().pos());
      CHECK(view.pos().x() == Approx(expected.x()));
      CHECK(view.pos().y() == Approx(expected.y()));
    }
  });
}

TEST_CASE("Editing a large automation costs in proportion to its size", "[curve][edition][performance]")
{
  // Loading, dragging, undoing, drawing and removing: ten times the segments
  // must cost about ten times as much, not a hundred. A ratio holds on slow,
  // loaded or instrumented machines, where a time limit would not.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto edit = [&](int N) {
      const auto t0 = std::chrono::steady_clock::now();
      CurveDoc d{ctx};
      Points pts;
      pts.reserve(N + 1);
      for(int i = 0; i <= N; i++)
        pts.push_back({double(i) / N, (i % 2) ? 1. : 0.});
      d.setPolyline(pts);
      REQUIRE(d.curve().segments().size() == std::size_t(N));

      CurveUi ui{d};
      const Curve::PointModel* pt{};
      for(auto p : d.curve().points())
        if(p->pos().x() >= 0.5)
        {
          pt = p;
          break;
        }
      REQUIRE(pt);

      Curve::StateBase state;
      Curve::MovePointCommandObject co{
          d.curve(), ui.presenter.get(), d.context().commandStack};
      co.setCurveState(&state);
      state.clickedPointId = {pt->previous(), pt->following()};
      state.currentPoint = pt->pos();
      const auto orig = pt->pos();
      co.press();
      for(int i = 0; i < 5; i++)
      {
        state.currentPoint = {orig.x(), 0.1 * i};
        co.move();
      }
      co.release();
      settle();

      d.stack().undo();
      d.stack().redo();
      settle();

      ui.settings().setTool(Curve::Tool::CreatePen);
      ui.press({0.3, 0.5});
      for(int i = 1; i <= 20; i++)
        ui.move({0.3 + i * 0.001, 0.5});
      ui.release({0.32, 0.5});

      for(int i = 0; i < 100; i++)
        if(auto p = d.curve().points()[N / 100 + i]; p->previous() && p->following())
          const_cast<Curve::PointModel*>(p)->selection.set(true);
      ui.presenter->removeSelection();
      settle();

      CHECK(curveError(d.curve()) == "");
      return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    };

    edit(5000); // warm-up: first-use costs are not the curve's
    const double small = edit(5000);
    const double large = edit(50000);
    INFO("5k: " << small << " s, 50k: " << large << " s");
    CHECK(large / small < 35.);
  });
}

TEST_CASE(
    "Pasting segments into a curve keeps what is outside the pasted range",
    "[curve][edition]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CHECK(editSurvives([&]() -> std::string {
      CurveDoc d{ctx};
      Points pts;
      for(int i = 0; i <= 1000; i++)
        pts.push_back({i / 1000., (i % 2) ? 1. : 0.});
      d.setPolyline(pts);
      CurveUi ui{d};

      QGraphicsView gv{&ui.scene};
      gv.setFrameStyle(QFrame::NoFrame);
      gv.setSceneRect(ui.rect);
      gv.resize(1000, 200);
      gv.show();
      settle();

      Selection sel;
      for(auto& seg : d.curve().segments())
        if(seg.start().x() >= 0.1 && seg.end().x() <= 0.2)
          sel.append(seg);
      auto& ed = curveEditor(ctx);
      JSONReader r;
      if(!ed.copy(r, sel, d.context()))
        return "copy refused the selected segments";

      QMimeData mime;
      mime.setData("text/plain", r.toByteArray());
      const QPoint global = gv.mapToGlobal(gv.mapFromScene(ui.toScene({0.8, 0.5})));
      ed.paste(global, &ui.focus, mime, d.context());
      settle();

      if(auto e = curveError(d.curve()); !e.empty())
        return e;
      // [0, 0.8) and (0.9, 1] as they were, the 100 pasted in between.
      const auto n = d.curve().segments().size();
      if(n < 990 || n > 1010)
        return "the curve has " + std::to_string(n) + " segments instead of about 1000";
      for(double x : {0.05, 0.35, 0.75, 0.95})
      {
        const auto v = d.curve().valueAt(x + 0.0005);
        if(!v || std::abs(*v - 0.5) > 1e-6)
          return "the curve changed outside the pasted range, at " + std::to_string(x);
      }
      return {};
    }));
  });
}

TEST_CASE(
    "The crash backup of the commands matches the stack through edits, undo and redo",
    "[curve][edition][backup]")
{
  // Each command is serialized once; undo and redo only move its bytes. The
  // file must stay what serializing the whole stack gives.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    REQUIRE(d.doc->backupManager());
    const auto file = d.doc->backupManager()->commandFileName();

    auto check = [&](const char* when) {
      QFile f{file};
      REQUIRE(f.open(QIODevice::ReadOnly));
      INFO(when);
      CHECK(f.readAll() == score::marshall<DataStream>(d.stack()));
    };

    d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
    check("after a push");
    {
      CurveUi ui{d};
      ui.drag({0.5, 1.}, {{0.6, 0.7}});
      ui.drag({1., 0.}, {{1., 0.3}});
    }
    check("after drags");
    d.stack().undo();
    check("after an undo");
    d.stack().undo();
    d.stack().redo();
    check("after undo and redo");
    d.stack().setIndex(1);
    check("after jumping in the history");
    d.setPolyline({{0., 1.}, {1., 0.}});
    check("after a push that drops the redo stack");

    // Loaded without its signals, as a crash restore does: the backup starts
    // over from the stack at the next command.
    {
      CurveUi ui{d};
      ui.drag({1., 0.}, {{1., 0.5}});
    }
    d.stack().undo();
    const auto bytes = score::marshall<DataStream>(d.stack());
    d.setPolyline({{0., 0.2}, {1., 0.2}});
    d.setPolyline({{0., 0.3}, {1., 0.3}});
    {
      DataStream::Deserializer w{bytes};
      score::loadCommandStack(ctx.components, w, d.stack(), [](auto) { return true; });
    }
    d.stack().undo();
    check("after a reload of the stack");
    d.setPolyline({{0., 0.5}, {1., 0.5}});
    check("after a push on the reloaded stack");
  });
}

TEST_CASE(
    "The automation executor takes each new curve, and frees the old one off the audio thread",
    "[curve][execution]")
{
  // Execution builds the ossia curve on the GUI thread and swaps it into the
  // node from the audio thread; the previous one comes back in the command,
  // which the engine destroys on the GUI thread.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {1., 1.}});

    auto run_exec = [](Execution::DocumentPlugin& plug) {
      ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
      plug.runAllCommands();
      ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
    };
    auto& plug = d.context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, baseInterval(*d.doc));
    run_exec(plug);

    REQUIRE(plug.baseScenario());
    auto& procs = plug.baseScenario()->baseInterval().processes();
    auto it = procs.find(d.autom->id());
    REQUIRE(it != procs.end());
    auto node = std::dynamic_pointer_cast<ossia::nodes::automation>(it->second->node);
    REQUIRE(node);

    auto value_at = [&](double x) -> double {
      auto c = node->behavior().target<std::shared_ptr<ossia::curve_abstract>>();
      REQUIRE(c);
      auto curve = std::dynamic_pointer_cast<ossia::curve<double, float>>(*c);
      REQUIRE(curve);
      return curve->value_at(x);
    };
    CHECK(value_at(0.25) == Approx(0.25));
    const std::weak_ptr<ossia::curve_abstract> first
        = *node->behavior().target<std::shared_ptr<ossia::curve_abstract>>();

    d.setPolyline({{0., 1.}, {1., 0.}});

    // What the audio thread does with the queued commands: run them, then hand
    // them to the GUI thread (through the GC queue) to be destroyed.
    std::vector<Execution::ExecutionCommand> ran;
    Execution::ExecutionCommand cmd;
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    while(plug.context().executionQueue.try_dequeue(cmd))
    {
      cmd();
      ran.push_back(std::move(cmd));
    }
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

    CHECK(value_at(0.25) == Approx(0.75));
    CHECK(!first.expired());
    ran.clear();
    CHECK(first.expired());
  });
}

namespace
{
std::vector<float> wave(std::size_t n, double periods)
{
  std::vector<float> v(n);
  for(std::size_t i = 0; i < n; i++)
    v[i] = 0.5f + 0.45f * std::sin(periods * 2. * M_PI * i / double(n - 1));
  return v;
}

bool isSampled(const Curve::SegmentModel& s)
{
  return s.concreteKey() == Metadata<ConcreteKey_k, Curve::PointArraySegment>::get();
}

//! The value of `values`, evenly spaced over [0, 1], interpolated linearly.
double expectedAt(const std::vector<float>& values, double x)
{
  const double pos = x * (values.size() - 1);
  const auto i = std::min(std::size_t(pos), values.size() - 2);
  const double t = pos - i;
  return values[i] + t * (values[i + 1] - values[i]);
}
}

TEST_CASE("A million samples make one segment that evaluates as them", "[curve][sampled]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    const auto values = wave(1'000'000, 7.);
    auto segs = Curve::curveFromSamples(values);
    REQUIRE(segs.size() == 1);
    CommandDispatcher<>{d.context().commandStack}.submit(
        new Curve::UpdateCurve{d.curve(), std::move(segs)});
    settle();

    auto& curve = d.curve();
    REQUIRE(curve.segments().size() == 1);
    CHECK(curve.points().size() == 2);
    CHECK(isSampled(*curve.segments().begin()));
    CHECK(curveError(curve) == "");

    for(double x : {0., 1e-7, 0.123456, 0.5, 0.999999, 1.})
    {
      INFO("x = " << x);
      REQUIRE(curve.valueAt(x));
      CHECK(*curve.valueAt(x) == Approx(expectedAt(values, x)).margin(1e-6));
    }

    // The executor, scaled to the automation's range.
    d.autom->setMin(2.);
    d.autom->setMax(6.);
    auto exec = Engine::score_to_ossia::curve<double, float>(
        [](double x) { return x; }, [](double y) -> float { return y * 4. + 2.; },
        curve.sortedSegments(), {});
    for(double x : {1e-7, 0.123456, 0.5, 0.999999, 1.})
    {
      INFO("x = " << x);
      CHECK(exec->value_at(x) == Approx(2. + 4. * expectedAt(values, x)).margin(1e-4));
    }
  });
}

TEST_CASE("A few samples stay points that can be edited", "[curve][sampled]")
{
  const auto values = wave(100, 1.);
  const auto segs = Curve::curveFromSamples(values);
  REQUIRE(segs.size() == 99);
  CHECK(Curve::isValidCurve(segs));
  for(const auto& s : segs)
    CHECK(s.type == Metadata<ConcreteKey_k, Curve::LinearSegment>::get());
}

TEST_CASE("A sampled curve saves and reloads", "[curve][sampled][serialization]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    const auto values = wave(200'000, 3.);
    CommandDispatcher<>{d.context().commandStack}.submit(
        new Curve::UpdateCurve{d.curve(), Curve::curveFromSamples(values)});
    settle();

    const auto id = d.autom->id();
    const auto saved = d.curve().toCurveData().front().specificSegmentData.value<Curve::PointArraySegmentData>();
    for(auto [format, reloaded] :
        {std::pair{"binary", score::test::reload_via_bytes(ctx, *d.doc)},
         std::pair{"JSON", score::test::reload_via_json(ctx, *d.doc)}})
    {
      INFO(format);
      REQUIRE(reloaded);
      auto& procs = baseInterval(*reloaded).processes;
      auto it = procs.find(id);
      REQUIRE(it != procs.end());
      auto& curve = static_cast<Automation::ProcessModel&>(*it).curve();
      REQUIRE(curve.segments().size() == 1);
      const auto back = curve.toCurveData().front().specificSegmentData.value<Curve::PointArraySegmentData>();
      REQUIRE(back.points);
      CHECK(back.points->size() == saved.points->size());
      CHECK(back.min_x == saved.min_x);
      CHECK(back.max_x == saved.max_x);
      CHECK(back.min_y == saved.min_y);
      CHECK(back.max_y == saved.max_y);
      if(std::string_view{format} == "binary")
      {
        CHECK(back == saved);
      }
      else
      {
        // score parses JSON numbers in rapidjson's fast mode: within an ulp.
        auto a = back.points->begin();
        auto b = saved.points->begin();
        for(; a != back.points->end(); ++a, ++b)
        {
          REQUIRE(a->first == Approx(b->first).epsilon(1e-15));
          REQUIRE(a->second == Approx(b->second).epsilon(1e-15));
        }
      }
      for(int i = 0; i <= 100; i++)
        CHECK(*curve.valueAt(i / 100.) == Approx(*d.curve().valueAt(i / 100.)));
    }
  });
}

TEST_CASE("A sampled segment is drawn with at most four points per pixel", "[curve][sampled]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    CommandDispatcher<>{d.context().commandStack}.submit(
        new Curve::UpdateCurve{d.curve(), Curve::curveFromSamples(wave(1'000'000, 50.))});
    settle();

    CurveUi ui{d};
    REQUIRE(ui.presenter->segments().m_map.size() == 1);
    const auto& view = *ui.presenter->segments().begin();
    const auto& pts = view.model().data();
    CHECK(pts.size() >= 1000);
    // First, lowest, highest and last of each column.
    CHECK(pts.size() <= 4 * 1000 + 4);

    // The extremes of the wave are drawn.
    double lo = 1., hi = 0.;
    for(auto p : pts)
    {
      lo = std::min(lo, p.y());
      hi = std::max(hi, p.y());
    }
    CHECK(lo == Approx(0.05).margin(1e-3));
    CHECK(hi == Approx(0.95).margin(1e-3));
  });
}

TEST_CASE("Samples converted to editable points follow them", "[curve][sampled]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    const auto values = wave(100'000, 2.);
    CommandDispatcher<>{d.context().commandStack}.submit(
        new Curve::UpdateCurve{d.curve(), Curve::curveFromSamples(values)});
    settle();

    CurveUi ui{d};
    ui.presenter->convertSamplesToPoints();
    settle();

    auto& curve = d.curve();
    CHECK(curveError(curve) == "");
    const auto& set = score::AppContext().settings<Curve::Settings::Model>();
    const double tolerance = 1. / std::max(set.getSimplificationRatio(), 100);
    const auto n = curve.segments().size();
    INFO(n << " segments");
    CHECK(n > 10);
    CHECK(n < 2000);
    for(const auto& seg : curve.segments())
      CHECK(!isSampled(seg));
    double worst = 0.;
    for(int i = 0; i <= 1000; i++)
    {
      const double x = i / 1000.;
      worst = std::max(worst, std::abs(*curve.valueAt(x) - expectedAt(values, x)));
    }
    INFO("tolerance " << tolerance);
    CHECK(worst < 2 * tolerance);

    d.stack().undo();
    settle();
    REQUIRE(curve.segments().size() == 1);
    CHECK(isSampled(*curve.segments().begin()));
  });
}

TEST_CASE("Cutting into a sampled segment keeps its samples in place", "[curve][sampled]")
{
  // Adding a point inside it, or drawing over part of it, shortens it: the
  // samples that remain must stay where they were, not be squeezed.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CHECK(editSurvives([&]() -> std::string {
      CurveDoc d{ctx};
      const auto values = wave(50'000, 3.);
      CommandDispatcher<>{d.context().commandStack}.submit(
          new Curve::UpdateCurve{d.curve(), Curve::curveFromSamples(values)});
      settle();
      CurveUi ui{d};

      auto differs = [&](std::initializer_list<double> xs) -> std::string {
        for(double x : xs)
        {
          const auto v = d.curve().valueAt(x);
          if(!v || std::abs(*v - expectedAt(values, x)) > 1e-5)
            return "the samples moved, at x = " + std::to_string(x);
        }
        return {};
      };

      // A point in the middle: both halves keep their samples.
      ui.palette->createPoint(ui.toScene({0.4, expectedAt(values, 0.4)}));
      settle();
      if(d.curve().segments().size() != 2)
        return "the point did not split the segment";
      if(auto e = curveError(d.curve()); !e.empty())
        return e;
      if(auto e = differs({0.1, 0.25, 0.39, 0.41, 0.6, 0.9}); !e.empty())
        return e;

      // A pen stroke over [0.6, 0.7]: the rest stays.
      ui.settings().setTool(Curve::Tool::CreatePen);
      ui.press({0.6, 0.5});
      for(int i = 1; i <= 10; i++)
        ui.move({0.6 + i * 0.01, 0.5});
      ui.release({0.7, 0.5});
      if(auto e = curveError(d.curve()); !e.empty())
        return e;
      return differs({0.1, 0.25, 0.39, 0.41, 0.55, 0.75, 0.9});
    }));
  });
}

TEST_CASE("The min/max pyramid answers any range as a scan would", "[curve][rendering]")
{
  std::mt19937 rng{42};
  for(std::size_t n : {1u, 2u, 3u, 7u, 64u, 1000u, 4097u})
  {
    std::vector<float> ys(n);
    std::uniform_real_distribution<float> val{-5.f, 5.f};
    for(auto& y : ys)
      y = val(rng);
    Curve::MinMaxPyramid pyramid;
    pyramid.build(n, [&](std::size_t i) { return ys[i]; });

    std::uniform_int_distribution<std::size_t> idx{0, n - 1};
    for(int k = 0; k < 500; k++)
    {
      auto a = idx(rng), b = idx(rng);
      if(a > b)
        std::swap(a, b);
      b++;
      const auto [lo, hi] = pyramid.range(a, b);
      INFO("n " << n << " [" << a << ", " << b << ")");
      REQUIRE(lo == *std::min_element(ys.begin() + a, ys.begin() + b));
      REQUIRE(hi == *std::max_element(ys.begin() + a, ys.begin() + b));
    }
  }
}

TEST_CASE("An envelope keeps each column's extremes at a cost set by the columns", "[curve][rendering]")
{
  constexpr std::size_t n = 200'000;
  constexpr int columns = 300;
  std::vector<double> ys(n);
  std::mt19937 rng{7};
  std::uniform_real_distribution<double> val{0., 1.};
  for(auto& y : ys)
    y = val(rng);
  auto x = [](std::size_t i) { return double(i) / (n - 1); };
  Curve::MinMaxPyramid pyramid;
  pyramid.build(n, [&](std::size_t i) { return ys[i]; });

  std::vector<QPointF> line;
  Curve::envelope(
      n, x, [&](std::size_t i) { return ys[i]; }, pyramid, 0, 0., 1., columns, line);
  CHECK(line.size() <= 4 * columns + 4);

  // Every column reaches the lowest and the highest of its points.
  for(int c = 0; c < columns; c++)
  {
    const double a = double(c) / columns, b = double(c + 1) / columns;
    double lo = 1., hi = 0.;
    for(std::size_t i = std::size_t(std::ceil(a * (n - 1))); i < n && x(i) < b; i++)
    {
      lo = std::min(lo, ys[i]);
      hi = std::max(hi, ys[i]);
    }
    double drawn_lo = 1., drawn_hi = 0.;
    for(auto p : line)
      if(p.x() > a && p.x() < b)
      {
        drawn_lo = std::min(drawn_lo, p.y());
        drawn_hi = std::max(drawn_hi, p.y());
      }
    INFO("column " << c);
    CHECK(drawn_lo == Approx(lo).margin(1e-6));
    CHECK(drawn_hi == Approx(hi).margin(1e-6));
  }
}

namespace
{
//! Whether something is drawn in rows [top, bottom) of column x.
bool drawnIn(const QImage& img, int x, int top, int bottom, QRgb background)
{
  for(int y = top; y < bottom; y++)
    if(img.pixel(x, y) != background)
      return true;
  return false;
}

QImage render(QGraphicsScene& scene, QRectF curve_rect)
{
  // Taller than the curve: the rows below it show the background.
  QGraphicsView gv{&scene};
  gv.setFrameStyle(QFrame::NoFrame);
  gv.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  gv.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  gv.setSceneRect(0., 0., curve_rect.width(), curve_rect.height() + 50.);
  gv.resize(int(curve_rect.width()), int(curve_rect.height()) + 50);
  gv.show();
  settle();
  return gv.viewport()->grab().toImage();
}
}

TEST_CASE(
    "A dense curve is drawn as its envelope, not as an average",
    "[curve][rendering]")
{
  // 100 000 points alternating between 0 and 1: each column spans the whole
  // height. Averaging them draws a flat line in the middle.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    Points pts;
    for(int i = 0; i <= 100'000; i++)
      pts.push_back({i / 100'000., double(i % 2)});
    d.setPolyline(pts);
    CurveUi ui{d};

    const auto img = render(ui.scene, ui.rect);
    const auto background = img.pixel(10, img.height() - 5);
    for(int x : {100, 500, 900})
    {
      INFO("column " << x);
      CHECK(drawnIn(img, x, 0, 10, background));
      CHECK(drawnIn(img, x, 190, 200, background));
    }
  });
}

TEST_CASE("A sampled segment is drawn as the envelope of its samples", "[curve][rendering]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    // 5000 periods over the million samples: 5 per pixel column.
    CommandDispatcher<>{d.context().commandStack}.submit(
        new Curve::UpdateCurve{d.curve(), Curve::curveFromSamples(wave(1'000'000, 5000.))});
    settle();
    CurveUi ui{d};

    const auto img = render(ui.scene, ui.rect);
    const auto background = img.pixel(10, img.height() - 5);
    for(int x : {100, 500, 900})
    {
      INFO("column " << x);
      // The wave spans [0.05, 0.95] of the height.
      CHECK(drawnIn(img, x, 5, 20, background));
      CHECK(drawnIn(img, x, 180, 195, background));
      CHECK(!drawnIn(img, x, 0, 4, background));
    }
  });
}

namespace
{
//! Every point and segment view where its model says, as setRect lays them out.
std::string viewsError(const CurveUi& ui)
{
  std::ostringstream err;
  const auto w = ui.rect.width();
  for(const auto& view : ui.presenter->points())
  {
    const auto expected = ui.toScene(view.model().pos());
    if(std::abs(view.pos().x() - expected.x()) > 1e-6
       || std::abs(view.pos().y() - expected.y()) > 1e-6)
      err << "point view at " << view.pos().x() << "," << view.pos().y() << " for "
          << expected.x() << "," << expected.y() << "; ";
  }
  for(const auto& view : ui.presenter->segments())
  {
    const auto& m = view.model();
    const double x0 = m.start().x() * w, x1 = m.end().x() * w;
    if(std::abs(view.pos().x() - x0) > 1e-6
       || std::abs(view.boundingRect().width() - (x1 - x0)) > 1e-6)
      err << "segment view over [" << view.pos().x() << ", "
          << view.pos().x() + view.boundingRect().width() << "] for [" << x0 << ", "
          << x1 << "]; ";
  }
  return err.str();
}

void selectPointAt(const Curve::Model& m, double x)
{
  for(auto p : m.points())
    if(std::abs(p->pos().x() - x) < 1e-9)
      const_cast<Curve::PointModel*>(p)->selection.set(true);
}
}

TEST_CASE("Views follow the curve after points are removed", "[curve][edition][views]")
{
  // Removing points drops views; the points that remain keep theirs, and
  // moving them must move those views, not the dropped ones.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CHECK(editSurvives([&]() -> std::string {
      CurveDoc d{ctx};
      d.setPolyline({{0., 0.}, {0.2, 1.}, {0.4, 0.}, {0.6, 1.}, {0.8, 0.}, {1., 1.}});
      CurveUi ui{d};

      // Not neighbours: a point whose two segments both go goes with them.
      selectPointAt(d.curve(), 0.2);
      selectPointAt(d.curve(), 0.8);
      ui.presenter->removeSelection();
      settle();
      if(d.curve().points().size() != 4)
        return "expected 4 points, got " + std::to_string(d.curve().points().size());
      if(auto e = viewsError(ui); !e.empty())
        return "after the removal: " + e;

      std::vector<QPointF> positions;
      for(auto p : d.curve().points())
        positions.push_back(p->pos());
      for(auto pos : positions)
      {
        const QPointF to{pos.x(), pos.y() > 0.5 ? 0.3 : 0.7};
        ui.drag(pos, {to});
        if(auto e = viewsError(ui); !e.empty())
          return "after moving the point at " + std::to_string(pos.x()) + ": " + e;
      }
      return curveError(d.curve());
    }));
  });
}

TEST_CASE("Segment views follow a horizontal drag", "[curve][edition][views]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
    CurveUi ui{d};

    ui.drag({0.5, 1.}, {{0.55, 1.}, {0.6, 1.}});
    REQUIRE(pointAt(d.curve(), {0.6, 1.}));
    CHECK(viewsError(ui) == "");

    d.stack().undo();
    settle();
    REQUIRE(pointAt(d.curve(), {0.5, 1.}));
    CHECK(viewsError(ui) == "");
  });
}

TEST_CASE("A moved segment is redrawn once per edit", "[curve][edition][views]")
{
  // The middle segment has both its ends moved by one command.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.3, 1.}, {0.6, 0.}, {1., 1.}});
    std::map<int, int> redraws;
    for(auto& s : d.curve().segments())
      QObject::connect(
          &s, &Curve::SegmentModel::dataChanged, &s,
          [&redraws, id = s.id().val()] { redraws[id]++; });

    auto segs = d.curve().toCurveData();
    for(auto& s : segs)
    {
      if(s.start.x() == 0.3)
        s.start = {0.3, 0.5};
      if(s.end.x() == 0.3)
        s.end = {0.3, 0.5};
      if(s.start.x() == 0.6)
        s.start = {0.6, 0.5};
      if(s.end.x() == 0.6)
        s.end = {0.6, 0.5};
    }
    CommandDispatcher<>{d.context().commandStack}.submit(
        new Curve::UpdateCurve{d.curve(), segs});
    settle();
    REQUIRE(redraws.size() == 3);
    for(auto [id, n] : redraws)
    {
      INFO("segment " << id);
      CHECK(n == 1);
    }
  });
}

TEST_CASE("Deleting a point of the second chain keeps the curve", "[curve][edition]")
{
  // A curve of two chains: [0, 0.5] and [0.6, 1].
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    std::vector<Curve::SegmentData> segs;
    auto seg = [&](int id, QPointF a, QPointF b, int prev, int next) {
      Curve::SegmentData s;
      s.id = Id<Curve::SegmentModel>{id};
      s.start = a;
      s.end = b;
      if(prev)
        s.previous = Id<Curve::SegmentModel>{prev};
      if(next)
        s.following = Id<Curve::SegmentModel>{next};
      s.type = Metadata<ConcreteKey_k, Curve::PowerSegment>::get();
      s.specificSegmentData = QVariant::fromValue(Curve::PowerSegmentData{});
      segs.push_back(s);
    };
    seg(1, {0., 0.}, {0.3, 1.}, 0, 2);
    seg(2, {0.3, 1.}, {0.5, 0.}, 1, 0);
    seg(3, {0.6, 0.}, {0.8, 1.}, 0, 4);
    seg(4, {0.8, 1.}, {1., 0.}, 3, 0);
    REQUIRE(Curve::isValidCurve(segs));
    CommandDispatcher<>{d.context().commandStack}.submit(
        new Curve::UpdateCurve{d.curve(), segs});
    settle();
    CurveUi ui{d};

    selectPointAt(d.curve(), 0.8);
    ui.presenter->removeSelection();
    settle();
    CHECK(!hasPointAtX(d.curve(), 0.8));
    CHECK(hasPointAtX(d.curve(), 0.3));
    CHECK(curveError(d.curve()) == "");
    CHECK(Curve::isValidCurve(d.curve().toCurveData()));
  });
}

TEST_CASE("An UpdateCurve with a duplicate id is refused", "[curve][edition]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
    const auto before = d.curve().toCurveData();

    // The first segment twice, and not the second: the counts match.
    auto next = before;
    next[1].id = next[0].id;
    next[1].previous = std::nullopt;
    next[0].following = std::nullopt;
    Curve::UpdateCurve cmd{d.curve(), next};
    CHECK(cmd.changes().empty());
    cmd.redo(d.context());
    settle();
    CHECK(sameCurve(d.curve().toCurveData(), before));
  });
}

TEST_CASE("Segments are listed in chain order, vertical steps included", "[curve][model]")
{
  // A staircase: each vertical step starts where the segment after it does,
  // so a sort by x may put it after that segment.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    Points pts;
    constexpr int steps = 100;
    for(int i = 0; i < steps; i++)
    {
      pts.push_back({double(i) / steps, double(i % 2)});
      pts.push_back({double(i) / steps, double((i + 1) % 2)});
    }
    pts.push_back({1., double(steps % 2)});
    d.setPolyline(pts);
    const auto ordered = Curve::orderedSegments(d.curve());
    REQUIRE(ordered.size() == pts.size() - 1);
    for(std::size_t i = 0; i + 1 < ordered.size(); i++)
    {
      INFO(i);
      REQUIRE(ordered[i].following == ordered[i + 1].id);
    }
  });
}

TEST_CASE("A waveform column spans the line across its edges", "[curve][rendering]")
{
  // Two points far apart: each column between them covers the part of the
  // line inside it, no more.
  std::vector<double> xs{0., 4.}, ys{0., 1.};
  Curve::MinMaxPyramid pyramid;
  pyramid.build(2, [&](std::size_t i) { return ys[i]; });
  std::vector<QLineF> lines;
  Curve::envelopeColumns(
      2, [&](std::size_t i) { return xs[i]; }, [&](std::size_t i) { return ys[i]; },
      pyramid, 0, 0., 1., 4, lines);
  REQUIRE(lines.size() == 4);
  for(int c = 0; c < 4; c++)
  {
    INFO("column " << c);
    CHECK(std::min(lines[c].y1(), lines[c].y2()) == Approx(c / 4.));
    CHECK(std::max(lines[c].y1(), lines[c].y2()) == Approx((c + 1) / 4.));
  }

  // A ramp, then dense points at its top: no full-height spike where they begin.
  xs = {0., 100.};
  ys = {0., 1.};
  for(int i = 1; i <= 50; i++)
  {
    xs.push_back(100. + i * 0.1);
    ys.push_back(1.);
  }
  pyramid.build(xs.size(), [&](std::size_t i) { return ys[i]; });
  Curve::envelopeColumns(
      xs.size(), [&](std::size_t i) { return xs[i]; },
      [&](std::size_t i) { return ys[i]; }, pyramid, 0, 95., 1., 10, lines);
  for(const auto& l : lines)
  {
    INFO("column at " << l.x1());
    CHECK(std::min(l.y1(), l.y2()) > 0.9);
  }
  // Outside the points: nothing.
  Curve::envelopeColumns(
      2, [&](std::size_t i) { return xs[i]; }, [&](std::size_t i) { return ys[i]; },
      pyramid, 0, -10., 1., 5, lines);
  CHECK(lines.empty());
}

TEST_CASE("A sampled segment drawn while it grows reflects its samples", "[curve][sampled][rendering]")
{
  // As recording does: a sample, drawn, then many more added in place.
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Curve::PointArraySegment seg{Id<Curve::SegmentModel>{1}, nullptr};
    seg.setStart({0., 0.});
    seg.setEnd({1., 0.});
    seg.setMinY(0.);
    seg.setMaxY(1.);
    seg.addPoint(0., 0.5);
    std::vector<QLineF> lines;
    seg.envelopeColumns(0., 0.1, 10, lines);

    for(int i = 1; i <= 3000; i++)
      seg.addPoint(i, (i % 100) / 99.);
    seg.envelopeColumns(seg.start().x(), (seg.end().x() - seg.start().x()) / 10., 10, lines);
    REQUIRE(lines.size() == 10);
    double lo = 1., hi = 0.;
    for(const auto& l : lines)
    {
      lo = std::min({lo, l.y1(), l.y2()});
      hi = std::max({hi, l.y1(), l.y2()});
    }
    CHECK(lo == Approx(0.).margin(1e-6));
    CHECK(hi == Approx(1.).margin(1e-6));
  });
}

TEST_CASE("A partial repaint of a sampled segment draws what the full one does", "[curve][sampled][rendering]")
{
  // Dense on the left, sparse on the right: the right part alone has fewer
  // samples than pixels, the segment as a whole has more.
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    std::vector<float> values;
    for(int i = 0; i < 40000; i++)
      values.push_back(0.5f + 0.4f * std::sin(i * 0.05f));
    auto data = Curve::curveFromSamples(values, 0).front();
    auto dat = data.specificSegmentData.value<Curve::PointArraySegmentData>();
    auto pts = std::make_shared<Curve::PointArraySamples>(*dat.points);
    for(int i = 1; i < 200; i++)
      pts->emplace(40000. + i * 200., 0.5 + 0.4 * std::sin(i * 0.3));
    dat.max_x = pts->rbegin()->first;
    dat.points = pts;
    data.specificSegmentData = QVariant::fromValue(dat);
    Curve::PointArraySegment seg{data, nullptr};

    TestStyle colors;
    Curve::SegmentView view{&seg, colors.style, nullptr};
    view.setRect({0., 0., 1000., 200.});

    auto paint = [&](QRectF exposed) {
      QImage img{1000, 200, QImage::Format_ARGB32};
      img.fill(Qt::black);
      QPainter p{&img};
      QStyleOptionGraphicsItem opt;
      opt.exposedRect = exposed;
      p.setClipRect(exposed);
      view.paint(&p, &opt, nullptr);
      return img;
    };
    const auto full = paint({0., 0., 1000., 200.});
    const QRectF part{900., 0., 100., 200.};
    const auto partial = paint(part);
    int differ = 0;
    for(int x = 900; x < 1000; x++)
      for(int y = 0; y < 200; y++)
        differ += full.pixel(x, y) != partial.pixel(x, y);
    CHECK(differ == 0);
  });
}

TEST_CASE("A sampled segment's hit-test shape stays small", "[curve][sampled][performance]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    std::vector<float> values;
    for(int i = 0; i < 200000; i++)
      values.push_back(float(i % 7) / 6.f);
    auto data = Curve::curveFromSamples(values).front();
    Curve::PointArraySegment seg{data, nullptr};
    TestStyle colors;
    Curve::SegmentView view{&seg, colors.style, nullptr};
    view.setRect({0., 0., 8000., 200.});
    CHECK(view.shape().elementCount() < 20000);
    CHECK(view.contains({4000., 100.}));
  });
}

TEST_CASE("Sampled data without its range or with a corrupt count loads", "[curve][sampled][serialization]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    // Written before the range was saved: the defaults.
    rapidjson::Document doc;
    doc.Parse(R"({"Points": [0, 0.5, 1, "x", 2, 1]})");
    REQUIRE(!doc.HasParseError());
    auto json = JSONWriter::unmarshall<Curve::PointArraySegmentData>(doc);
    CHECK(json.min_x == 0.);
    CHECK(json.max_x == 1.);
    CHECK(json.min_y == 0.);
    CHECK(json.max_y == 1.);
    REQUIRE(json.points);
    CHECK(json.points->size() == 2);

    // A count far beyond what the stream holds, and a non-finite sample.
    QByteArray bytes;
    {
      QDataStream s{&bytes, QIODevice::WriteOnly};
      s << 0. << 1. << 0. << 1. << qint64(1'000'000'000'000) << 0. << 0.5
        << 1. << std::numeric_limits<double>::quiet_NaN();
    }
    auto bin = DataStreamWriter::unmarshall<Curve::PointArraySegmentData>(bytes);
    REQUIRE(bin.points);
    CHECK(bin.points->size() == 1);
  });
}

TEST_CASE("Non-finite samples are left out of an imported curve", "[curve][sampled]")
{
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float inf = std::numeric_limits<float>::infinity();

  CHECK(Curve::curveFromSamples(std::vector<float>{nan, nan}).empty());

  auto one = Curve::curveFromSamples(std::vector<float>{nan, 0.3f, inf});
  REQUIRE(one.size() == 1);
  CHECK(one[0].start.y() == Approx(0.3));

  // Editable: each value keeps its place.
  auto segs = Curve::curveFromSamples(std::vector<float>{nan, 0.f, 0.5f, nan, 1.f});
  REQUIRE(segs.size() == 2);
  CHECK(Curve::isValidCurve(segs));
  CHECK(segs[0].start.x() == Approx(0.25));
  CHECK(segs[1].start.x() == Approx(0.5));
  CHECK(segs[1].end.x() == Approx(1.));

  // Sampled.
  std::vector<float> many(20000, 0.5f);
  many[0] = nan;
  many[10] = inf;
  auto sampled = Curve::curveFromSamples(many);
  REQUIRE(sampled.size() == 1);
  const auto dat = sampled[0].specificSegmentData.value<Curve::PointArraySegmentData>();
  REQUIRE(dat.points);
  CHECK(dat.points->size() == many.size() - 2);
  for(auto [x, y] : *dat.points)
    REQUIRE(ossia::safe_isfinite(y));
  CHECK(Curve::isValidCurve(sampled));
}

TEST_CASE("A crash backup written by an earlier version restores an UpdateCurve", "[curve][edition][backup]")
{
  // Earlier versions saved the whole curve before and after the command.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
    const auto before = d.curve().toCurveData();
    auto after = before;
    after[0].end = {0.4, 0.2};
    after[1].start = {0.4, 0.2};
    // And one segment replaced by two.
    Curve::SegmentData extra = after[1];
    extra.id = Id<Curve::SegmentModel>{999};
    extra.start = {0.7, 0.5};
    extra.previous = after[1].id;
    extra.following = std::nullopt;
    after[1].end = {0.7, 0.5};
    after[1].following = extra.id;
    after.push_back(extra);
    REQUIRE(Curve::isValidCurve(after));

    QByteArray bytes;
    {
      DataStreamReader r{&bytes};
      r.m_stream << score::IDocument::path(d.curve()) << before << after;
    }
    Curve::UpdateCurve cmd;
    cmd.deserialize(bytes);
    cmd.redo(d.context());
    settle();
    CHECK(sameCurve(d.curve().toCurveData(), after));
    cmd.undo(d.context());
    settle();
    CHECK(sameCurve(d.curve().toCurveData(), before));

    // And the current format round-trips.
    Curve::UpdateCurve again;
    again.deserialize(cmd.serialize());
    again.redo(d.context());
    settle();
    CHECK(sameCurve(d.curve().toCurveData(), after));

    // Back, which removes the added segment.
    QByteArray back;
    {
      DataStreamReader r{&back};
      r.m_stream << score::IDocument::path(d.curve()) << after << before;
    }
    Curve::UpdateCurve undoing;
    undoing.deserialize(back);
    undoing.redo(d.context());
    settle();
    CHECK(sameCurve(d.curve().toCurveData(), before));
  });
}

TEST_CASE("A state message moving an end of the curve notifies it", "[curve][automation]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
    int changes = 0;
    QObject::connect(&d.curve(), &Curve::Model::changed, [&] { changes++; });

    Automation::ProcessState start{*d.autom, 0., nullptr};
    ::State::Message m;
    m.address = d.autom->address();
    m.value = 0.7f;
    start.setMessages({m}, Process::MessageNode{});
    CHECK(changes == 1);
    CHECK(d.curve().sortedSegments().front()->start().y() == Approx(0.7));

    Automation::ProcessState end{*d.autom, 1., nullptr};
    m.value = 0.2f;
    end.setMessages({m}, Process::MessageNode{});
    CHECK(changes == 2);
    CHECK(d.curve().sortedSegments().back()->end().y() == Approx(0.2));
  });
}

namespace
{
Curve::SegmentData segment(
    int id, QPointF a, QPointF b, int prev = 0, int next = 0,
    std::optional<UuidKey<Curve::SegmentFactory>> type = {})
{
  Curve::SegmentData s;
  s.id = Id<Curve::SegmentModel>{id};
  s.start = a;
  s.end = b;
  if(prev)
    s.previous = Id<Curve::SegmentModel>{prev};
  if(next)
    s.following = Id<Curve::SegmentModel>{next};
  s.type = type ? *type : Metadata<ConcreteKey_k, Curve::PowerSegment>::get();
  if(s.type == Metadata<ConcreteKey_k, Curve::LinearSegment>::get())
    s.specificSegmentData = QVariant::fromValue(Curve::LinearSegmentData{});
  else
    s.specificSegmentData = QVariant::fromValue(Curve::PowerSegmentData{});
  return s;
}

void submit(const CurveDoc& d, const std::vector<Curve::SegmentData>& segs)
{
  CommandDispatcher<>{d.context().commandStack}.submit(
      new Curve::UpdateCurve{d.curve(), segs});
  settle();
}
}

TEST_CASE("A curve is validated before it replaces the current one", "[curve][model]")
{
  using Curve::isValidCurve;
  CHECK(isValidCurve({}));
  CHECK(isValidCurve(std::vector{segment(1, {0, 0}, {1, 1})}));
  const double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK(!isValidCurve(std::vector{segment(1, {0, nan}, {1, 1})}));
  CHECK(!isValidCurve(std::vector{segment(1, {0.5, 0}, {0.2, 1})}));
  // Duplicate ids.
  CHECK(!isValidCurve(std::vector{segment(1, {0, 0}, {0.5, 1}), segment(1, {0.5, 1}, {1, 0})}));
  // A link to a missing segment, and a link that is not returned.
  CHECK(!isValidCurve(std::vector{segment(1, {0, 0}, {0.5, 1}, 0, 7)}));
  CHECK(!isValidCurve(std::vector{segment(1, {0, 0}, {0.5, 1}, 0, 2), segment(2, {0.5, 1}, {1, 0})}));
  CHECK(!isValidCurve(std::vector{segment(1, {0, 0}, {0.5, 1}), segment(2, {0.5, 1}, {1, 0}, 1)}));
  // A cycle.
  CHECK(!isValidCurve(
      std::vector{segment(1, {0, 0}, {0.5, 1}, 2, 2), segment(2, {0.5, 1}, {1, 0}, 1, 1)}));

  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    auto& curve = d.curve();
    const std::vector good{
        segment(1, {0, 0}, {0.5, 1}, 0, 2), segment(2, {0.5, 1}, {1, 0}, 1, 0)};
    curve.fromCurveData(good);
    CHECK(sameCurve(curve.toCurveData(), good));

    // Refused: the curve stays.
    curve.fromCurveData(std::vector{segment(3, {0.5, 0}, {0.2, 1})});
    CHECK(sameCurve(curve.toCurveData(), good));
    const auto unknown = UuidKey<Curve::SegmentFactory>::fromString(
        QStringLiteral("00000000-1111-2222-3333-444444444444"));
    curve.fromCurveData(std::vector{segment(3, {0, 0}, {1, 1}, 0, 0, unknown)});
    CHECK(sameCurve(curve.toCurveData(), good));

    // The same id with another type: replaced.
    const auto linear = Metadata<ConcreteKey_k, Curve::LinearSegment>::get();
    const std::vector other{
        segment(1, {0, 0}, {0.5, 1}, 0, 2, linear), segment(2, {0.5, 1}, {1, 0}, 1, 0)};
    curve.fromCurveData(other);
    CHECK(sameCurve(curve.toCurveData(), other));
    CHECK(curve.segments().at(Id<Curve::SegmentModel>{1}).concreteKey() == linear);

    // Through UpdateCurve, an unknown type is left out with a warning.
    Curve::UpdateCurve cmd{curve, std::vector{segment(5, {0, 0}, {1, 1}, 0, 0, unknown)}};
    cmd.redo(d.context());
    settle();
    CHECK(curve.segments().find(Id<Curve::SegmentModel>{5}) == curve.segments().end());
    cmd.undo(d.context());
    settle();
    CHECK(sameCurve(curve.toCurveData(), other));
  });
}

TEST_CASE("The order of the segments follows their links through every edit", "[curve][model]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    auto& curve = d.curve();
    auto ids = [&] {
      std::vector<int> r;
      for(auto s : curve.sortedSegments())
        r.push_back(s->id().val());
      return r;
    };
    submit(d, {segment(1, {0, 0}, {0.5, 1}, 0, 2), segment(2, {0.5, 1}, {1, 0}, 1, 0)});
    CHECK(ids() == std::vector{1, 2});

    // The two segments trade places.
    submit(d, {segment(2, {0, 0}, {0.5, 1}, 0, 1), segment(1, {0.5, 1}, {1, 0}, 2, 0)});
    CHECK(ids() == std::vector{2, 1});
    CHECK(curveError(curve) == "");

    // A vertical step added where a segment starts: before it, not after.
    submit(
        d, {segment(2, {0, 0}, {0.5, 1}, 0, 3), segment(3, {0.5, 1}, {0.5, 0}, 2, 1),
            segment(1, {0.5, 0}, {1, 0}, 3, 0)});
    CHECK(ids() == std::vector{2, 3, 1});
    CHECK(curveError(curve) == "");

    // Two chains, then links that no longer form chains: ordered by x.
    Curve::SegmentData a = segment(10, {0, 0}, {0.4, 1}, 0, 11);
    Curve::SegmentData b = segment(11, {0.4, 1}, {1, 0}, 0, 0);
    std::vector<Id<Curve::SegmentModel>> removed;
    for(auto& s : curve.segments())
      removed.push_back(s.id());
    const Curve::SegmentData* up[]{&b, &a};
    curve.applyChanges(removed, up);
    CHECK(ids() == std::vector{10, 11});

    CHECK(curve.lastPointPos() == Approx(1.));
    CHECK(curve.valueAt(-1.) == std::nullopt);
    CHECK(curve.valueAt(2.) == std::nullopt);
    curve.clear();
    settle();
    CHECK(curve.segments().size() == 0);
    CHECK(curve.lastPointPos() == 0.);
    CHECK(curve.valueAt(0.5) == std::nullopt);
  });
}

TEST_CASE("Saved curves with broken links are repaired on load", "[curve][model][serialization]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    auto& curve = d.curve();
    curve.clear();
    settle();

    auto a = new Curve::PowerSegment{Id<Curve::SegmentModel>{1}, nullptr};
    a->setStart({0.5, 1.});
    a->setEnd({1., 0.});
    a->setPrevious(Id<Curve::SegmentModel>{42});
    auto b = new Curve::PowerSegment{Id<Curve::SegmentModel>{2}, nullptr};
    b->setStart({0., 0.});
    b->setEnd({0.5, 1.});
    b->setFollowing(Id<Curve::SegmentModel>{1});
    curve.loadSegments({a, b});
    CHECK(curveError(curve) == "");
    REQUIRE(curve.sortedSegments().size() == 2);
    CHECK(curve.sortedSegments().front()->id().val() == 2);
    CHECK(*curve.sortedSegments().front()->following() == Id<Curve::SegmentModel>{1});
  });
}

TEST_CASE("Selection is given and read back by the curve", "[curve][model]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
    auto& curve = d.curve();
    Selection sel;
    const auto& seg = *curve.sortedSegments().front();
    sel.append(&seg);
    sel.append(curve.points()[1]);
    curve.setSelection(sel);
    CHECK(seg.selection.get());
    CHECK(curve.points()[1]->selection.get());
    CHECK(!curve.points()[0]->selection.get());
    CHECK(curve.selectedChildren().size() == 2);
    curve.setSelection({});
    CHECK(curve.selectedChildren().size() == 0);
  });
}

TEST_CASE("Deleting from the editor removes points and fills the hole", "[curve][edition]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.25, 1.}, {0.5, 0.}, {0.75, 1.}, {1., 0.}});
    auto& editor = curveEditor(ctx);

    // No focused process: nothing to delete from.
    Selection first;
    first.append(d.curve().points()[1]);
    CHECK(!editor.remove(first, d.context()));
    // The event loop may move the focus: set before each call.
    auto focus = [&] { d.doc->focusManager().set(QPointer<Automation::ProcessModel>{d.autom}); };

    // Not a curve element: not for this editor.
    Selection other;
    other.append(d.autom);
    focus();
    CHECK(!editor.remove(other, d.context()));

    // An end point, a segment, and nothing to delete.
    Selection ends;
    ends.append(d.curve().points().front());
    ends.append(d.curve().sortedSegments().front());
    focus();
    CHECK(editor.remove(ends, d.context()));
    settle();
    CHECK(d.curve().points().size() == 5);

    Selection mid;
    mid.append(d.curve().points()[2]);
    focus();
    CHECK(editor.remove(mid, d.context()));
    settle();
    CHECK(!hasPointAtX(d.curve(), 0.5));
    CHECK(curveError(d.curve()) == "");

    // Everything but the ends: one segment across.
    Selection all;
    for(auto p : d.curve().points())
      all.append(p);
    focus();
    CHECK(editor.remove(all, d.context()));
    settle();
    CHECK(curveError(d.curve()) == "");
  });
}

TEST_CASE("Removing segments fills or leaves holes", "[curve][edition]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    // Out of x order in the model's chain list: two chains, the later first.
    submit(
        d, {segment(1, {0.6, 0}, {1, 1}, 0, 0), segment(2, {0, 0}, {0.3, 1}, 0, 3),
            segment(3, {0.3, 1}, {0.5, 0}, 2, 0)});
    auto& curve = d.curve();

    ossia::hash_set<int32_t> removed{3};
    auto kept = Curve::removeSegments(curve, removed, false);
    CHECK(kept.size() == 2);
    CHECK(Curve::isValidCurve(kept));

    auto filled = Curve::removeSegments(curve, removed, true);
    CHECK(Curve::isValidCurve(filled));
    CHECK(filled.size() == 3);

    ossia::hash_set<int32_t> all{1, 2, 3};
    auto none = Curve::removeSegments(curve, all, true);
    REQUIRE(none.size() == 1);
    CHECK(none.front().start.x() == 0.);
    CHECK(none.front().end.x() == 1.);
  });
}

TEST_CASE("A point is created on a point, in a gap, or past the curve", "[curve][edition]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    // On an existing point: that point moves.
    std::vector segs{segment(1, {0, 0}, {0.5, 1}, 0, 2), segment(2, {0.5, 1}, {1, 0}, 1, 0)};
    Curve::createPointAt(segs, {0.5, 0.3});
    CHECK(segs.size() == 2);
    CHECK(segs[0].end == QPointF(0.5, 0.3));
    CHECK(segs[1].start == QPointF(0.5, 0.3));

    // In a gap between two chains: joined to both.
    std::vector gap{segment(1, {0, 0}, {0.3, 1}), segment(2, {0.7, 1}, {1, 0})};
    Curve::createPointAt(gap, {0.5, 0.5});
    CHECK(gap.size() == 4);
    CHECK(Curve::isValidCurve(gap));

    // In an empty curve: two segments to the edges.
    std::vector<Curve::SegmentData> empty;
    Curve::createPointAt(empty, {0.4, 0.5});
    CHECK(empty.size() == 2);
    CHECK(Curve::isValidCurve(empty));

    // Past the end in a full view: only the segment that reaches it.
    std::vector past{segment(1, {0, 0}, {1, 1})};
    Curve::createPointAt(past, {1.5, 0.5});
    CHECK(Curve::isValidCurve(past));
    CHECK(past.back().end == QPointF(1.5, 0.5));
  });
}

namespace
{
//! A sampled segment over [a, b] of the curve, with `n` samples of a wave.
Curve::SegmentData sampledSegment(int id, double a, double b, int n, int prev = 0, int next = 0)
{
  std::vector<float> values;
  for(int i = 0; i < n; i++)
    values.push_back(0.5f + 0.4f * std::sin(i * 0.1f));
  auto d = Curve::curveFromSamples(values, 0).front();
  d.id = Id<Curve::SegmentModel>{id};
  d.start = {a, d.start.y()};
  d.end = {b, d.end.y()};
  if(prev)
    d.previous = Id<Curve::SegmentModel>{prev};
  if(next)
    d.following = Id<Curve::SegmentModel>{next};
  return d;
}

void sendMouse(QGraphicsScene& scene, QGraphicsItem& item, QEvent::Type t, QPointF pos)
{
  QGraphicsSceneMouseEvent ev{t};
  ev.setButton(t == QEvent::GraphicsSceneMouseMove ? Qt::NoButton : Qt::LeftButton);
  ev.setButtons(t == QEvent::GraphicsSceneMouseRelease ? Qt::NoButton : Qt::LeftButton);
  ev.setScenePos(pos);
  ev.setPos(item.mapFromScene(pos));
  scene.sendEvent(&item, &ev);
  settle();
}

//! A colour no skin draws curves with.
constexpr QRgb unpainted = qRgb(1, 254, 3);

QImage paintItem(QGraphicsItem& item, QSize size, QRectF exposed, QTransform t = {})
{
  QImage img{size, QImage::Format_ARGB32};
  img.fill(unpainted);
  QPainter p{&img};
  p.setTransform(t);
  QStyleOptionGraphicsItem opt;
  opt.exposedRect = exposed;
  item.paint(&p, &opt, nullptr);
  return img;
}

int litPixels(const QImage& img)
{
  int n = 0;
  for(int y = 0; y < img.height(); y++)
    for(int x = 0; x < img.width(); x++)
      n += img.pixel(x, y) != unpainted;
  return n;
}
}

TEST_CASE("Samples are converted to points from the context menu", "[curve][sampled][edition]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    // Points, then two sampled segments.
    auto s2 = sampledSegment(2, 0.2, 0.6, 3000, 1, 3);
    auto s3 = sampledSegment(3, 0.6, 1., 3000, 2, 0);
    s3.start = s2.end;
    submit(d, {segment(1, {0, 0}, s2.start, 0, 2), s2, s3});
    CurveUi ui{d};
    auto sampledCount = [&] {
      return ossia::count_if(d.curve().segments(), [](const auto& s) { return isSampled(s); });
    };
    REQUIRE(sampledCount() == 2);

    QMenu menu;
    ui.presenter->fillContextMenu(menu, {}, {});
    QAction* convert{};
    for(auto a : menu.actions())
      if(a->text() == QObject::tr("Convert samples to editable points"))
        convert = a;
    REQUIRE(convert);

    // Only the selected one.
    const_cast<Curve::SegmentModel&>(d.curve().segments().at(Id<Curve::SegmentModel>{3}))
        .selection.set(true);
    convert->trigger();
    settle();
    CHECK(sampledCount() == 1);
    CHECK(curveError(d.curve()) == "");
    CHECK(Curve::isValidCurve(d.curve().toCurveData()));

    // Then the rest.
    ui.presenter->convertSamplesToPoints();
    settle();
    CHECK(sampledCount() == 0);
    CHECK(curveError(d.curve()) == "");

    // Nothing left to convert: no menu entry, no command.
    QMenu none;
    ui.presenter->fillContextMenu(none, {}, {});
    for(auto a : none.actions())
      CHECK(a->text() != QObject::tr("Convert samples to editable points"));
    const auto commands = d.stack().size();
    ui.presenter->convertSamplesToPoints();
    CHECK(d.stack().size() == commands);
  });
}

TEST_CASE("The curve view turns mouse events into a gesture, lost release included", "[curve][edition][views]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
    CurveUi ui{d};
    auto& view = ui.presenter->view();
    // As the process presenter connects them.
    QObject::connect(&view, &Curve::View::pressed, ui.palette.get(), &Curve::ToolPalette::on_pressed);
    QObject::connect(&view, &Curve::View::moved, ui.palette.get(), &Curve::ToolPalette::on_moved);
    QObject::connect(&view, &Curve::View::released, ui.palette.get(), &Curve::ToolPalette::on_released);

    sendMouse(ui.scene, view, QEvent::GraphicsSceneMousePress, ui.toScene({0.5, 1.}));
    sendMouse(ui.scene, view, QEvent::GraphicsSceneMouseMove, ui.toScene({0.5, 0.6}));
    sendMouse(ui.scene, view, QEvent::GraphicsSceneMouseRelease, ui.toScene({0.5, 0.6}));
    CHECK(pointAt(d.curve(), {0.5, 0.6}));

    // The grab goes without a release: the gesture ends where it was.
    sendMouse(ui.scene, view, QEvent::GraphicsSceneMousePress, ui.toScene({0.5, 0.6}));
    sendMouse(ui.scene, view, QEvent::GraphicsSceneMouseMove, ui.toScene({0.5, 0.3}));
    QEvent ungrab{QEvent::UngrabMouse};
    ui.scene.sendEvent(&view, &ungrab);
    settle();
    CHECK(pointAt(d.curve(), {0.5, 0.3}));

    // A second ungrab, after the gesture ended, does nothing.
    const auto commands = d.stack().size();
    ui.scene.sendEvent(&view, &ungrab);
    settle();
    CHECK(d.stack().size() == commands);

    // Escape cancels whatever is in progress.
    ui.press({0.5, 0.3});
    ui.move({0.5, 0.9});
    ui.palette->on_cancel();
    settle();
    CHECK(pointAt(d.curve(), {0.5, 0.3}));
  });
}

TEST_CASE("A curve with many points is drawn directly, as a waveform or a line", "[curve][rendering]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    Points pts;
    constexpr int N = 4000;
    for(int i = 0; i <= N; i++)
      pts.push_back({double(i) / N, 0.5 + 0.4 * std::sin(i * 0.05)});
    d.setPolyline(pts);
    CurveUi ui{d, {0., 0., 1000., 200.}};
    auto& view = ui.presenter->view();
    REQUIRE(view.directDraw());

    // 4 points per pixel: a waveform.
    CHECK(litPixels(paintItem(view, {1000, 200}, {0., 0., 1000., 200.})) > 1000);

    // Wide enough for a line through them.
    ui.relayout(10000.);
    CHECK(litPixels(paintItem(view, {1000, 200}, {0., 0., 1000., 200.})) > 500);

    // No width: nothing.
    ui.relayout(0.);
    CHECK(litPixels(paintItem(view, {1000, 200}, {0., 0., 1000., 200.})) == 0);
  });
}

TEST_CASE("A sampled segment is drawn as a line where it is sparse, and not where it is empty", "[curve][sampled][rendering]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Curve::PointArraySegment seg{sampledSegment(1, 0., 1., 50), nullptr};
    TestStyle colors;
    Curve::SegmentView view{&seg, colors.style, nullptr};
    view.setRect({0., 0., 1000., 200.});

    // 50 samples over 1000 pixels: the samples joined.
    CHECK(litPixels(paintItem(view, {1000, 200}, {0., 0., 1000., 200.})) > 1000);
    // Outside the segment.
    CHECK(litPixels(paintItem(view, {1000, 200}, {2000., 0., 100., 200.})) == 0);
    // A transform that squashes it to nothing.
    CHECK(litPixels(paintItem(view, {1000, 200}, {0., 0., 1000., 200.}, QTransform::fromScale(0., 1.))) == 0);

    // With no width.
    seg.setEnd({0., seg.end().y()});
    CHECK(litPixels(paintItem(view, {1000, 200}, {0., 0., 1000., 200.})) == 0);

    // Disabled: a coarser shape.
    seg.setEnd({1., seg.end().y()});
    const int enabled = view.shape().elementCount();
    view.disable();
    CHECK(view.shape().elementCount() < enabled);
    view.enable();
  });
}

TEST_CASE("Curvature is set on the clicked segment, or all the selected ones with Alt", "[curve][edition]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.3, 1.}, {0.6, 0.}, {1., 1.}});
    auto& curve = d.curve();
    auto sorted = curve.sortedSegments();
    for(auto s : sorted)
      const_cast<Curve::SegmentModel*>(s)->selection.set(true);

    Curve::StateBase state;
    Curve::SetSegmentParametersCommandObject co{curve, d.context().commandStack};
    co.setCurveState(&state);
    state.clickedSegmentId = sorted[0]->id();
    state.currentPoint = {0.1, 0.5};
    co.press();
    state.currentPoint = {0.1, 0.7};
    co.move();
    co.release();
    settle();
    CHECK(gammaOf(curve, sorted[0]->id().val()) != Approx(1.));
    CHECK(gammaOf(curve, sorted[1]->id().val()) == Approx(1.));

    QWindow win;
    win.show();
    QWindowSystemInterface::handleKeyEvent<QWindowSystemInterface::SynchronousDelivery>(
        &win, QEvent::KeyPress, Qt::Key_Alt, Qt::AltModifier);
    REQUIRE(qApp->keyboardModifiers() & Qt::AltModifier);
    state.clickedSegmentId = sorted[1]->id();
    state.currentPoint = {0.4, 0.5};
    co.press();
    state.currentPoint = {0.4, 0.2};
    co.move();
    co.release();
    QWindowSystemInterface::handleKeyEvent<QWindowSystemInterface::SynchronousDelivery>(
        &win, QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
    settle();
    CHECK(gammaOf(curve, sorted[1]->id().val()) != Approx(1.));
    CHECK(gammaOf(curve, sorted[2]->id().val()) != Approx(1.));

    // Cancelled: back to where it was.
    const double before = gammaOf(curve, sorted[2]->id().val());
    state.clickedSegmentId = sorted[2]->id();
    co.press();
    state.currentPoint = {0.4, 0.9};
    co.move();
    co.cancel();
    settle();
    CHECK(gammaOf(curve, sorted[2]->id().val()) == Approx(before));

    // The clicked segment is gone before the move.
    state.clickedSegmentId = Id<Curve::SegmentModel>{12345};
    const auto commands = d.stack().size();
    co.press();
    co.move();
    co.release();
    CHECK(d.stack().size() == commands);
  });
}

TEST_CASE("Point moves outside a gesture, or to where they cannot go, change nothing", "[curve][edition]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
    CurveUi ui{d};
    const auto before = d.curve().toCurveData();
    const auto* mid = pointAt(d.curve(), {0.5, 1.});
    REQUIRE(mid);

    Curve::StateBase state;
    Curve::MovePointCommandObject co{d.curve(), ui.presenter.get(), d.context().commandStack};
    co.setCurveState(&state);

    // A move without a press.
    state.currentPoint = {0.5, 0.2};
    co.move();
    CHECK(sameCurve(d.curve().toCurveData(), before));

    // A point that no longer exists.
    state.clickedPointId = {Id<Curve::SegmentModel>{777}, Id<Curve::SegmentModel>{778}};
    co.press();
    co.move();
    co.release();
    CHECK(sameCurve(d.curve().toCurveData(), before));

    // Vertically only, then cancelled.
    state.clickedPointId = {mid->previous(), mid->following()};
    state.currentPoint = mid->pos();
    co.press();
    state.currentPoint = {0.5, 0.2};
    co.move();
    CHECK(pointAt(d.curve(), {0.5, 0.2}));
    co.cancel();
    settle();
    CHECK(sameCurve(d.curve().toCurveData(), before));

    // Non-finite: ignored.
    Curve::CreatePointCommandObject create{d.curve(), ui.presenter.get(), d.context().commandStack};
    create.setCurveState(&state);
    state.currentPoint = {0.25, 0.5};
    create.press();
    state.currentPoint = {std::numeric_limits<double>::quiet_NaN(), 0.5};
    create.move();
    create.release();
    settle();
    CHECK(curveError(d.curve()) == "");
  });
}

TEST_CASE("Points dragged across vertical steps and chain ends", "[curve][edition]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    for(bool suppress : {false, true})
    {
      INFO("suppress " << suppress);
      CHECK(editSurvives([&]() -> std::string {
        CurveDoc d{ctx};
        // A vertical step at 0.5.
        d.setPolyline({{0., 0.}, {0.25, 0.3}, {0.5, 0.3}, {0.5, 0.8}, {0.75, 0.8}, {1., 0.}});
        CurveUi ui{d};
        ui.presenter->setBoundedMove(false);
        EditionSettingsGuard g{ui.settings()};
        ui.settings().setLockBetweenPoints(false);
        ui.settings().setSuppressOnOverlap(suppress);

        // The step's top, right, then left across its bottom.
        ui.drag({0.5, 0.8}, {{0.6, 0.8}});
        if(auto e = curveError(d.curve()); !e.empty())
          return "right: " + e;
        const auto* top = pointAt(d.curve(), {0.6, 0.8});
        if(!top)
          return "the step's top did not follow";
        ui.drag({0.6, 0.8}, {{0.45, 0.8}, {0.1, 0.8}});
        if(auto e = curveError(d.curve()); !e.empty())
          return "left: " + e;

        // The first and last points, past their neighbours.
        ui.drag({0., 0.}, {{0.3, 0.1}, {0.9, 0.1}});
        if(auto e = curveError(d.curve()); !e.empty())
          return "first: " + e;
        ui.drag({1., 0.}, {{0.5, 0.5}, {0.05, 0.5}});
        if(auto e = curveError(d.curve()); !e.empty())
          return "last: " + e;
        return {};
      }));
    }
  });
}

TEST_CASE("Envelope edge cases", "[curve][rendering]")
{
  std::vector<double> xs{0., 1., 2., 3., 4., 5., 6., 7.}, ys{0., 1., 0., 1., 0., 1., 0., 1.};
  Curve::MinMaxPyramid pyramid;
  pyramid.build(xs.size(), [&](std::size_t i) { return ys[i]; });
  auto x = [&](std::size_t i) { return xs[i]; };
  auto y = [&](std::size_t i) { return ys[i]; };

  std::vector<QPointF> line;
  Curve::envelope(0, x, y, pyramid, 0, 0., 1., 10, line);
  CHECK(line.empty());
  Curve::envelope(xs.size(), x, y, pyramid, 0, 0., 7., 0, line);
  CHECK(line.empty());
  Curve::envelope(xs.size(), x, y, pyramid, 0, 3., 1., 10, line);
  CHECK(line.empty());
  // Inside the points: their neighbours on both sides, to reach the edges.
  Curve::envelope(xs.size(), x, y, pyramid, 0, 2.5, 4.5, 2, line);
  REQUIRE(!line.empty());
  CHECK(line.front().x() == 2.);
  CHECK(line.back().x() >= 5.);

  std::vector<QLineF> cols;
  Curve::envelopeColumns(0, x, y, pyramid, 0, 0., 1., 10, cols);
  CHECK(cols.empty());
  Curve::envelopeColumns(xs.size(), x, y, pyramid, 0, 0., 0., 10, cols);
  CHECK(cols.empty());

  CHECK(Curve::pixelColumns(0., 10., 0.).count == 0);
  CHECK(Curve::pixelColumns(10., 0., 1.).count == 0);
  const auto c = Curve::pixelColumns(0.3, 10.2, 2.);
  CHECK(c.width == 0.5);
  CHECK(c.first == 0.);
  CHECK(c.count == 21);
}

TEST_CASE("Pasting inside a segment cuts it on both sides", "[curve][edition]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.1, 1.}, {1., 0.}});
    CurveUi ui{d};
    QGraphicsView gv{&ui.scene};
    gv.setFrameStyle(QFrame::NoFrame);
    gv.setSceneRect(ui.rect);
    gv.resize(1000, 200);
    gv.show();
    settle();

    auto& ed = curveEditor(ctx);
    Selection sel;
    sel.append(d.curve().sortedSegments().front());
    JSONReader r;
    REQUIRE(ed.copy(r, sel, d.context()));
    QMimeData mime;
    mime.setData("text/plain", r.toByteArray());
    auto paste = [&](double x) {
      const QPoint global = gv.mapToGlobal(gv.mapFromScene(ui.toScene({x, 0.5})));
      return ed.paste(global, &ui.focus, mime, d.context());
    };

    // [0.4, 0.5] inside [0.1, 1]: both of its sides stay.
    CHECK(paste(0.4));
    settle();
    CHECK(curveError(d.curve()) == "");
    CHECK(d.curve().segments().size() == 4);
    CHECK(hasPointAtX(d.curve(), 0.4));
    CHECK(hasPointAtX(d.curve(), 0.5));
    // The right half now starts where the pasted segment ends, (0.5, 1).
    CHECK(*d.curve().valueAt(0.7) == Approx(0.6));

    // Over the end: the segment it starts in is cut, the rest goes.
    CHECK(paste(0.95));
    settle();
    CHECK(curveError(d.curve()) == "");
    CHECK(hasPointAtX(d.curve(), 0.95));

    // A layer with no width: nothing to paste into.
    ui.relayout(0.);
    const auto commands = d.stack().size();
    CHECK(paste(0.5));
    CHECK(d.stack().size() == commands);
  });
}

TEST_CASE("Magnetism snaps to the nearest point", "[curve][automation]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.3, 1.}, {0.7, 0.}, {1., 1.}});
    const Process::ProcessModel& a = *d.autom;
    const auto dur = a.duration();
    auto snap = [&](double x) {
      auto m = a.magneticPosition(nullptr, TimeVal(int64_t(x * dur.impl)));
      REQUIRE(m);
      return double(m->time.impl) / dur.impl;
    };
    CHECK(snap(0.) == Approx(0.));
    CHECK(snap(0.2) == Approx(0.3));
    CHECK(snap(0.45) == Approx(0.3));
    CHECK(snap(0.55) == Approx(0.7));
    CHECK(snap(2.) == Approx(1.));
    d.curve().clear();
    settle();
    CHECK(!a.magneticPosition(nullptr, TimeVal::zero()));
  });
}

TEST_CASE("Segment types other than power follow edits and play back", "[curve][model][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    const auto linear = Metadata<ConcreteKey_k, Curve::LinearSegment>::get();
    const auto easing = Metadata<ConcreteKey_k, Curve::Segment_backIn>::get();
    auto seg = [&](int id, QPointF a, QPointF b, int prev, int next, auto type, QVariant data) {
      auto s = segment(id, a, b, prev, next);
      s.type = type;
      s.specificSegmentData = data;
      return s;
    };
    submit(
        d, {seg(1, {0, 0}, {0.5, 1}, 0, 2, linear, QVariant::fromValue(Curve::LinearSegmentData{})),
            seg(2, {0.5, 1}, {1, 0}, 1, 0, easing, QVariant::fromValue(Curve::EasingData{}))});
    auto& curve = d.curve();
    REQUIRE(curve.segments().size() == 2);
    CHECK(*curve.valueAt(0.25) == Approx(0.5));

    // Moved: updated in place, data unchanged.
    submit(
        d, {seg(1, {0, 0.2}, {0.5, 1}, 0, 2, linear, QVariant::fromValue(Curve::LinearSegmentData{})),
            seg(2, {0.5, 1}, {1, 0}, 1, 0, easing, QVariant::fromValue(Curve::EasingData{}))});
    CHECK(*curve.valueAt(0.) == Approx(0.2));
    for(auto& s : curve.segments())
    {
      s.updateData(1000);
      CHECK(s.data().size() <= 1000);
      CHECK(s.data().size() >= 2);
    }

    // Executors, in every value type.
    auto x = [](double v) { return v; };
    const auto& sorted = curve.sortedSegments();
    CHECK(Engine::score_to_ossia::curve<double, double>(x, x, sorted, {})->value_at(0.25) == Approx(0.6));
    CHECK(Engine::score_to_ossia::curve<double, float>(x, x, sorted, {})->value_at(0.25) == Approx(0.6));
    auto to_int = [](double v) { return int(v * 100); };
    CHECK(Engine::score_to_ossia::curve<double, int>(x, to_int, sorted, {})->value_at(0.25) == 60);

    // Vertical: worth the value they jump to.
    Curve::LinearSegment vl{Id<Curve::SegmentModel>{9}, nullptr};
    vl.setStart({0.5, 0.});
    vl.setEnd({0.5, 1.});
    CHECK(vl.valueAt(0.5) == 1.);
    Curve::Segment_backIn ve{Id<Curve::SegmentModel>{10}, nullptr};
    ve.setStart({0.5, 0.});
    ve.setEnd({0.5, 1.});
    CHECK(ve.valueAt(0.5) == 1.);
  });
}

TEST_CASE("A point edit command survives a save and a missing point", "[curve][edition][serialization]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
    const auto* mid = pointAt(d.curve(), {0.5, 1.});
    REQUIRE(mid);
    Curve::MovePoint cmd{d.curve(), mid->id(), {0.5, 0.25}};
    Curve::MovePoint copy;
    copy.deserialize(cmd.serialize());
    copy.redo(d.context());
    settle();
    CHECK(pointAt(d.curve(), {0.5, 0.25}));
    copy.undo(d.context());
    settle();
    CHECK(pointAt(d.curve(), {0.5, 1.}));

    // The point is gone by the time it runs: nothing to move.
    Curve::MovePoint gone{d.curve(), mid->id(), {0.5, 0.25}};
    d.setPolyline({{0., 0.}, {1., 1.}});
    gone.redo(d.context());
    gone.undo(d.context());
    settle();
    CHECK(curveError(d.curve()) == "");
    CHECK(d.curve().points().size() == 2);
  });
}

TEST_CASE("A sampled segment in every state it can be in", "[curve][sampled]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    // No samples at all.
    Curve::PointArraySegment empty{Id<Curve::SegmentModel>{1}, nullptr};
    empty.setStart({0., 0.});
    empty.setEnd({1., 0.});
    CHECK(empty.valueAt(0.5) == Approx(0.));
    std::vector<QPointF> line;
    empty.envelope(0., 1., 10, line);
    CHECK(line.empty());
    std::vector<QLineF> cols;
    empty.envelopeColumns(0., 0.1, 10, cols);
    CHECK(cols.empty());
    empty.updateData(10);

    Curve::PointArraySegment seg{sampledSegment(2, 0., 1., 1000), nullptr};
    // With no width.
    Curve::PointArraySegment flat{sampledSegment(3, 0.5, 0.5, 1000), nullptr};
    flat.envelope(0., 1., 10, line);
    CHECK(line.empty());
    flat.envelopeColumns(0., 0.1, 10, cols);
    CHECK(cols.empty());
    CHECK(flat.samplesBetween(0., 1.) == 0);
    // Asked from right to left.
    CHECK(seg.samplesBetween(0.6, 0.4) == seg.samplesBetween(0.4, 0.6));
    // Outside the segment.
    seg.envelope(2., 3., 10, line);
    CHECK(line.empty());

    // Same data again: nothing changes, nothing is redrawn.
    int redraws = 0;
    QObject::connect(&seg, &Curve::SegmentModel::dataChanged, [&] { redraws++; });
    seg.setSpecificData(seg.toSegmentSpecificData());
    CHECK(redraws == 0);
    CHECK(seg.specificDataEquals(seg.toSegmentSpecificData()));
    CHECK(!seg.specificDataEquals(QVariant::fromValue(Curve::PowerSegmentData{})));
    auto other = seg.toSegmentSpecificData().value<Curve::PointArraySegmentData>();
    other.max_y = 2.;
    CHECK(!seg.specificDataEquals(QVariant::fromValue(other)));
    seg.setSpecificData(QVariant::fromValue(other));
    CHECK(redraws == 1);

    // Executors, plain and scaled, in each value type.
    const double mid = seg.valueAt(0.5);
    CHECK(seg.makeDoubleFunction()(0.5, 0., 0.) == Approx(mid));
    CHECK(seg.makeFloatFunction()(0.5, 0.f, 0.f) == Approx(mid));
    CHECK(seg.makeIntFunction()(0.5, 0, 0) == std::lround(mid));
    CHECK(seg.makeScaledDoubleFunction(10., 100.)(0.5, 0., 0.) == Approx(10. + 100. * mid));
    CHECK(seg.makeScaledIntFunction(10., 100.)(0.5, 0, 0) == std::lround(10. + 100. * mid));

    // A pen stroke that goes back: what it passes over again is replaced.
    Curve::PointArraySegment pen{Id<Curve::SegmentModel>{4}, nullptr};
    pen.setStart({0., 0.});
    pen.setEnd({1., 0.});
    pen.reserve(16);
    for(double x : {0., 1., 2., 3., 4., 5.})
      pen.addPointUnscaled(x, 0.5);
    CHECK(pen.points().size() == 6);
    pen.addPointUnscaled(1., 0.9);
    CHECK(pen.points().size() == 3); // 0, 1, 5
    pen.addPointUnscaled(6., 0.1);
    CHECK(pen.points().size() == 3); // 0, 1, 6

    // Conversions of nothing.
    Curve::SegmentData none = sampledSegment(5, 0., 1., 10);
    auto dat = none.specificSegmentData.value<Curve::PointArraySegmentData>();
    dat.points = nullptr;
    none.specificSegmentData = QVariant::fromValue(dat);
    Curve::SegmentIdAllocator ids{std::vector<Curve::SegmentData>{}};
    CHECK(Curve::editableSegments(none, 0.01, ids).empty());
  });
}

TEST_CASE("Samples given out of order are sorted, keeping the first of each x", "[curve][sampled][serialization]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    rapidjson::Document doc;
    doc.Parse(R"({"MinX": 0, "MaxX": 3, "MinY": 0, "MaxY": 1, "Points": [3, 0.3, 1, 0.1, 2, 0.2, 1, 0.9, 0, 0]})");
    auto dat = JSONWriter::unmarshall<Curve::PointArraySegmentData>(doc);
    REQUIRE(dat.points);
    REQUIRE(dat.points->size() == 4);
    auto it = dat.points->begin();
    for(int i = 0; i < 4; i++, ++it)
      CHECK(it->first == i);
    CHECK(dat.points->at(1.) == Approx(0.1));
  });
}

TEST_CASE("A curvature change through an edit updates the segment in place", "[curve][model]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {1., 1.}}, 1.);
    auto& seg = *d.curve().sortedSegments().front();
    const auto* before = &seg;
    int redraws = 0;
    QObject::connect(&seg, &Curve::SegmentModel::dataChanged, [&] { redraws++; });
    d.setPolyline({{0., 0.}, {1., 1.}}, 0.5);
    CHECK(d.curve().sortedSegments().front() == before);
    CHECK(gammaOf(d.curve(), seg.id().val()) == Approx(0.5));
    CHECK(redraws == 1);
  });
}

TEST_CASE("The execution curve evaluates before, between and after its points", "[curve][execution]")
{
  ossia::curve<double, float> c;
  c.set_x0(0.2);
  c.set_y0(0.5f);
  CHECK(c.value_at(0.5) == Approx(0.5f));
  c.add_point(ossia::curve_segment_linear<float>{}, 0.6, 1.f);
  c.add_point(ossia::curve_segment_linear<float>{}, 1., 0.f);
  CHECK(c.value_at(0.) == Approx(0.5f));
  CHECK(c.value_at(0.2) == Approx(0.5f));
  CHECK(c.value_at(0.4) == Approx(0.75f));
  CHECK(c.value_at(0.6) == Approx(1.f));
  CHECK(c.value_at(0.8) == Approx(0.5f));
  CHECK(c.value_at(2.) == Approx(0.f));
}

TEST_CASE("The segment type is changed from the context menu", "[curve][edition]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
    CurveUi ui{d};
    auto& first = *d.curve().sortedSegments().front();
    const_cast<Curve::SegmentModel&>(first).selection.set(true);

    QMenu menu;
    ui.presenter->fillContextMenu(menu, {}, {});
    QMenu* types{};
    for(auto a : menu.actions())
      if(a->menu() && a->text() == QObject::tr("Type"))
        types = a->menu();
    REQUIRE(types);
    QAction* linear{};
    std::function<void(QMenu*)> find = [&](QMenu* m) {
      for(auto a : m->actions())
      {
        if(a->menu())
          find(a->menu());
        else if(a->text() == QObject::tr("Linear"))
          linear = a;
      }
    };
    find(types);
    REQUIRE(linear);
    linear->trigger();
    settle();
    const auto key = Metadata<ConcreteKey_k, Curve::LinearSegment>::get();
    CHECK(d.curve().sortedSegments().front()->concreteKey() == key);
    CHECK(d.curve().sortedSegments().back()->concreteKey() != key);
    CHECK(curveError(d.curve()) == "");
  });
}

TEST_CASE("Moves around a vertical step, and a lone segment's end dragged past the other", "[curve][edition]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    for(bool suppress : {false, true})
    {
      INFO("suppress " << suppress);
      CHECK(editSurvives([&]() -> std::string {
        CurveDoc d{ctx};
        d.setPolyline({{0., 0.}, {0.5, 0.2}, {0.5, 0.8}, {1., 1.}});
        CurveUi ui{d};
        EditionSettingsGuard g{ui.settings()};
        ui.settings().setSuppressOnOverlap(suppress);

        // Locked: the top is held at the step's x, however far left it goes.
        ui.settings().setLockBetweenPoints(true);
        ui.drag({0.5, 0.8}, {{0.3, 0.9}});
        if(auto e = curveError(d.curve()); !e.empty())
          return "locked: " + e;
        if(!pointAt(d.curve(), {0.5, 0.9}))
          return "the locked point left the step";

        // Unlocked: the bottom goes right, across the step.
        ui.settings().setLockBetweenPoints(false);
        ui.drag({0.5, 0.2}, {{0.7, 0.1}});
        if(auto e = curveError(d.curve()); !e.empty())
          return "bottom right: " + e;
        return {};
      }));
      CHECK(editSurvives([&]() -> std::string {
        CurveDoc d{ctx};
        d.setPolyline({{0., 0.}, {0.5, 0.2}, {0.5, 0.8}, {1., 1.}});
        CurveUi ui{d};
        EditionSettingsGuard g{ui.settings()};
        ui.settings().setSuppressOnOverlap(suppress);
        ui.settings().setLockBetweenPoints(false);
        // The top, left, across the step.
        ui.drag({0.5, 0.8}, {{0.3, 0.8}});
        return curveError(d.curve());
      }));
    }

    // One segment, its start dragged past its end in a full view: nothing
    // would be left, so nothing changes.
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {1., 1.}});
    CurveUi ui{d};
    ui.presenter->setBoundedMove(false);
    EditionSettingsGuard g{ui.settings()};
    ui.settings().setLockBetweenPoints(false);
    ui.settings().setSuppressOnOverlap(false);
    ui.drag({0., 0.}, {{1.2, 0.}});
    CHECK(curveError(d.curve()) == "");
    CHECK(d.curve().segments().size() == 1);

    // A non-finite position reaching the command object directly.
    const auto before = d.curve().toCurveData();
    Curve::StateBase state;
    Curve::MovePointCommandObject co{d.curve(), ui.presenter.get(), d.context().commandStack};
    co.setCurveState(&state);
    const auto* p = d.curve().points().front();
    state.clickedPointId = {p->previous(), p->following()};
    state.currentPoint = p->pos();
    co.press();
    state.currentPoint = {std::numeric_limits<double>::quiet_NaN(), 0.};
    co.move();
    co.release();
    settle();
    CHECK(sameCurve(d.curve().toCurveData(), before));
  });
}

TEST_CASE("A curve is replaced whole, with fewer segments", "[curve][model]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.3, 1.}, {0.6, 0.}, {1., 1.}});
    const std::vector one{segment(1, {0, 0.5}, {1, 0.5})};
    d.curve().fromCurveData(one);
    CHECK(sameCurve(d.curve().toCurveData(), one));
    CHECK(d.curve().points().size() == 2);

    // Links that make no chains, with segments starting at the same x.
    Curve::SegmentData a = segment(10, {0, 0}, {0.5, 1}, 0, 11);
    Curve::SegmentData b = segment(11, {0, 1}, {1, 0}, 0, 0);
    std::vector<Id<Curve::SegmentModel>> removed{Id<Curve::SegmentModel>{1}};
    const Curve::SegmentData* up[]{&b, &a};
    d.curve().applyChanges(removed, up);
    REQUIRE(d.curve().sortedSegments().size() == 2);
    CHECK(d.curve().sortedSegments().front()->id().val() == 10);

    // A model on its own, destroyed with its segments.
    {
      Curve::Model m{Id<Curve::Model>{5}, nullptr};
      m.addSegment(new Curve::PowerSegment{Id<Curve::SegmentModel>{1}, &m});
    }
  });
}

TEST_CASE("Segment data compare by value", "[curve][model]")
{
  CHECK(QVariant::fromValue(Curve::LinearSegmentData{}) == QVariant::fromValue(Curve::LinearSegmentData{}));
  CHECK(QVariant::fromValue(Curve::EasingData{}) == QVariant::fromValue(Curve::EasingData{}));
  CHECK(QVariant::fromValue(Curve::PowerSegmentData{0.5}) != QVariant::fromValue(Curve::PowerSegmentData{1.}));
}

TEST_CASE("In a full view, the last point goes past the end", "[curve][edition]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    for(bool suppress : {false, true})
    {
      INFO("suppress " << suppress);
      CurveDoc d{ctx};
      d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
      CurveUi ui{d};
      ui.presenter->setBoundedMove(false);
      EditionSettingsGuard g{ui.settings()};
      ui.settings().setLockBetweenPoints(false);
      ui.settings().setSuppressOnOverlap(suppress);
      ui.drag({1., 0.}, {{1.2, 0.3}, {1.3, 0.4}});
      CHECK(curveError(d.curve()) == "");
      CHECK(pointAt(d.curve(), {1.3, 0.4}));
      CHECK(d.curve().segments().size() == 2);
    }
  });
}

TEST_CASE("Rubber-band selection, a click on nothing, and escape", "[curve][edition][views]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.25, 1.}, {0.5, 0.}, {0.75, 1.}, {1., 0.}});
    CurveUi ui{d};
    auto selected = [&] { return d.curve().selectedChildren().size(); };

    // Around the two middle peaks' region: some points and segments.
    ui.press({0.2, 0.95});
    ui.move({0.55, 0.6});
    ui.move({0.55, 0.05});
    ui.release({0.55, 0.05});
    CHECK(selected() > 0);
    CHECK(!ui.presenter->view().boundingRect().isEmpty());

    // The area is drawn while dragging.
    ui.press({0.6, 0.95});
    ui.move({0.9, 0.5});
    auto img = paintItem(ui.presenter->view(), {1000, 200}, {0., 0., 1000., 200.});
    CHECK(litPixels(img) > 0);
    ui.palette->on_cancel();
    settle();
    CHECK(selected() == 0);

    // A click on nothing selects the process.
    ui.press({0.6, 0.95});
    ui.release({0.6, 0.95});
    CHECK(selected() == 0);
  });
}

TEST_CASE("Every tool through the palette: create, curvature, and the smart tool on segments", "[curve][edition]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
    CurveUi ui{d};

    // Create: a point where the cursor is released.
    ui.settings().setTool(Curve::Tool::Create);
    ui.press({0.3, 0.2});
    ui.move({0.32, 0.25});
    ui.release({0.32, 0.25});
    CHECK(hasPointAtX(d.curve(), 0.32));
    CHECK(curveError(d.curve()) == "");
    // Create on an existing point and on a segment.
    ui.press({0.5, 1.});
    ui.release({0.5, 1.});
    ui.press({0.75, 0.5});
    ui.move({0.76, 0.5});
    ui.release({0.76, 0.5});
    CHECK(curveError(d.curve()) == "");

    // Set segment: dragging on a segment bends it.
    d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
    ui.settings().setTool(Curve::Tool::SetSegment);
    const auto id = d.curve().sortedSegments().front()->id().val();
    ui.press({0.25, 0.5});
    ui.move({0.25, 0.8});
    ui.release({0.25, 0.8});
    CHECK(gammaOf(d.curve(), id) != Approx(1.));
    // On a point, and on nothing: no bending.
    ui.press({0.5, 1.});
    ui.move({0.5, 0.8});
    ui.release({0.5, 0.8});
    ui.press({0.25, 0.05});
    ui.release({0.25, 0.05});

    // Select: a click on a segment, moved over a point, released on a segment.
    ui.settings().setTool(Curve::Tool::Select);
    ui.press({0.75, 0.5});
    ui.move({0.5, 1.});
    ui.move({0.25, 0.5});
    ui.move({0.25, 0.05});
    ui.release({0.75, 0.5});
    CHECK(curveError(d.curve()) == "");
    ui.press({0.5, 1.});
    ui.move({0.5, 1.});
    ui.release({0.5, 1.});
    CHECK(curveError(d.curve()) == "");
  });
}

TEST_CASE("The curve view forwards clicks, keys and context menus, and draws itself", "[curve][views]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
    CurveUi ui{d};
    auto& view = ui.presenter->view();
    int doubles = 0, presses = 0, releases = 0, menus = 0;
    QObject::connect(&view, &Curve::View::doubleClick, [&](QPointF) { doubles++; });
    QObject::connect(&view, &Curve::View::keyPressed, [&](int) { presses++; });
    QObject::connect(&view, &Curve::View::keyReleased, [&](int) { releases++; });
    QObject::connect(&view, &Curve::View::contextMenuRequested, [&](QPoint, QPointF) { menus++; });

    sendMouse(ui.scene, view, QEvent::GraphicsSceneMouseDoubleClick, {10., 10.});
    CHECK(doubles == 1);
    QKeyEvent press{QEvent::KeyPress, Qt::Key_Shift, Qt::ShiftModifier};
    QKeyEvent release{QEvent::KeyRelease, Qt::Key_Shift, Qt::NoModifier};
    ui.scene.sendEvent(&view, &press);
    ui.scene.sendEvent(&view, &release);
    CHECK(presses == 1);
    CHECK(releases == 1);
    QGraphicsSceneContextMenuEvent menu{QEvent::GraphicsSceneContextMenu};
    ui.scene.sendEvent(&view, &menu);
    CHECK(menus == 1);

    CHECK(!view.pixmap().isNull());
    view.setRect({});
    CHECK(view.pixmap().isNull());
  });
}

TEST_CASE("Selected points and segments are drawn in their own colours", "[curve][views]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
    CurveUi ui{d};
    for(auto p : d.curve().points())
      const_cast<Curve::PointModel*>(p)->selection.set(true);
    for(auto& s : d.curve().segments())
      const_cast<Curve::SegmentModel&>(s).selection.set(true);
    settle();
    for(auto& v : ui.presenter->points())
      CHECK(litPixels(paintItem(const_cast<Curve::PointView&>(v), {40, 40}, {-20., -20., 40., 40.}, QTransform::fromTranslate(20., 20.))) > 0);
    ui.presenter->enable();
    ui.presenter->disable();
    ui.presenter->enable();
  });
}

TEST_CASE("Pen strokes that start and end inside segments, and a cancelled one", "[curve][edition]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
    CurveUi ui{d};
    ui.settings().setTool(Curve::Tool::CreatePen);

    // Inside the first segment to inside the second.
    ui.press({0.2, 0.5});
    for(int i = 1; i <= 20; i++)
      ui.move({0.2 + i * 0.03, 0.5});
    ui.release({0.8, 0.5});
    CHECK(curveError(d.curve()) == "");
    CHECK(Curve::isValidCurve(d.curve().toCurveData()));

    // Cancelled: the curve is back.
    const auto before = d.curve().toCurveData();
    ui.press({0.1, 0.2});
    ui.move({0.3, 0.2});
    ui.palette->on_cancel();
    settle();
    CHECK(sameCurve(d.curve().toCurveData(), before));

    // The create tool, cancelled too.
    ui.settings().setTool(Curve::Tool::Create);
    ui.press({0.1, 0.2});
    ui.move({0.12, 0.2});
    ui.palette->on_cancel();
    settle();
    CHECK(sameCurve(d.curve().toCurveData(), before));
  });
}

TEST_CASE("Every segment type copies, evaluates and saves", "[curve][model][serialization]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // Power: curved. Through the base class, as the curve uses them.
    Curve::PowerSegment pm{Id<Curve::SegmentModel>{1}, nullptr};
    Curve::SegmentModel& p = pm;
    p.setStart({0., 0.});
    p.setEnd({1., 1.});
    p.setVerticalParameter(0.5);
    const double mid = p.valueAt(0.5);
    CHECK(mid != Approx(0.5));
    Curve::PowerSegment pcm{pm, Id<Curve::SegmentModel>{2}, nullptr};
    const Curve::SegmentModel& pc = pcm;
    CHECK(pc.valueAt(0.5) == Approx(mid));
    CHECK(p.makeDoubleFunction()(0.5, 0., 1.) == Approx(mid));
    CHECK(p.makeFloatFunction()(0.5, 0.f, 1.f) == Approx(mid));
    CHECK(p.makeIntFunction()(0.5, 0, 100) == std::lround(100 * mid));

    Curve::LinearSegment lm{Id<Curve::SegmentModel>{3}, nullptr};
    lm.setStart({0., 0.});
    lm.setEnd({1., 1.});
    Curve::LinearSegment lcm{lm, Id<Curve::SegmentModel>{4}, nullptr};
    const Curve::SegmentModel& lc = lcm;
    CHECK(lc.valueAt(0.25) == Approx(0.25));

    Curve::Segment_backIn em{Id<Curve::SegmentModel>{5}, nullptr};
    Curve::SegmentModel& e = em;
    e.setStart({0., 0.});
    e.setEnd({1., 1.});
    CHECK(e.valueAt(0.5) == Approx(ossia::easing::backIn{}(0.5)));
    CHECK(!e.verticalParameter());
    CHECK(!e.horizontalParameter());
    e.setVerticalParameter(0.3);
    e.setHorizontalParameter(0.3);
    CHECK(e.toSegmentData().specificSegmentData.canConvert<Curve::EasingData>());

    const auto flat = Curve::flatCurveSegment(5., 0., 10.);
    CHECK(flat.start.y() == Approx(0.5));
    CHECK(flat.end.y() == Approx(0.5));

    // A document with each type, saved and loaded.
    CurveDoc d{ctx};
    const auto linear = Metadata<ConcreteKey_k, Curve::LinearSegment>::get();
    const auto easing = Metadata<ConcreteKey_k, Curve::Segment_backIn>::get();
    auto s1 = segment(1, {0, 0}, {0.3, 1}, 0, 2, linear);
    auto s2 = segment(2, {0.3, 1}, {0.6, 0}, 1, 3);
    s2.specificSegmentData = QVariant::fromValue(Curve::PowerSegmentData{0.3});
    auto s3 = segment(3, {0.6, 0}, {1, 1}, 2, 0);
    s3.type = easing;
    s3.specificSegmentData = QVariant::fromValue(Curve::EasingData{});
    submit(d, {s1, s2, s3});
    const auto id = d.autom->id();
    for(auto reloaded : {score::test::reload_via_bytes(ctx, *d.doc), score::test::reload_via_json(ctx, *d.doc)})
    {
      REQUIRE(reloaded);
      auto& procs = baseInterval(*reloaded).processes;
      auto it = procs.find(id);
      REQUIRE(it != procs.end());
      auto& curve = static_cast<Automation::ProcessModel&>(*it).curve();
      CHECK(sameCurve(curve.toCurveData(), d.curve().toCurveData()));
      for(double x : {0.1, 0.45, 0.8})
        CHECK(*curve.valueAt(x) == Approx(*d.curve().valueAt(x)));
    }
  });
}

TEST_CASE("Curve domains from address domains and values", "[curve][model]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    const Curve::CurveDomain unset{ossia::domain{}};
    CHECK(unset.min == 0.);
    CHECK(unset.max == 1.);
    const Curve::CurveDomain range{ossia::make_domain(2.f, -3.f)};
    CHECK(range.start == -3.);
    CHECK(range.end == 2.);

    // From a value: the range grows to hold it.
    CHECK(Curve::CurveDomain{ossia::domain{}, ossia::value{4.f}}.max == 4.);
    CHECK(Curve::CurveDomain{ossia::domain{}, ossia::value{-4.f}}.min == -4.);
    CHECK(Curve::CurveDomain{ossia::domain{}, ossia::value{0.f}}.max == 1.);
    const Curve::CurveDomain same{ossia::make_domain(3.f, 3.f), ossia::value{3.f}};
    CHECK(same.max == 4.);

    const Curve::CurveDomain ends{ossia::make_domain(0.f, 1.f), -1., 5.};
    CHECK(ends.min == -1.);
    CHECK(ends.max == 5.);
    const Curve::CurveDomain open{ossia::domain{}, 0.2, 0.8};
    CHECK(open.min == 0.2);
    CHECK(open.max == 0.8);

    const auto back = DataStreamWriter::unmarshall<Curve::CurveDomain>(
        DataStreamReader::marshall(ends));
    CHECK(back.min == ends.min);
    CHECK(back.max == ends.max);
    CHECK(back.start == ends.start);
    CHECK(back.end == ends.end);
  });
}

TEST_CASE("Segment views of tweened segments, their menus and opaque area", "[curve][views]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
    CurveUi ui{d};
    for(auto& v : ui.presenter->segments())
    {
      auto& view = const_cast<Curve::SegmentView&>(v);
      view.setTween(true);
      view.setSelected(true);
      view.setSelected(false);
      view.setTween(false);
      CHECK(!view.opaqueArea().isEmpty());
      int menus = 0;
      QObject::connect(&view, &Curve::SegmentView::contextMenuRequested, [&](QPoint, QPointF) { menus++; });
      QGraphicsSceneContextMenuEvent menu{QEvent::GraphicsSceneContextMenu};
      ui.scene.sendEvent(&view, &menu);
      CHECK(menus == 1);
    }

    // A curvature command, saved and loaded.
    Curve::SetSegmentParameters cmd{
        d.curve(), Curve::SegmentParameterMap{{d.curve().sortedSegments().front()->id(), {0.5, 0.}}}};
    Curve::SetSegmentParameters copy;
    copy.deserialize(cmd.serialize());
    copy.redo(d.context());
    settle();
    CHECK(gammaOf(d.curve(), d.curve().sortedSegments().front()->id().val()) != Approx(1.));
  });
}

TEST_CASE("Curve domains are kept valid when refined", "[curve][model]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Curve::CurveDomain zero{ossia::domain{}, 0., 0.};
    CHECK(zero.max == 1.);
    CHECK(zero.start == 0.);
    CHECK(zero.end == 1.);
    Curve::CurveDomain point{ossia::domain{}, 3., 3.};
    CHECK(point.max == 4.);

    Curve::CurveDomain d{0.2, 0.6};
    d.refine(ossia::make_domain(-1.f, 2.f));
    CHECK(d.min == -1.);
    CHECK(d.max == 2.);
    d.refine(ossia::domain{});
    CHECK(d.min == Approx(std::min(d.start, d.end)));
  });
}

TEST_CASE("Pen strokes from and to existing points", "[curve][edition]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    for(auto [from, to] : {std::pair{0.5, 0.8}, std::pair{0.5, 0.2}, std::pair{0.2, 0.5}, std::pair{0.8, 0.5}})
    {
      INFO(from << " -> " << to);
      CurveDoc d{ctx};
      d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
      CurveUi ui{d};
      ui.settings().setTool(Curve::Tool::CreatePen);
      ui.press({from, 0.5});
      for(int i = 1; i <= 10; i++)
        ui.move({from + (to - from) * i / 10., 0.5});
      ui.release({to, 0.5});
      CHECK(curveError(d.curve()) == "");
      CHECK(Curve::isValidCurve(d.curve().toCurveData()));
    }
  });
}

TEST_CASE("Copy and paste refuse what is not theirs", "[curve][edition]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
    CurveUi ui{d};
    auto& ed = curveEditor(ctx);

    JSONReader r;
    CHECK(!ed.copy(r, Selection{}, d.context()));
    Selection other;
    other.append(d.autom);
    CHECK(!ed.copy(r, other, d.context()));
    Selection points;
    points.append(d.curve().points()[1]);
    CHECK(ed.copy(r, points, d.context()));

    // Segments that belong to no curve.
    Curve::PowerSegment lone{Id<Curve::SegmentModel>{1}, nullptr};
    Selection orphan;
    orphan.append(&lone);
    JSONReader r2;
    CHECK(ed.copy(r2, orphan, d.context()));

    QMimeData mime;
    CHECK(!ed.paste({}, nullptr, mime, d.context()));
    QObject nothing;
    CHECK(!ed.paste({}, &nothing, mime, d.context()));
    mime.setData("text/plain", "not json");
    CHECK(!ed.paste({}, &ui.focus, mime, d.context()));
    mime.setData("text/plain", R"({"Other": []})");
    CHECK(!ed.paste({}, &ui.focus, mime, d.context()));
    // Nowhere on screen: accepted, nothing done.
    mime.setData("text/plain", R"({"Segments": []})");
    const auto commands = d.stack().size();
    CHECK(ed.paste({}, &ui.focus, mime, d.context()));
    CHECK(d.stack().size() == commands);

    // The focused process holds no curve.
    Curve::Model standalone{Id<Curve::Model>{3}, nullptr};
    d.doc->focusManager().set(QPointer<Scenario::IntervalModel>{&baseInterval(*d.doc)});
    CHECK(!ed.remove(points, d.context()));
  });
}

TEST_CASE("The presenter switches to direct drawing and back, and drops all views", "[curve][views]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    CurveDoc d{ctx};
    d.setPolyline({{0., 0.}, {0.5, 1.}, {1., 0.}});
    CurveUi ui{d};
    auto& view = ui.presenter->view();
    CHECK(!view.directDraw());

    Points many;
    for(int i = 0; i <= 3500; i++)
      many.push_back({i / 3500., (i % 2) ? 1. : 0.});
    d.setPolyline(many);
    CHECK(view.directDraw());
    // Segments added one at a time while drawing directly.
    d.curve().addSegment(new Curve::PowerSegment{Id<Curve::SegmentModel>{99999}, &d.curve()});
    settle();
    CHECK(view.directDraw());

    d.setPolyline({{0., 0.}, {1., 1.}});
    CHECK(!view.directDraw());
    CHECK(ui.presenter->points().m_map.size() == 2);

    // Nothing left: no view.
    d.curve().clear();
    settle();
    d.curve().curveReset();
    settle();
    CHECK(ui.presenter->points().m_map.size() == 0);
    CHECK(ui.presenter->segments().m_map.size() == 0);

    // Nothing selected: nothing removed.
    d.setPolyline({{0., 0.}, {1., 1.}});
    const auto commands = d.stack().size();
    ui.presenter->removeSelection();
    CHECK(d.stack().size() == commands);
    ui.presenter->enableActions(true);

    // A disabled tool ignores the mouse.
    ui.settings().setTool(Curve::Tool::Disabled);
    ui.press({0., 0.});
    ui.move({0.5, 0.5});
    ui.release({0.5, 0.5});
    CHECK(d.stack().size() == commands);
  });
}
