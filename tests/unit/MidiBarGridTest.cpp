// The piano roll draws the bars of the interval holding it in musical mode,
// where the timeline has them, however deep and wherever it starts.

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>

#include <ossia/editor/scenario/time_signature.hpp>

#include <QApplication>

#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/LayerView.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ProcessList.hpp>

#include <QGraphicsRectItem>
#include <QGraphicsScene>

#include <Midi/MidiBarGrid.hpp>
#include <Midi/MidiPresenter.hpp>
#include <Midi/MidiProcess.hpp>
#include <Midi/MidiView.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Process.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using Catch::Approx;

namespace
{
//! Flicks of a 4/4 bar.
constexpr int64_t bar_flicks = 4 * ossia::quarter_duration<int64_t>;

//! Pixels per bar in these tests.
constexpr double bar_px = 200.;
constexpr double zoom = double(bar_flicks) / bar_px;

bool has_line_at(const std::vector<double>& xs, double x)
{
  return std::any_of(xs.begin(), xs.end(), [=](double l) { return std::abs(l - x) < 0.5; });
}

int lines_between(const std::vector<double>& xs, double a, double b)
{
  return int(std::count_if(xs.begin(), xs.end(), [=](double x) { return x > a && x < b; }));
}

struct Setup
{
  Scenario::IntervalModel* box{};
  Midi::ProcessModel* midi{};
};

//! A piano roll in a box starting half a bar into the score.
Setup make(score::Document& doc)
{
  auto& scenario = score::test::base_scenario(doc);
  Setup s;
  Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc.context()};
  s.box = &m.createBox(
      scenario, TimeVal{bar_flicks / 2}, TimeVal{bar_flicks / 2 + 8 * bar_flicks}, 0.4);
  s.midi = qobject_cast<Midi::ProcessModel*>(m.createProcessInNewSlot(
      *s.box, Metadata<ConcreteKey_k, Midi::ProcessModel>::get(), QString{}));
  m.commit();
  return s;
}
}

TEST_CASE(
    "The piano roll's bar lines are the score's, counted from where its box starts",
    "[midi][musical-grid]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& settings = ctx.settings<Scenario::Settings::Model>();
    settings.setMeasureBars(true);
    REQUIRE(score::test::base_interval(*doc).hasTimeSignature());

    auto s = make(*doc);
    REQUIRE(s.midi);

    Midi::BarGrid grid{*s.midi, settings, nullptr};
    grid.setZoom(zoom);
    const auto& lines = grid.lines(0., 4 * bar_px);

    // The box starts half a bar in: the score's next bar is half a bar into it.
    CHECK(has_line_at(lines.bars, bar_px / 2));
    CHECK(has_line_at(lines.bars, bar_px / 2 + bar_px));
    CHECK(has_line_at(lines.bars, bar_px / 2 + 2 * bar_px));
    CHECK_FALSE(has_line_at(lines.bars, 0.));

    // Beats and their halves between two bars, evenly.
    const int subs = lines_between(lines.subdivisions, bar_px / 2, bar_px / 2 + bar_px);
    CHECK(subs >= 3);
    CHECK((subs + 1) % 4 == 0);
  });
}

TEST_CASE("The piano roll has no bar lines outside musical mode", "[midi][musical-grid]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& settings = ctx.settings<Scenario::Settings::Model>();
    auto s = make(*doc);

    Midi::BarGrid grid{*s.midi, settings, nullptr};
    grid.setZoom(zoom);

    settings.setMeasureBars(false);
    CHECK(grid.lines(0., 4 * bar_px).bars.empty());
    CHECK(grid.lines(0., 4 * bar_px).subdivisions.empty());

    settings.setMeasureBars(true);
    CHECK_FALSE(grid.lines(0., 4 * bar_px).bars.empty());
  });
}

