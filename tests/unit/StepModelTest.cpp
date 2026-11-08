// The step sequencer's model: sequences, the Sequence port, the commands, and
// documents saved before sequences and the Duration port existed.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Process/ApplicationPlugin.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>

#include <Audio/Settings/Model.hpp>
#include <Media/Step/Commands.hpp>
#include <Media/Step/Model.hpp>

#include <score/graphics/RectItem.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QCoreApplication>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include <QPointer>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Media::Step::Model;

namespace
{
Model* reload(
    const score::GUIApplicationContext& ctx, score::Document& doc, QObject* owner,
    rapidjson::Document& jdoc)
{
  auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
  JSONWriter writer{jdoc};
  auto clone = deserialize_interface(pl, writer, doc.context(), owner);
  REQUIRE(clone);
  auto m = dynamic_cast<Model*>(clone);
  REQUIRE(m);
  return m;
}

//! The layer and the node item of a step sequencer, as the score view makes
//! them. The layer's steps are 50 pixels wide and 100 high; the node item is
//! 300 by 120, its steps sharing the width.
struct StepViews
{
  Process::DataflowManager dfm;
  FocusDispatcher focus;
  Process::Context ctx;
  QGraphicsScene scene;
  Process::LayerView* view{};
  Process::LayerPresenter* presenter{};
  QGraphicsItem* item{};

  StepViews(score::Document& doc, Model& m)
      : ctx{doc.context(), dfm, focus}
  {
    auto& layers = doc.context().app.interfaces<Process::LayerFactoryList>();
    auto* factory = layers.findDefaultFactory(m);
    REQUIRE(factory);
    view = factory->makeLayerView(m, ctx, nullptr);
    scene.addItem(view);
    presenter = factory->makeLayerPresenter(m, view, ctx, nullptr);
    presenter->setWidth(800., 800.);
    presenter->setHeight(100.);
    // A step of the default duration lasts 125 ms.
    presenter->on_zoomRatioChanged(TimeVal::fromMsecs(125. / 50.).impl);
    item = factory->makeItem(m, ctx, nullptr);
    REQUIRE(item);
    scene.addItem(item);
  }

  ~StepViews()
  {
    delete presenter;
    delete view;
    delete item;
  }
};

void sendMouse(
    QGraphicsScene& scene, QGraphicsItem& item, QEvent::Type t, QPointF pos,
    Qt::KeyboardModifiers mods = {}, Qt::MouseButton button = Qt::LeftButton)
{
  QGraphicsSceneMouseEvent ev{t};
  ev.setButton(t == QEvent::GraphicsSceneMouseMove ? Qt::NoButton : button);
  ev.setButtons(t == QEvent::GraphicsSceneMouseRelease ? Qt::NoButton : button);
  ev.setModifiers(mods);
  ev.setScenePos(item.mapToScene(pos));
  ev.setPos(pos);
  scene.sendEvent(&item, &ev);
}

//! Press at the first point, move through the others, release at the last.
void drag(
    QGraphicsScene& scene, QGraphicsItem& item, std::vector<QPointF> path,
    Qt::KeyboardModifiers mods = {})
{
  sendMouse(scene, item, QEvent::GraphicsSceneMousePress, path.front(), mods);
  for(auto p : path)
    sendMouse(scene, item, QEvent::GraphicsSceneMouseMove, p, mods);
  sendMouse(scene, item, QEvent::GraphicsSceneMouseRelease, path.back(), mods);
}

QAction* findAction(QMenu& menu, const QString& text)
{
  for(auto a : menu.actions())
    if(a->text() == text)
      return a;
  return nullptr;
}

rapidjson::Document save(const Model& m)
{
  const Process::ProcessModel& as_base = m;
  JSONReader reader;
  reader.readFrom(as_base);
  return toValue(reader);
}
}

TEST_CASE("step sequencer: the Sequence port selects, and grows the list", "[step]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Model m{TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{0}, nullptr};
    REQUIRE(m.inlets().size() == 3);
    REQUIRE(m.sequences().size() == 1);
    const auto first = m.steps();

    m.sequenceSelect->setValue(2);
    CHECK(m.currentSequence() == 2);
    REQUIRE(m.sequences().size() == 3);
    // New sequences have the length of the one shown
    CHECK(m.stepCount() == int(first.size()));

    // Counts are per sequence
    m.setStepCount(5);
    CHECK(m.stepCount() == 5);
    m.sequenceSelect->setValue(0);
    CHECK(m.steps() == first);

    m.sequenceSelect->setValue(-3);
    CHECK(m.currentSequence() == 0);
  });
}

