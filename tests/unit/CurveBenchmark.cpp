// Timings of curve edition on large automations, through the same entry points
// as the GUI. Hidden: run by hand, in a release build.
//
//   CURVE_BENCH_N=10000,100000 tests/unit/test_unit_curve_bench "[.curve_bench]"
//
// Each step prints its wall time; a repeated step stops repeating once it has
// used its budget and reports the average of what ran.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Curve/Commands/SetSegmentParameters.hpp>
#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/CurveConversion.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/CurveView.hpp>
#include <Curve/Palette/CommandObjects/MovePointCommandObject.hpp>
#include <Curve/Palette/CurveEditionSettings.hpp>
#include <Curve/Palette/CurvePaletteBaseStates.hpp>
#include <Curve/Palette/CurvePalette.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <Curve/Segment/PointArray/PointArraySegment.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Automation/AutomationColors.hpp>
#include <Automation/AutomationModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/model/ObjectEditor.hpp>
#include <score/model/Skin.hpp>
#include <score/selection/Selection.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QImage>
#include <QMimeData>
#include <QPainter>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#if defined(__unix__)
#include <unistd.h>
#endif

namespace
{
using clk = std::chrono::steady_clock;

void settle()
{
  for(int i = 0; i < 3; i++)
  {
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();
  }
}

//! CURVE_BENCH_STEPS=paste,pen runs only those steps (and the loading);
//! "points" and "sampled" pick the kind of curve.
bool step(const char* name)
{
  static const auto only = qEnvironmentVariable("CURVE_BENCH_STEPS").split(',', Qt::SkipEmptyParts);
  const auto n = QString::fromLatin1(name);
  if(only.isEmpty() || only.contains(n))
    return true;
  // A step of the points benchmark, with no kind named: the points run.
  return n == "points" && !only.contains("sampled");
}

double ms_since(clk::time_point t0)
{
  return std::chrono::duration<double, std::milli>(clk::now() - t0).count();
}

//! 0 where /proc is not available.
long rss_kb()
{
  std::ifstream f("/proc/self/statm");
  long pages{}, resident{};
  f >> pages >> resident;
#if defined(__unix__)
  return resident * (sysconf(_SC_PAGESIZE) / 1024);
#else
  return 0;
#endif
}

void report(const char* step, double total_ms, int reps = 1)
{
  if(reps <= 1)
    std::printf("  %-44s %10.2f ms\n", step, total_ms);
  else
    std::printf(
        "  %-44s %10.2f ms  (%d x %.3f ms)\n", step, total_ms, reps, total_ms / reps);
  std::fflush(stdout);
}

//! Runs f up to `reps` times, stopping early once `budget_ms` is spent.
template <typename F>
void repeat(const char* step, int reps, double budget_ms, F&& f)
{
  auto t0 = clk::now();
  int done = 0;
  for(; done < reps; done++)
  {
    f(done);
    if(ms_since(t0) > budget_ms)
    {
      done++;
      break;
    }
  }
  report(step, ms_since(t0), done);
}

//! Zooming: the layer resized, then painted; and a slice of it seen at 1000x.
void zoomBench(
    QGraphicsView& gv, Curve::Presenter& pres, Curve::View& view, QRectF rect, int n)
{
  repeat("zoom: resize the layer and paint", 20, 10000., [&](int i) {
    QRectF r = rect;
    r.setWidth(rect.width() * (1. + 0.1 * (i % 5)));
    view.setRect(r);
    pres.setRect(r);
    gv.viewport()->grab();
  });
  view.setRect(rect);
  pres.setRect(rect);

  gv.scale(1000., 1.);
  gv.centerOn(rect.width() * 0.5, rect.height() * 0.5);
  repeat("paint a slice zoomed in 1000x", 20, 5000., [&](int) { gv.viewport()->grab(); });
  gv.resetTransform();
}

Scenario::IntervalModel& baseInterval(score::Document& doc)
{
  return static_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate())
      .baseInterval();
}

std::vector<Curve::SegmentData> sawtooth(int n)
{
  std::vector<Curve::SegmentData> segs;
  segs.reserve(n);
  for(int i = 0; i < n; i++)
  {
    Curve::SegmentData d;
    d.id = Id<Curve::SegmentModel>{i + 1};
    d.start = {double(i) / n, (i % 2) ? 1. : 0.};
    d.end = {double(i + 1) / n, (i % 2) ? 0. : 1.};
    if(i > 0)
      d.previous = Id<Curve::SegmentModel>{i};
    if(i + 1 < n)
      d.following = Id<Curve::SegmentModel>{i + 2};
    d.type = Metadata<ConcreteKey_k, Curve::PowerSegment>::get();
    d.specificSegmentData = QVariant::fromValue(Curve::PowerSegmentData{1.});
    segs.push_back(std::move(d));
  }
  return segs;
}