TEST_CASE(
    "A box given a time signature gives its piano roll bars counted from its start",
    "[midi][musical-grid]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& settings = ctx.settings<Scenario::Settings::Model>();
    settings.setMeasureBars(true);
    auto s = make(*doc);

    Midi::BarGrid grid{*s.midi, settings, nullptr};
    grid.setZoom(zoom);

    score::test::base_interval(*doc).setHasTimeSignature(false);
    CHECK(grid.lines(0., 4 * bar_px).bars.empty());

    // 4/4 from the box's own start.
    s.box->setHasTimeSignature(true);
    const auto& lines = grid.lines(0., 4 * bar_px);
    CHECK(has_line_at(lines.bars, 0.));
    CHECK(has_line_at(lines.bars, bar_px));
    CHECK_FALSE(has_line_at(lines.bars, bar_px / 2));
  });
}

TEST_CASE(
    "A note snaps to a line the piano roll draws, and is stored at its time",
    "[midi][musical-grid]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& settings = ctx.settings<Scenario::Settings::Model>();
    settings.setMeasureBars(true);
    settings.setMagneticMeasures(true);
    auto s = make(*doc);

    // The layer, as a slot shows it, at a zoom where a bar is not a whole
    // number of pixels
    Process::DataflowManager dfm;
    FocusDispatcher fd;
    Process::Context pctx{doc->context(), dfm, fd};
    QGraphicsScene scene;
    QGraphicsRectItem root{QRectF{0., 0., 4000., 1000.}};
    scene.addItem(&root);
    auto& layers = ctx.interfaces<Process::LayerFactoryList>();
    auto* fact = layers.findDefaultFactory(*s.midi);
    REQUIRE(fact);
    auto* view = static_cast<Midi::View*>(fact->makeLayerView(*s.midi, pctx, &root));
    auto* presenter = fact->makeLayerPresenter(*s.midi, view, pctx, nullptr);
    const double barPx = 200.3;
    const ZoomRatio zr = double(bar_flicks) / barPx;
    presenter->on_zoomRatioChanged(zr);
    const double width = s.midi->duration().toPixels(zr);
    presenter->setHeight(600.);
    presenter->setWidth(width, width);

    Midi::BarGrid grid{*s.midi, settings, nullptr};
    grid.setZoom(zr);
    const auto& lines = grid.lines(0., width);

    // Near the score's second bar in the box, half a bar after its start
    const double bar = barPx / 2. + barPx;
    REQUIRE(has_line_at(lines.bars, bar));
    const auto near = grid.nearestLine(bar + 3.);
    REQUIRE(near);
    CHECK(*near == Approx(bar));
    for(double x : {bar + 3., bar - 4., barPx * 3.2})
    {
      const auto line = grid.nearestLine(x);
      REQUIRE(line);
      CHECK((has_line_at(lines.bars, *line) || has_line_at(lines.subdivisions, *line)));
    }

    // Stored at the bar's time, not a pixel's rounding away from it
    const auto note = view->noteAtPos(QPointF{bar + 3., 300.});
    const double stored = note.start() * double(s.midi->duration().impl);
    CHECK(std::abs(stored - double(bar_flicks + bar_flicks / 2)) < 1e-6 * bar_flicks);

    delete presenter;
    delete view;
    scene.removeItem(&root);
  });
}

TEST_CASE("Moving the box moves the piano roll's bar lines", "[midi][musical-grid]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& settings = ctx.settings<Scenario::Settings::Model>();
    settings.setMeasureBars(true);
    auto s = make(*doc);

    Midi::BarGrid grid{*s.midi, settings, nullptr};
    grid.setZoom(zoom);
    int changes = 0;
    QObject::connect(&grid, &Midi::BarGrid::changed, [&] { changes++; });
    CHECK(has_line_at(grid.lines(0., 4 * bar_px).bars, bar_px / 2));

    // A quarter of a bar later: the score's bars come a quarter earlier in it
    s.box->setStartDate(s.box->date() + TimeVal{bar_flicks / 4});
    CHECK(changes == 1);
    const auto& lines = grid.lines(0., 4 * bar_px);
    CHECK(has_line_at(lines.bars, bar_px / 4));
    CHECK_FALSE(has_line_at(lines.bars, bar_px / 2));
  });
}
