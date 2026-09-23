// Curve edition as the GUI drives it: mouse positions fed to Curve::ToolPalette
// on a real Presenter and View. Scenarios that can abort run in a forked child
// (ForkProbe), which also checks the model's invariants.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/ForkProbe.hpp>

#include <Curve/Commands/MovePoint.hpp>
#include <Curve/Commands/SetSegmentParameters.hpp>
#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/CurveView.hpp>
#include <Curve/Palette/CommandObjects/CreatePointCommandObject.hpp>
#include <Curve/Palette/CommandObjects/MovePointCommandObject.hpp>
#include <Curve/Palette/CommandObjects/SetSegmentParametersCommandObject.hpp>
#include <Curve/Palette/CurveEditionSettings.hpp>
#include <Curve/Palette/CurvePalette.hpp>
#include <Curve/Point/CurvePointModel.hpp>
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

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <score/model/ObjectEditor.hpp>
#include <score/model/Skin.hpp>
#include <score/selection/Selection.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
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

#include <cmath>
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
std::string curveError(const Curve::Model& m)
{
  std::ostringstream err;
  const auto segs = Curve::orderedSegments(m);
  auto find = [&](const Id<Curve::SegmentModel>& id) -> const Curve::SegmentData* {
    for(auto& s : segs)
      if(s.id == id)
        return &s;
    return nullptr;
  };
  auto finite = [](QPointF p) { return std::isfinite(p.x()) && std::isfinite(p.y()); };

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

    int starts = 0, ends = 0;
    for(auto pt : m.points())
    {
      if(pt->following() == s.id)
      {
        starts++;
        if(pt->pos() != s.start)
          err << "the start point of segment " << id << " is misplaced; ";
      }
      if(pt->previous() == s.id)
      {
        ends++;
        if(pt->pos() != s.end)
          err << "the end point of segment " << id << " is misplaced; ";
      }
    }
    if(starts != 1 || ends != 1)
      err << "segment " << id << " has " << starts << " start and " << ends
          << " end points; ";
  }

  for(std::size_t i = 0; i + 1 < segs.size(); i++)
    if(segs[i + 1].start.x() < segs[i].end.x())
      err << "segments " << segs[i].id.val() << " and " << segs[i + 1].id.val()
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
      res.push_back(s.specificSegmentData.value<Curve::PointArraySegmentData>()
                        .m_points.size());
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
  // MovePoint addresses the point by Id<PointModel>, and every UpdateCurve
  // rebuilds the points: this holds only because a rebuild of the same curve
  // hands out the same linear ids in the same x order.
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
      disp.submit(d.curve(), std::vector{good});
      disp.submit(d.curve(), std::vector{broken});
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
        CHECK((std::isfinite(p.x()) && std::isfinite(p.y())));
    }

    Curve::PowerSegment step{Id<Curve::SegmentModel>{2}, nullptr};
    step.setStart({0.5, 0.});
    step.setEnd({0.5, 1.});
    CHECK(std::isfinite(static_cast<const Curve::SegmentModel&>(step).valueAt(0.5)));
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