TEST_CASE("step sequencer: a new count reaches whoever follows the steps", "[step]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Model m{TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{0}, nullptr};
    // Connected by name: the signal functions are inline, and the address of
    // the test executable's copy is not the plug-in's, which a pointer-to-member
    // connection compares against.
    auto* witness = new QObject;
    QPointer<QObject> alive{witness};
    REQUIRE(QObject::connect(&m, SIGNAL(stepsChanged()), witness, SLOT(deleteLater())));
    // The executor listens to stepsChanged, so a count change must emit it
    // along with stepCountChanged.
    m.setStepCount(12);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(!alive);
    delete alive.data();
  });
}

TEST_CASE("step sequencer: commands act on the sequence they were made on", "[step]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    auto* m = dynamic_cast<Model*>(score::test::add_process(
        *doc, QStringLiteral("c953fa55-65f0-4b93-8bfc-54780250d2b8"), {}));
    REQUIRE(m);

    m->sequenceSelect->setValue(1);
    const auto before1 = m->steps();
    Media::ChangeSteps edit{*m, ossia::float_vector{0.f, 0.f}};
    Media::SetStepCount count{*m, 3};
    edit.redo(dctx);
    CHECK(m->steps() == ossia::float_vector{0.f, 0.f});

    // Shown: sequence 0; undo still restores sequence 1.
    m->sequenceSelect->setValue(0);
    const auto steps0 = m->steps();
    edit.undo(dctx);
    CHECK(m->sequences()[1] == before1);
    CHECK(m->steps() == steps0);

    count.redo(dctx);
    CHECK(m->sequences()[1].size() == 3);
    CHECK(m->steps() == steps0);
    count.undo(dctx);
    CHECK(m->sequences()[1] == before1);
  });
}

TEST_CASE("step sequencer: sequences and ports round-trip", "[step]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QObject* owner = new QObject{&doc->model()};
    Model m{TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, owner};
    m.sequenceSelect->setValue(2);
    m.setSteps({0.1f, 0.2f, 0.3f});
    m.switchQuantification->setValue(4.);
    m.stepDuration->setValue(ossia::vec2f{0.125f, 1.f});
    m.setMin(-2.);

    auto jdoc = save(m);
    auto* r = reload(ctx, *doc, owner, jdoc);
    CHECK(r->sequences() == m.sequences());
    CHECK(r->currentSequence() == 2);
    CHECK(ossia::convert<int>(r->sequenceSelect->value()) == 2);
    CHECK(ossia::convert<float>(r->switchQuantification->value()) == 4.f);
    CHECK(r->stepDuration->value() == ossia::value{ossia::vec2f{0.125f, 1.f}});
    CHECK(r->min() == -2.);
    CHECK(r->outlet->id() == Id<Process::Port>{0});
    delete r;

    // DataStream too: undo/redo and copy/paste
    const Process::ProcessModel& as_base = m;
    auto bytes = score::marshall<DataStream>(as_base);
    DataStreamWriter w{bytes};
    auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
    auto* d = dynamic_cast<Model*>(deserialize_interface(pl, w, doc->context(), owner));
    REQUIRE(d);
    CHECK(d->sequences() == m.sequences());
    CHECK(d->stepDuration->value() == m.stepDuration->value());
    delete d;
  });
}

