// The grid chooser: halp::grid_chooser as an avendish process declares it, the
// Process::GridChooser port score makes of it (saved and loaded with its
// {value, unit} and what it allows), the values the object receives, and the
// knob: its readout, its unit menu, its detents and its reset.

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Crousti/Executor.hpp>
#include <Crousti/ProcessModel.hpp>

#include <score/graphics/InfiniteScroller.hpp>
#include <score/graphics/widgets/QGraphicsGridChooser.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/document/Document.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>

#include <Avnd/Factories.hpp>
#include <avnd/binding/ossia/from_value.hpp>
#include <avnd/binding/ossia/to_value.hpp>
#include <catch2/catch_test_macros.hpp>
#include <halp/grid_chooser.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

namespace
{
using Grid = score::QGraphicsGridChooser;

struct GridSpec
{
  halp_meta(name, "Grid chooser test")
  halp_meta(c_name, "grid_chooser_test")
  halp_meta(uuid, "e3a3d0c8-5b0f-4a43-9f4e-2a8f1e7d6c51");
  struct
  {
    halp::grid_chooser<
        "Quantize",
        halp::grid_setup{
            .units = halp::grid_units::musical | halp::grid_units::period,
            .init = halp::grid_value::bars(1),
            .max_bars = 8},
        "cycle">
        quantize;
  } inputs;
  struct
  {
  } outputs;
  void operator()() { }
};

//! The same object when its control was a time chooser
struct TimeSpec
{
  halp_meta(name, "Grid chooser test")
  halp_meta(c_name, "grid_chooser_test")
  halp_meta(uuid, "e3a3d0c8-5b0f-4a43-9f4e-2a8f1e7d6c51");
  struct
  {
    halp::time_chooser<"Quantize"> quantize;
  } inputs;
  struct
  {
  } outputs;
  void operator()() { }
};

//! The process saved, and loaded as a GridSpec. Not through the process
//! list: these test objects are not registered there.
template <typename Spec>
Process::ProcessModel* load_json(const Process::ProcessModel& proc, QObject* parent)
{
  JSONReader reader;
  reader.readFrom(proc);
  const auto json = readJson(reader.toByteArray());
  JSONObject::Deserializer des{json};
  return new oscr::ProcessModel<Spec>{des, parent};
}

template <typename Spec>
Process::ProcessModel*
load_datastream(const Process::ProcessModel& proc, QObject* parent)
{
  const QByteArray bytes = DataStreamReader::marshall(proc);
  DataStream::Deserializer outer{bytes};
  QByteArray process;
  outer.stream() >> process;
  DataStream::Deserializer des{process};
  SCORE_DEBUG_CHECK_DELIMITER2(des);
  UuidKey<Process::ProcessModel> key;
  TSerializer<DataStream, UuidKey<Process::ProcessModel>>::writeTo(des, key);
  SCORE_DEBUG_CHECK_DELIMITER2(des);
  return new oscr::ProcessModel<Spec>{des, parent};
}

ossia::vec2f grid(const Process::ControlInlet& p)
{
  return ossia::convert<ossia::vec2f>(p.value());
}
}

TEST_CASE("Grid chooser: the port of a halp::grid_chooser", "[grid_chooser]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    auto& itv = score::test::base_interval(*doc);

    auto* proc = new oscr::ProcessModel<GridSpec>{
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{7781}, dctx, &itv};
    itv.processes.add(proc);

    auto* port = qobject_cast<Process::GridChooser*>(proc->inlets()[0]);
    REQUIRE(port);
    CHECK(port->units == (halp::grid_units::musical | halp::grid_units::period));
    CHECK(port->maxBars == 8);
    CHECK(port->periodName == QStringLiteral("cycle"));
    CHECK(grid(*port) == ossia::vec2f{1.f, 2.f});
    CHECK(ossia::convert<ossia::vec2f>(port->init()) == ossia::vec2f{1.f, 2.f});

    // A number is seconds
    port->setValue(0.5f);
    CHECK(grid(*port) == ossia::vec2f{0.5f, 0.f});

    // Three bars, saved and loaded: the value and what the control allows
    port->setValue(ossia::vec2f{3.f, 2.f});
    for(auto load : {&load_json<GridSpec>, &load_datastream<GridSpec>})
    {
      auto* loaded = load(*proc, &doc->model());
      REQUIRE(loaded);
      auto* p = qobject_cast<Process::GridChooser*>(loaded->inlets()[0]);
      REQUIRE(p);
      CHECK(p->id() == port->id());
      CHECK(grid(*p) == ossia::vec2f{3.f, 2.f});
      CHECK(p->units == port->units);
      CHECK(p->maxBars == 8);
      CHECK(p->maxSeconds == port->maxSeconds);
      CHECK(p->periodName == QStringLiteral("cycle"));
      delete loaded;
    }
  });
}