const Curve::PointModel* pointNear(const Curve::Model& m, double x)
{
  const Curve::PointModel* best{};
  double dist = 1e9;
  for(auto pt : m.points())
  {
    if(pt->previous() && pt->following() && std::abs(pt->pos().x() - x) < dist)
    {
      dist = std::abs(pt->pos().x() - x);
      best = pt;
    }
  }
  return best;
}

void bench(const score::GUIApplicationContext& ctx, int n)
{
  std::printf("\nN = %d segments\n", n);

  auto doc = score::test::new_document(ctx);
  REQUIRE(doc);
  Scenario::Command::Macro m{
      new Scenario::Command::DropProcessInIntervalMacro, doc->context()};
  auto& autom = m.createProcessInNewSlot<Automation::ProcessModel>(
      baseInterval(*doc), QString{});
  m.commit();
  settle();
  auto& curve = autom.curve();
  auto& stack = doc->commandStack();
  CommandDispatcher<> disp{doc->context().commandStack};

  // Loading, as a document or a script does.
  {
    auto data = sawtooth(n);
    auto t0 = clk::now();
    disp.submit(new Curve::UpdateCurve{curve, std::move(data)});
    report("replace the curve (UpdateCurve)", ms_since(t0));
    t0 = clk::now();
    settle();
    report("  deferred deletes", ms_since(t0));
  }
  REQUIRE(curve.segments().size() == std::size_t(n));

  QGraphicsScene scene;
  Automation::Colors colors{score::Skin::instance()};
  const QRectF rect{0., 0., 2000., 300.};
  auto view = new Curve::View{nullptr};
  scene.addItem(view);
  QObject focus;
  std::unique_ptr<Curve::Presenter> pres;
  {
    auto t0 = clk::now();
    pres = std::make_unique<Curve::Presenter>(
        doc->context(), colors.style(), curve, view, &focus);
    view->setRect(rect);
    view->setDefaultWidth(rect.width());
    pres->setRect(rect);
    report("open the layer (presenter + views)", ms_since(t0));
  }
  Curve::ToolPalette palette{doc->context(), *pres};
  auto& settings = pres->editionSettings();
  settings.setTool(Curve::Tool::Select);
  settle();

  QGraphicsView gv{&scene};
  gv.setFrameStyle(QFrame::NoFrame);
  gv.setSceneRect(rect);
  gv.resize(2000, 300);
  gv.show();
  settle();
  // Through the view's own paint event, as on screen.
  if(step("paint"))
  {
    repeat("paint the layer", 20, 5000., [&](int) { gv.viewport()->grab(); });
    zoomBench(gv, *pres, *view, rect, n);
  }

  if(step("exec"))
  // Execution: what the automation component does on each curve change, then
  // what the audio thread does on each tick.
  {
    const double min = autom.min(), max = autom.max();
    auto scale_x = [](double v) { return v; };
    auto scale_y = [=](double v) -> float { return v * (max - min) + min; };
    std::shared_ptr<ossia::curve<double, float>> c;
    repeat("build the execution curve", 10, 5000., [&](int) {
      auto segs = curve.sortedSegments();
      c = Engine::score_to_ossia::curve<double, float>(scale_x, scale_y, segs, {});
    });
    volatile float sink{};
    repeat("execution value_at, 1000 ticks of a playback", 1, 60000., [&](int) {
      for(int i = 0; i < 1000; i++)
        sink = sink + c->value_at(i / 1000.);
    });
  }

  if(step("valueat"))
  {
    volatile double sink{};
    repeat("Model::valueAt x 1000", 1, 60000., [&](int) {
      for(int i = 0; i < 1000; i++)
        sink = sink + curve.valueAt(i / 1000.).value_or(0.);
    });
  }

  if(step("drag"))
  // A point dragged through its command object, as the select tool does.
  {
    auto pt = pointNear(curve, 0.5);
    REQUIRE(pt);
    Curve::StateBase state;
    Curve::MovePointCommandObject co{curve, pres.get(), doc->context().commandStack};
    co.setCurveState(&state);
    state.clickedPointId = {pt->previous(), pt->following()};
    state.currentPoint = pt->pos();
    const auto orig = pt->pos();

    auto t0 = clk::now();
    co.press();
    report("drag: press", ms_since(t0));
    repeat("drag: move", 20, 20000., [&](int i) {
      state.currentPoint = {orig.x(), 0.2 + 0.03 * i};
      co.move();
    });
    t0 = clk::now();
    co.release();
    report("drag: release", ms_since(t0));
    t0 = clk::now();
    settle();
    report("drag: deferred deletes", ms_since(t0));

    t0 = clk::now();
    stack.undo();
    report("undo the drag", ms_since(t0));
    t0 = clk::now();
    stack.redo();
    report("redo the drag", ms_since(t0));
    settle();
  }

  if(step("curvature"))
  // Shift-drag on one segment.
  {
    auto& seg = *curve.segments().begin();
    repeat("set one segment's curvature, and undo", 20, 20000., [&](int) {
      disp.submit(new Curve::SetSegmentParameters{
          curve, Curve::SegmentParameterMap{{seg.id(), {0.5, 0.}}}});
      stack.undo();
    });
    settle();
  }

  if(step("create"))
  // Double-click in the middle: create tool.
  {
    auto t0 = clk::now();
    palette.createPoint({0.5 * rect.width() + 0.37, 0.3 * rect.height()});
    settle();
    report("create a point (double click)", ms_since(t0));
  }

  if(step("pen"))
  // A pen stroke over a tenth of the curve.
  {
    settings.setTool(Curve::Tool::CreatePen);
    settle();
    auto toScene
        = [&](double x, double y) { return QPointF{x * rect.width(), (1. - y) * rect.height()}; };
    auto t0 = clk::now();
    palette.on_pressed(toScene(0.3, 0.5));
    settle();
    int moves = 0;
    for(; moves < 200; moves++)
    {
      const double x = 0.3 + moves * 0.0005;
      palette.on_moved(toScene(x, 0.5 + 0.3 * std::sin(x * 200.)));
      settle();
      if(ms_since(t0) > 20000.)
        break;
    }
    palette.on_released(toScene(0.3 + moves * 0.0005, 0.5));
    settle();
    report("pen stroke (moves)", ms_since(t0), moves);
    settings.setTool(Curve::Tool::Select);
    settle();
  }

  if(step("remove"))
  // Remove a thousand selected points (context menu > Remove).
  {
    Selection sel;
    int k = 0;
    for(auto pt : curve.points())
      if(pt->previous() && pt->following() && pt->pos().x() > 0.6 && k++ < 1000)
        sel.append(pt);
    auto t0 = clk::now();
    const_cast<Curve::Model&>(curve).setSelection(sel);
    report("select 1000 points", ms_since(t0));
    t0 = clk::now();
    pres->removeSelection();
    settle();
    report("remove 1000 points", ms_since(t0));
  }

  if(step("paste"))
  // Copy a thousand segments and paste them elsewhere.
  {
    auto ed = ctx.interfaces<score::ObjectEditorList>().get(
        UuidKey<score::ObjectEditor>{"d2b20e55-296f-49cc-a1c5-1ba1a1122d07"});
    REQUIRE(ed);
    Selection sel;
    int k = 0;
    for(auto& seg : curve.segments())
      if(seg.start().x() > 0.1 && seg.end().x() < 0.2 && k++ < 1000)
        sel.append(seg);
    JSONReader r;
    auto t0 = clk::now();
    ed->copy(r, sel, doc->context());
    report("copy 1000 segments", ms_since(t0));

    QMimeData mime;
    mime.setData("text/plain", r.toByteArray());
    const QPoint global = gv.mapToGlobal(gv.mapFromScene(QPointF{0.8 * rect.width(), 10.}));
    t0 = clk::now();
    ed->paste(global, &focus, mime, doc->context());
    settle();
    report("paste 1000 segments", ms_since(t0));
  }

  // The undo stack holds every edit above.
  std::printf("  %-44s %10ld kB\n", "resident memory", rss_kb());

  if(step("reload"))
  {
    // The reloaded documents must hold the whole curve, or the time says nothing.
    auto segments_in = [&](score::Document& d) -> std::size_t {
      auto& procs = baseInterval(d).processes;
      auto it = procs.find(autom.id());
      if(it == procs.end())
        return 0;
      return static_cast<Automation::ProcessModel&>(*it).curve().segments().size();
    };
    const auto expected = curve.segments().size();
    std::printf("  %-44s %10zu\n", "segments saved", expected);

    auto t0 = clk::now();
    auto reloaded = score::test::reload_via_bytes(ctx, *doc);
    report("save and reload the document (binary)", ms_since(t0));
    REQUIRE(reloaded);
    REQUIRE(segments_in(*reloaded) == expected);
    t0 = clk::now();
    auto json = score::test::reload_via_json(ctx, *doc);
    report("save and reload the document (JSON)", ms_since(t0));
    REQUIRE(json);
    REQUIRE(segments_in(*json) == expected);
  }

  palette.on_cancel();
  settle();
}