TEST_CASE("step sequencer: a document from before sequences plays as it did", "[step]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QObject* owner = new QObject{&doc->model()};
    Model m{TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, owner};

    // What a document saved by the previous release has: one list of steps,
    // a count and a duration in samples.
    auto jdoc = save(m);
    for(auto key : {"SequenceSelect", "SwitchQuantification", "StepDuration", "Sequences",
                    "Sequence"})
      if(jdoc.HasMember(key))
        jdoc.RemoveMember(key);
    auto& alloc = jdoc.GetAllocator();
    rapidjson::Value steps{rapidjson::kArrayType};
    for(float f : {0.25f, 0.5f, 0.75f})
      steps.PushBack(f, alloc);
    jdoc.AddMember("Steps", steps, alloc);
    jdoc.AddMember("StepCount", 3, alloc);
    jdoc.AddMember("StepDur", 24000, alloc);

    auto* r = reload(ctx, *doc, owner, jdoc);
    REQUIRE(r->sequences().size() == 1);
    CHECK(r->steps() == ossia::float_vector{0.25f, 0.5f, 0.75f});
    CHECK(r->currentSequence() == 0);
    REQUIRE(r->inlets().size() == 3);
    CHECK(ossia::convert<int>(r->sequenceSelect->value()) == 0);
    CHECK(ossia::convert<float>(r->switchQuantification->value()) == 1.f);

    // The same length, in seconds and not synced
    const double rate = ctx.settings<Audio::Settings::Model>().getRate();
    const auto dur = ossia::convert<ossia::vec2f>(r->stepDuration->value());
    CHECK(dur[0] == Catch::Approx(24000. / rate));
    CHECK(dur[1] == 0.f);
    CHECK(r->outlet->id() == Id<Process::Port>{0});
    delete r;
  });
}

TEST_CASE("step sequencer: new processes go from 0 to 1, saved ones keep theirs", "[step]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QObject* owner = new QObject{&doc->model()};
    Model m{TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, owner};
    CHECK(m.min() == 0.);
    CHECK(m.max() == 1.);

    // A document saved when the range started at -1.
    auto jdoc = save(m);
    jdoc["StepMin"] = -1.;
    auto* r = reload(ctx, *doc, owner, jdoc);
    CHECK(r->min() == -1.);
    CHECK(r->max() == 1.);
    delete r;

    m.setMin(-1.);
    const Process::ProcessModel& as_base = m;
    auto bytes = score::marshall<DataStream>(as_base);
    DataStreamWriter w{bytes};
    auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
    auto* d = dynamic_cast<Model*>(deserialize_interface(pl, w, doc->context(), owner));
    REQUIRE(d);
    CHECK(d->min() == -1.);
    delete d;
  });
}

TEST_CASE("step sequencer: Randomize in the menu is one undo step", "[step]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* m = dynamic_cast<Model*>(score::test::add_process(
        *doc, QStringLiteral("c953fa55-65f0-4b93-8bfc-54780250d2b8"), {}));
    REQUIRE(m);
    m->sequenceSelect->setValue(1);
    m->setSteps(ossia::float_vector(16, 0.5f));
    const auto other = m->sequences()[0];
    const auto before = m->steps();

    StepViews v{*doc, *m};
    auto& stack = doc->commandStack();
    const int commands = stack.currentIndex();

    QMenu menu;
    v.presenter->fillContextMenu(
        menu, {}, {},
        ctx.applicationPlugin<Process::ApplicationPlugin>().layerContextMenuRegistrar());
    auto* randomize = findAction(menu, QObject::tr("Randomize"));
    REQUIRE(randomize);
    randomize->trigger();

    REQUIRE(stack.currentIndex() == commands + 1);
    const auto random = m->steps();
    REQUIRE(random.size() == before.size());
    CHECK(random != before);
    for(float f : random)
    {
      CHECK(f >= 0.f);
      CHECK(f <= 1.f);
    }
    CHECK(m->sequences()[0] == other);

    stack.undo();
    CHECK(m->steps() == before);
    stack.redo();
    // The values drawn, not new ones.
    CHECK(m->steps() == random);
    CHECK(m->sequences()[0] == other);
  });
}