TEST_CASE("Grid chooser: a time chooser's value keeps its meaning", "[grid_chooser]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();

    // A time chooser's {x, mode}: mode 0 is seconds, as the grid's free unit;
    // synced, x is a fraction of a whole note, as the grid's note unit.
    for(auto [saved, expected] :
        {std::pair{ossia::vec2f{0.125f, 1.f}, halp::grid_value::note(0.125)},
         std::pair{ossia::vec2f{0.3f, 0.f}, halp::grid_value::seconds(0.3f)}})
    {
      oscr::ProcessModel<TimeSpec> old{
          TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{7782}, dctx, nullptr};
      auto* tc = qobject_cast<Process::TimeChooser*>(old.inlets()[0]);
      REQUIRE(tc);
      tc->setValue(saved);

      // Loaded with the object as it is now
      std::unique_ptr<Process::ProcessModel> loaded{load_json<GridSpec>(
          static_cast<const Process::ProcessModel&>(old), &doc->model())};
      auto* p = qobject_cast<Process::GridChooser*>(loaded->inlets()[0]);
      REQUIRE(p);
      halp::grid_value received;
      oscr::from_ossia_value(p->value(), received);
      CHECK(received.unit == expected.unit);
      CHECK(float(received.value) == float(expected.value));
    }
  });
}

TEST_CASE("Grid chooser: what the object receives", "[grid_chooser]")
{
  halp::grid_value g;
  oscr::from_ossia_value(ossia::value{ossia::vec2f{3.f, 2.f}}, g);
  CHECK(g == halp::grid_value::bars(3));
  oscr::from_ossia_value(ossia::value{ossia::vec2f{0.25f, 3.f}}, g);
  CHECK(g == halp::grid_value::period(0.25));
  oscr::from_ossia_value(ossia::value{ossia::vec2f{1.f, -1.f}}, g);
  CHECK(g.unit == halp::grid_unit::parent);
  // A cable or an OSC message: seconds
  oscr::from_ossia_value(ossia::value{0.5f}, g);
  CHECK(g == halp::grid_value::seconds(0.5));
  // The offset rides along as a third component
  oscr::from_ossia_value(ossia::value{ossia::vec3f{0.25f, 1.f, 0.5f}}, g);
  CHECK(g.unit == halp::grid_unit::note);
  CHECK(g.offset == 0.5);

  CHECK(
      oscr::to_ossia_value(halp::grid_value::bars(3))
      == ossia::value{ossia::vec2f{3.f, 2.f}});
  CHECK(
      oscr::to_ossia_value(halp::grid_value{halp::grid_unit::note, 0.25, 0.5})
      == ossia::value{ossia::vec3f{0.25f, 1.f, 0.5f}});
}

TEST_CASE("Grid chooser: readouts", "[grid_chooser]")
{
  CHECK(Grid::label({0.f, 0.f}) == "None");
  CHECK(Grid::label({0.25f, 0.f}) == "250 ms");
  CHECK(Grid::label({2.f, 0.f}) == "2.00 s");
  CHECK(Grid::label({1.f / 12.f, 1.f}) == "1/8T");
  CHECK(Grid::label({3.f / 16.f, 1.f}) == "1/8.");
  CHECK(Grid::label({1.f, 1.f}) == "1/1");
  CHECK(Grid::label({2.f, 1.f}) == "2/1");
  CHECK(Grid::label({0.2f, 1.f}) == "1/5");
  CHECK(Grid::label({1.f, 2.f}) == "1 bar");
  CHECK(Grid::label({3.f, 2.f}) == "3 bars");
  CHECK(Grid::label({1.f, 3.f}) == "Loop");
  CHECK(Grid::label({2.f, 3.f}) == "2x loop");
  CHECK(Grid::label({0.5f, 3.f}) == "1/2 loop");
  CHECK(Grid::label({0.25f, 3.f}, "cycle") == "1/4 cycle");
  CHECK(Grid::label({0.f, -1.f}) == "Parent");

  // Plain ASCII, always
  for(auto v : {ossia::vec2f{0.004f, 0.f}, {1.f / 48.f, 1.f}, {16.f, 2.f}, {8.f, 3.f}})
    for(QChar c : Grid::label(v))
      CHECK(c.unicode() < 128);
}