//! What a file import does with N values: one sampled segment.
void benchSampled(const score::GUIApplicationContext& ctx, int n)
{
  std::printf("\nN = %d samples, as one sampled segment\n", n);
  auto doc = score::test::new_document(ctx);
  REQUIRE(doc);
  Scenario::Command::Macro m{
      new Scenario::Command::DropProcessInIntervalMacro, doc->context()};
  auto& autom = m.createProcessInNewSlot<Automation::ProcessModel>(
      baseInterval(*doc), QString{});
  m.commit();
  settle();
  auto& curve = autom.curve();

  // Noisy, like sensor data: each pixel column spans most of the height.
  std::vector<float> values(n);
  uint32_t seed = 12345;
  for(int i = 0; i < n; i++)
  {
    seed = seed * 1664525u + 1013904223u;
    const float noise = float(seed >> 8) / float(1 << 24);
    values[i] = 0.3f + 0.2f * std::sin(20. * 2. * M_PI * i / double(n - 1)) + 0.4f * noise;
  }

  const long rss0 = rss_kb();
  auto t0 = clk::now();
  CommandDispatcher<>{doc->context().commandStack}.submit(
      new Curve::UpdateCurve{curve, Curve::curveFromSamples(values)});
  settle();
  report("import (curveFromSamples + UpdateCurve)", ms_since(t0));
  std::printf("  %-44s %10ld kB\n", "resident memory added", rss_kb() - rss0);
  REQUIRE(curve.segments().size() == 1);

  QGraphicsScene scene;
  Automation::Colors colors{score::Skin::instance()};
  const QRectF rect{0., 0., 2000., 300.};
  auto view = new Curve::View{nullptr};
  scene.addItem(view);
  QObject focus;
  t0 = clk::now();
  auto pres = std::make_unique<Curve::Presenter>(
      doc->context(), colors.style(), curve, view, &focus);
  view->setRect(rect);
  view->setDefaultWidth(rect.width());
  pres->setRect(rect);
  report("open the layer (decimated drawing)", ms_since(t0));

  QGraphicsView gv{&scene};
  gv.setFrameStyle(QFrame::NoFrame);
  gv.setSceneRect(rect);
  gv.resize(2000, 300);
  gv.show();
  settle();
  repeat("paint the layer", 20, 5000., [&](int) { gv.viewport()->grab(); });
  zoomBench(gv, *pres, *view, rect, n);

  std::shared_ptr<ossia::curve<double, float>> c;
  repeat("build the execution curve", 10, 5000., [&](int) {
    c = Engine::score_to_ossia::curve<double, float>(
        [](double v) { return v; }, [](double v) -> float { return v; },
        curve.sortedSegments(), {});
  });
  volatile float sink{};
  repeat("execution value_at, 1000 ticks of a playback", 1, 60000., [&](int) {
    for(int i = 0; i < 1000; i++)
      sink = sink + c->value_at(i / 1000.);
  });

  t0 = clk::now();
  auto reloaded = score::test::reload_via_bytes(ctx, *doc);
  report("save and reload the document (binary)", ms_since(t0));
  REQUIRE(reloaded);

  t0 = clk::now();
  pres->convertSamplesToPoints();
  settle();
  report("convert to editable points", ms_since(t0));
  std::printf("  %-44s %10zu\n", "segments after conversion", curve.segments().size());
}
}

TEST_CASE("Curve edition on large automations", "[.curve_bench]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QString sizes = qEnvironmentVariable("CURVE_BENCH_N", "10000,100000");
    for(const auto& s : sizes.split(','))
    {
      if(step("points"))
        bench(ctx, s.toInt());
      if(step("sampled"))
        benchSampled(ctx, s.toInt());
    }
  });
}