TEST_CASE("step sequencer: mouse gestures on the bars, one undo step each", "[step]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* m = dynamic_cast<Model*>(score::test::add_process(
        *doc, QStringLiteral("c953fa55-65f0-4b93-8bfc-54780250d2b8"), {}));
    REQUIRE(m);
    const ossia::float_vector start{0.5f, 0.5f, 0.5f, 0.5f, 0.2f, 0.9f, 0.5f, 0.5f};
    m->setSteps(start);

    StepViews v{*doc, *m};
    auto& stack = doc->commandStack();

    struct Surface
    {
      QGraphicsItem* item;
      double stepWidth;
      double height;
    };
    for(auto [item, stepWidth, height] :
        {Surface{v.view, 50., 100.}, Surface{v.item, 300. / 8., 120.}})
    {
      INFO((item == v.view ? "layer" : "node"));
      m->setSteps(start);
      const auto x = [w = stepWidth](int step) { return (step + 0.5) * w; };

      {
        // A plain drag sets the bars it crosses to where the mouse is.
        const int commands = stack.currentIndex();
        drag(
            v.scene, *item,
            {{x(1), 0.25 * height}, {x(2), 0.75 * height}, {x(3), 0.75 * height}});
        CHECK(stack.currentIndex() == commands + 1);
        CHECK(m->steps()[0] == start[0]);
        CHECK(m->steps()[1] == Catch::Approx(0.25f));
        CHECK(m->steps()[2] == Catch::Approx(0.75f));
        CHECK(m->steps()[3] == Catch::Approx(0.75f));
        CHECK(m->steps()[4] == start[4]);
        stack.undo();
        CHECK(m->steps() == start);
      }

      {
        // Shift moves every bar by how far the mouse went: up a tenth of the
        // height draws every bar a tenth higher, 0 being the top.
        const int commands = stack.currentIndex();
        drag(
            v.scene, *item, {{x(0), 0.5 * height}, {x(3), 0.45 * height}, {x(5), 0.4 * height}},
            Qt::ShiftModifier);
        CHECK(stack.currentIndex() == commands + 1);
        for(std::size_t i = 0; i < start.size(); i++)
          CHECK(m->steps()[i] == Catch::Approx(std::clamp(start[i] - 0.1f, 0.f, 1.f)));

        // Down past the bottom: clamped.
        stack.undo();
        drag(v.scene, *item, {{x(0), 0.}, {x(0), height}}, Qt::ShiftModifier);
        CHECK(stack.currentIndex() == commands + 1);
        for(float f : m->steps())
          CHECK(f == 1.f);
        stack.undo();
        CHECK(m->steps() == start);
      }

      {
        // Alt blends towards random values: not at all where it pressed, all
        // the way once far enough.
        const int commands = stack.currentIndex();
        sendMouse(
            v.scene, *item, QEvent::GraphicsSceneMousePress, {x(0), 0.}, Qt::AltModifier);
        sendMouse(
            v.scene, *item, QEvent::GraphicsSceneMouseMove, {x(0), 0.}, Qt::AltModifier);
        CHECK(m->steps() == start);
        sendMouse(
            v.scene, *item, QEvent::GraphicsSceneMouseMove, {x(0), 2. * height},
            Qt::AltModifier);
        const auto far = m->steps();
        CHECK(far != start);
        for(float f : far)
        {
          CHECK(f >= 0.f);
          CHECK(f <= 1.f);
        }
        // Halfway back: halfway between.
        sendMouse(
            v.scene, *item, QEvent::GraphicsSceneMouseMove,
            {x(0), std::max(80., height) / 2.}, Qt::AltModifier);
        for(std::size_t i = 0; i < start.size(); i++)
          CHECK(m->steps()[i] == Catch::Approx((start[i] + far[i]) / 2.f).margin(1e-5));
        sendMouse(
            v.scene, *item, QEvent::GraphicsSceneMouseRelease,
            {x(0), std::max(80., height) / 2.}, Qt::AltModifier);
        CHECK(stack.currentIndex() == commands + 1);
        stack.undo();
        CHECK(m->steps() == start);
      }

      {
        // A modifier press that does not move leaves nothing to undo.
        const int commands = stack.currentIndex();
        drag(v.scene, *item, {{x(2), 10.}}, Qt::ShiftModifier);
        drag(v.scene, *item, {{x(2), 10.}}, Qt::AltModifier);
        CHECK(stack.currentIndex() == commands);
        CHECK(m->steps() == start);
      }
    }

    {
      // The right button opens the menu, and edits nothing.
      const int commands = stack.currentIndex();
      sendMouse(
          v.scene, *v.view, QEvent::GraphicsSceneMousePress, {25., 10.}, {},
          Qt::RightButton);
      sendMouse(
          v.scene, *v.view, QEvent::GraphicsSceneMouseRelease, {25., 10.}, {},
          Qt::RightButton);
      CHECK(stack.currentIndex() == commands);
      CHECK(m->steps() == start);
    }
  });
}