TEST_CASE("Grid chooser: units, menu and memory", "[grid_chooser]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QGraphicsScene scene;
    auto* item = new Grid{nullptr};
    scene.addItem(item);
    item->setUnits(
        Grid::flag(Grid::Free) | Grid::flag(Grid::Note) | Grid::flag(Grid::Period));
    item->setPeriodName("loop");
    int released = 0;
    QObject::connect(item, &Grid::sliderReleased, item, [&] { released++; });

    item->setValue({3.f / 16.f, 1.f});
    CHECK(item->unit() == Grid::Note);
    CHECK(item->feel() == Grid::Feel::Dotted);
    CHECK(item->text() == "1/8.");

    // The menu lists the allowed units, then the feels of a note value
    {
      std::unique_ptr<QMenu> menu{item->makeUnitMenu()};
      QStringList names;
      for(auto* a : menu->actions())
        if(!a->isSeparator())
          names << a->text();
      CHECK(
          names
          == QStringList{
              "Free time", "Note value", "Loop", "Straight", "Dotted", "Triplet"});

      // The same note, triplet
      for(auto* a : menu->actions())
        if(a->text() == "Triplet")
          a->trigger();
      CHECK(item->text() == "1/8T");
      CHECK(item->value() == ossia::vec2f{1.f / 12.f, 1.f});
      CHECK(released == 1);
    }

    // To the loop and back: each unit comes back to where it was left
    item->chooseUnit(Grid::Period);
    CHECK(item->value() == ossia::vec2f{1.f, 3.f});
    CHECK(item->text() == "Loop");
    item->setValue({2.f, 3.f});
    item->chooseUnit(Grid::Note);
    CHECK(item->value() == ossia::vec2f{1.f / 12.f, 1.f});
    item->chooseUnit(Grid::Period);
    CHECK(item->value() == ossia::vec2f{2.f, 3.f});
    CHECK(released == 4);

    {
      std::unique_ptr<QMenu> menu{item->makeUnitMenu()};
      // No feels outside of note values; bars are not allowed here
      CHECK(menu->actions().size() == 3);
    }

    // A value off the detents is kept until the knob moves
    item->setValue({0.3f, 3.f});
    CHECK(item->value() == ossia::vec2f{0.3f, 3.f});

    scene.removeItem(item);
    delete item;
  });
}

namespace
{
struct Scene final : public QGraphicsScene
{
  using QGraphicsScene::sendEvent;
};

void mouse(Scene& scene, Grid& item, QEvent::Type t, QPoint from, QPoint to, QPointF pos)
{
  QGraphicsSceneMouseEvent ev{t};
  ev.setScreenPos(to);
  ev.setLastScreenPos(from);
  ev.setButtonDownScreenPos(Qt::LeftButton, from);
  ev.setButton(Qt::LeftButton);
  ev.setButtons(Qt::LeftButton);
  ev.setPos(pos);
  ev.setScenePos(item.mapToScene(pos));
  scene.sendEvent(&item, &ev);
}
}

TEST_CASE(
    "Grid chooser: the knob steps through whole bars, and resets", "[grid_chooser]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    auto* item = new Grid{nullptr};
    scene.addItem(item);
    item->setMaxBars(8);
    item->init = {1.f, 2.f};
    item->setValue({2.f, 2.f});

    score::InfiniteScroller::cancel();
    const QPointF dial{17., 12.};
    mouse(scene, *item, QEvent::GraphicsSceneMousePress, {500, 500}, {500, 500}, dial);
    const QPoint p0 = score::InfiniteScroller::currentGeometry.center().toPoint();

    std::vector<float> seen;
    for(int dy = 5; dy <= 2000; dy += 5)
    {
      mouse(
          scene, *item, QEvent::GraphicsSceneMouseMove, p0 - QPoint{0, dy - 5},
          p0 - QPoint{0, dy}, dial);
      const auto v = item->value();
      CHECK(v[1] == 2.f);
      CHECK(v[0] == std::round(v[0]));
      CHECK(v[0] >= 1.f);
      CHECK(v[0] <= 8.f);
      seen.push_back(v[0]);
    }
    mouse(
        scene, *item, QEvent::GraphicsSceneMouseRelease, p0 - QPoint{0, 2000},
        p0 - QPoint{0, 2000}, dial);
    // Up to the longest grid the control allows, through every bar count
    CHECK(item->value() == ossia::vec2f{8.f, 2.f});
    for(float b = 3.f; b <= 8.f; b++)
      CHECK(std::find(seen.begin(), seen.end(), b) != seen.end());

    // Double-click on the dial: the default, in its unit
    item->chooseUnit(Grid::Free);
    mouse(
        scene, *item, QEvent::GraphicsSceneMouseDoubleClick, {500, 500}, {500, 500},
        dial);
    CHECK(item->value() == ossia::vec2f{1.f, 2.f});
    CHECK(item->text() == "1 bar");

    scene.removeItem(item);
    delete item;
  });
}
