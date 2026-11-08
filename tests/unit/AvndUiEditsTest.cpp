// The edits a custom avnd ui makes through `set`: in the document's history,
// one undo step per gesture, and none when the ui only reacts to a change.
// And how a ui's presentations lay its controls out.

#include <Process/Commands/SetControlValue.hpp>
#include <Process/Dataflow/NodeItem.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Avnd/Factories.hpp>
#include <Crousti/Layer.hpp>
#include <Crousti/ProcessModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/graphics/RectItem.hpp>
#include <score/graphics/widgets/QGraphicsTextButton.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <QApplication>
#include <QGraphicsScene>
#include <QWindow>
#include <qpa/qwindowsysteminterface.h>

#include <catch2/catch_test_macros.hpp>
#include <halp/controls.hpp>
#include <halp/custom_widgets.hpp>
#include <halp/layout.hpp>
#include <halp/meta.hpp>
#include <halp/soundfile_port.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <functional>

namespace uiedits_test
{
template <auto F>
struct settable : halp::control<F>
{
  std::function<void(typename halp::control<F>::control_value_type)> set;
};

//! What the ui does when a control changes, set by each test
inline std::function<void()> react;

struct Object
{
  halp_meta(name, "Ui edits test")
  halp_meta(c_name, "avnd_ui_edits_test")
  halp_meta(category, "Test")
  halp_meta(author, "ossia score")
  halp_meta(description, "A ui that edits its controls")
  halp_meta(uuid, "4d3c5f0e-7a1b-4e63-9c0d-2b8f61a7e5d4")

  struct ins
  {
    halp::hslider_f32<"A", halp::range{0., 1., 0.}> a;
    halp::hslider_f32<"B", halp::range{0., 1., 0.}> b;
  } inputs;

  struct outs
  {
    halp::hslider_f32<"Out", halp::range{0., 1., 0.}> out;
  } outputs;

  void operator()() { }

  struct ui
  {
    halp_meta(layout, halp::layouts::vbox)
    settable<&ins::a> a;
    settable<&ins::b> b;
    settable<&outs::out> out;

    void on_control_update()
    {
      if(react)
        react();
    }
  };
};

//! A ui laid out with presentations, on controls that are only shown
struct PresentationObject
{
  halp_meta(name, "Ui presentation test")
  halp_meta(c_name, "avnd_ui_presentation_test")
  halp_meta(category, "Test")
  halp_meta(author, "ossia score")
  halp_meta(description, "A ui showing its controls in several ways")
  halp_meta(uuid, "9b1e0c7a-58d2-4f3e-8c61-0a4d2e7f93b5")

  struct ins
  {
    halp::xy_pad_f32<"Pad", halp::xy_range{0., 2., {0.5, 1.5}}> pad;
    halp::xy_spinboxes_f32<"Spin", halp::xy_range{-1., 1., {0.25, -0.5}}> spin;
    halp::hslider_f32<"Hidden", halp::range{0., 1., 0.}> hidden;
    //! Shown as its waveform, unless asked small
    struct : halp::soundfile_port<"Sound">
    {
      halp_flag(waveform);
    } sound;
    halp::hslider_f32<"P", halp::range{0., 1., 0.}> p;
    halp::hslider_f32<"Q", halp::range{0., 1., 0.}> q;
  } inputs;

  struct outs
  {
    halp::hslider_f32<"Shown out", halp::range{0., 1., 0.}> shown;
    halp::hslider_f32<"Hidden out", halp::range{0., 1., 0.}> hidden;
  } outputs;

  void operator()() { }

  //! Edits P and Q as their own values, one transaction each; a third value
  //! and transaction have no control
  struct RawPair
  {
    static constexpr double width() { return 40.; }
    static constexpr double height() { return 20.; }
    std::tuple<float, float, float> values{};
    std::tuple<
        halp::transaction<float>, halp::transaction<float>, halp::transaction<float>>
        transactions;
    void paint(auto ctx) { }
  };

  struct ui
  {
    halp_meta(layout, halp::layouts::vbox)
    static constexpr halp::look none{.widget = halp::control_widget::none};
    halp::item<&ins::hidden, none> hidden;
    halp::item<&ins::sound, halp::look{.size = halp::control_size::compact}> sound;
    halp::item<&outs::shown> shown_out;
    halp::item<&outs::hidden, none> hidden_out;
    halp::custom_multi_control<RawPair, &ins::p, &ins::q> pair;
  };
};

//! A ui with an item filling its column, beside a wider one
struct FillObject
{
  halp_meta(name, "Ui fill test")
  halp_meta(c_name, "avnd_ui_fill_test")
  halp_meta(category, "Test")
  halp_meta(author, "ossia score")
  halp_meta(description, "A ui with an item as wide as its column")
  halp_meta(uuid, "c2f8e1d4-6a37-4b90-b5e2-71d0c9a3f468")

  struct ins
  {
    halp::hslider_f32<"A", halp::range{0., 1., 0.}> a;
  } inputs;
  struct
  {
  } outputs;
  void operator()() { }

  struct Filling
  {
    static constexpr double min_width = 50.;
    double w{min_width};
    double width() const noexcept { return w; }
    void set_width(double x) noexcept { w = std::max(x, min_width); }
    static constexpr double height() { return 10.; }
    void paint(auto ctx) { }
  };
  struct Wide
  {
    static constexpr double width() { return 200.; }
    static constexpr double height() { return 10.; }
    void paint(auto ctx) { }
  };

  struct ui
  {
    halp_meta(layout, halp::layouts::vbox)
    halp::custom_actions_item<Filling> fill;
    halp::custom_actions_item<Wide> wide;
  };
};

void spin()
{
  for(int i = 0; i < 3; i++)
    QApplication::processEvents();
}

float value(const Process::Port& p)
{
  if(auto in = qobject_cast<const Process::ControlInlet*>(&p))
    return ossia::convert<float>(in->value());
  return ossia::convert<float>(static_cast<const Process::ControlOutlet&>(p).value());
}

//! The process in the document's base interval, and its ui laid out in a scene
template <typename Obj>
struct Fixture
{
  using Model = oscr::ProcessModel<Obj>;
  using Root = oscr::RootItem<Obj, score::GraphicsVBoxLayout>;

  score::Document& doc;
  Scenario::IntervalModel& interval;
  Model* proc{};
  Process::DataflowManager dfm;
  FocusDispatcher fd;
  Process::Context pctx;
  QGraphicsScene scene;
  QGraphicsRectItem* parent{};
  Root* root{};

  explicit Fixture(score::Document& d)
      : doc{d}
      , interval{score::test::base_interval(d)}
      , pctx{d.context(), dfm, fd}
  {
    proc = new Model{
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{4242}, d.context(),
        &interval};
    interval.processes.add(proc);

    parent = new QGraphicsRectItem{QRectF{0., 0., 1000., 1000.}};
    scene.addItem(parent);
    oscr::LayerFactory<Obj> factory;
    root = dynamic_cast<Root*>(
        static_cast<Process::LayerFactory&>(factory).makeItem(*proc, pctx, parent));
    REQUIRE(root);
  }

  ~Fixture()
  {
    react = {};
    scene.removeItem(parent);
    delete parent;
    interval.processes.remove(proc);
  }

  Process::ControlInlet& inlet(int i)
  {
    return *safe_cast<Process::ControlInlet*>(proc->inlets()[i]);
  }
  Process::ControlOutlet& outlet(int i)
  {
    return *safe_cast<Process::ControlOutlet*>(proc->outlets()[i]);
  }
  Process::ControlInlet& a() { return inlet(0); }
  Process::ControlInlet& b() { return inlet(1); }
  Process::ControlOutlet& out() { return outlet(0); }
  auto& ui() { return root->ui; }
  int steps() { return doc.commandStack().size(); }

  //! The items laid out beside the item of \p port, in its control's box
  QList<QGraphicsItem*> controlItems(const Process::Port& port)
  {
    for(auto* item : scene.items())
      if(auto* p = dynamic_cast<Dataflow::PortItem*>(item); p && &p->port() == &port)
        return p->parentItem()->childItems();
    FAIL("no item for port " << port.name().toStdString());
    return {};
  }
};
}

using namespace uiedits_test;

TEST_CASE("A ui's edits in one event are one undo step", "[avnd][ui][command]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Fixture<Object> f{*doc};
    REQUIRE(f.ui().a.set);
    const int before = f.steps();

    f.ui().a.set(0.25f);
    f.ui().b.set(0.5f);
    f.ui().a.set(0.75f);
    // Applied at once, pushed once the event is over
    CHECK(value(f.a()) == 0.75f);
    CHECK(value(f.b()) == 0.5f);
    CHECK(f.steps() == before);
    spin();
    REQUIRE(f.steps() == before + 1);

    doc->commandStack().undo();
    CHECK(value(f.a()) == 0.f);
    CHECK(value(f.b()) == 0.f);
    doc->commandStack().redo();
    CHECK(value(f.a()) == 0.75f);
    CHECK(value(f.b()) == 0.5f);

    // A value the control already has is no edit
    f.ui().a.set(0.75f);
    spin();
    CHECK(f.steps() == before + 1);
  });
}

TEST_CASE("A ui reacting to a change does not edit", "[avnd][ui][command]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Fixture<Object> f{*doc};
    REQUIRE(f.ui().a.set);
    // A ui deriving A from B: the change of B may come from execution, or be
    // an undo or a redo, and none of them edits the document
    react = [&] { f.ui().a.set(value(f.b())); };
    const int before = f.steps();

    // Execution writing the control
    f.b().setValue(0.5f);
    spin();
    CHECK(value(f.a()) == 0.f);
    CHECK(f.steps() == before);

    // A user's edit of B, its undo and its redo
    CommandDispatcher<>{doc->context().commandStack}.submit<Process::SetControlValue>(
        f.b(), 0.25f);
    spin();
    CHECK(value(f.a()) == 0.f);
    CHECK(f.steps() == before + 1);
    doc->commandStack().undo();
    spin();
    doc->commandStack().redo();
    spin();
    CHECK(value(f.a()) == 0.f);
    CHECK(f.steps() == before + 1);
    CHECK(doc->commandStack().canRedo() == false);
  });
}

TEST_CASE("A command pushes the ui's pending edits first", "[avnd][ui][command]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Fixture<Object> f{*doc};
    REQUIRE(f.ui().a.set);
    const int before = f.steps();

    f.ui().a.set(0.25f);
    CommandDispatcher<>{doc->context().commandStack}.submit<Process::SetControlValue>(
        f.b(), 0.5f);
    spin();
    REQUIRE(f.steps() == before + 2);

    // Undone in the order they were made: B, then A
    doc->commandStack().undo();
    CHECK(value(f.a()) == 0.25f);
    CHECK(value(f.b()) == 0.f);
    doc->commandStack().undo();
    CHECK(value(f.a()) == 0.f);
  });
}

TEST_CASE("A ui's edits while a button is held are one undo step", "[avnd][ui][command]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Fixture<Object> f{*doc};
    REQUIRE(f.ui().a.set);
    const int before = f.steps();

    QWindow window;
    window.resize(100, 100);
    window.show();
    const QPointF pos{10., 10.};
    QWindowSystemInterface::handleMouseEvent(
        &window, pos, window.mapToGlobal(pos), Qt::LeftButton, Qt::LeftButton,
        QEvent::MouseButtonPress);
    QWindowSystemInterface::flushWindowSystemEvents();
    REQUIRE(QGuiApplication::mouseButtons() == Qt::LeftButton);

    // A drag: one edit per move
    for(float v : {0.1f, 0.2f, 0.3f})
    {
      f.ui().a.set(v);
      spin();
    }
    CHECK(value(f.a()) == 0.3f);
    CHECK(f.steps() == before);

    QWindowSystemInterface::handleMouseEvent(
        &window, pos, window.mapToGlobal(pos), Qt::NoButton, Qt::LeftButton,
        QEvent::MouseButtonRelease);
    QWindowSystemInterface::flushWindowSystemEvents();
    spin();
    REQUIRE(f.steps() == before + 1);
    doc->commandStack().undo();
    CHECK(value(f.a()) == 0.f);
  });
}

TEST_CASE("A ui edits a control outlet as one undo step", "[avnd][ui][command]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Fixture<Object> f{*doc};
    REQUIRE(f.ui().a.set);
    const int before = f.steps();

    f.ui().out.set(0.5f);
    f.ui().a.set(0.25f);
    spin();
    REQUIRE(f.steps() == before + 1);
    CHECK(value(f.out()) == 0.5f);
    doc->commandStack().undo();
    CHECK(value(f.out()) == 0.f);
    CHECK(value(f.a()) == 0.f);
  });
}

TEST_CASE("An xy control starts where its xy_range says", "[avnd][ui][xy]")
{
  using Ins = PresentationObject::ins;
  static_assert(requires { Ins{}.pad.value.x; });
  CHECK(Ins{}.pad.value.x == 0.5f);
  CHECK(Ins{}.pad.value.y == 1.5f);
  CHECK(Ins{}.spin.value.x == 0.25f);
  CHECK(Ins{}.spin.value.y == -0.5f);

  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Fixture<PresentationObject> f{*doc};

    auto* pad = qobject_cast<Process::XYSlider*>(&f.inlet(0));
    REQUIRE(pad);
    CHECK(pad->getMin() == ossia::vec2f{0.f, 0.f});
    CHECK(pad->getMax() == ossia::vec2f{2.f, 2.f});
    CHECK(ossia::convert<ossia::vec2f>(pad->init()) == ossia::vec2f{0.5f, 1.5f});

    auto* spin = qobject_cast<Process::XYSpinboxes*>(&f.inlet(1));
    REQUIRE(spin);
    CHECK(spin->getMin() == ossia::vec2f{-1.f, -1.f});
    CHECK(spin->getMax() == ossia::vec2f{1.f, 1.f});
    CHECK(ossia::convert<ossia::vec2f>(spin->init()) == ossia::vec2f{0.25f, -0.5f});
  });
}

TEST_CASE("A control shown as none is its port and label only", "[avnd][ui][layout]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Fixture<PresentationObject> f{*doc};

    const auto widgets = [&](const Process::Port& port) {
      int n = 0;
      for(auto* item : f.controlItems(port))
        if(!dynamic_cast<Dataflow::PortItem*>(item)
           && !dynamic_cast<score::SimpleTextItem*>(item)
           && !dynamic_cast<score::EmptyItem*>(item))
          n++;
      return n;
    };
    // An inlet and an outlet asked for none; another outlet as it is
    CHECK(widgets(f.inlet(2)) == 0);
    CHECK(widgets(f.outlet(1)) == 0);
    CHECK(widgets(f.outlet(0)) == 1);
  });
}

TEST_CASE("A small sound file is a file button", "[avnd][ui][layout]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Fixture<PresentationObject> f{*doc};

    auto* sound = qobject_cast<Process::AudioFileChooser*>(f.proc->inlets()[3]);
    REQUIRE(sound);
    int buttons = 0;
    for(auto* item : f.controlItems(*sound))
      if(dynamic_cast<score::QGraphicsTextButton*>(item))
        buttons++;
    CHECK(buttons == 1);
  });
}

TEST_CASE("A multi-control on raw values edits them as one undo step", "[avnd][ui][command]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Fixture<PresentationObject> f{*doc};
    auto& pair = f.ui().pair;
    auto& p = f.inlet(4);
    auto& q = f.inlet(5);
    const int before = f.steps();

    // Its values follow the ports, whatever changes them
    p.setValue(0.25f);
    CHECK(std::get<0>(pair.values) == 0.25f);

    auto& [tp, tq, none] = pair.transactions;
    REQUIRE(tp.start);
    REQUIRE(tq.commit);
    tp.start();
    tp.update(0.5f);
    tp.update(0.75f);
    tq.update(0.5f);
    CHECK(value(p) == 0.75f);
    CHECK(value(q) == 0.5f);
    tq.commit();
    REQUIRE(f.steps() == before + 1);
    doc->commandStack().undo();
    CHECK(value(p) == 0.25f);
    CHECK(value(q) == 0.f);

    // Rolled back: as before, with no step
    doc->commandStack().redo();
    tp.start();
    tp.update(0.1f);
    tp.rollback();
    CHECK(value(p) == 0.75f);
    CHECK(f.steps() == before + 1);

    // A transaction without a control does nothing
    REQUIRE(none.update);
    none.start();
    none.update(0.5f);
    none.commit();
    spin();
    CHECK(f.steps() == before + 1);

    // A command made during a gesture comes after it
    tp.start();
    tp.update(0.5f);
    CommandDispatcher<>{doc->context().commandStack}.submit<Process::SetControlValue>(
        q, 0.25f);
    tp.update(0.6f);
    tp.commit();
    spin();
    REQUIRE(f.steps() == before + 4);
    doc->commandStack().undo();
    CHECK(value(p) == 0.5f);
    CHECK(value(q) == 0.25f);
    doc->commandStack().undo();
    CHECK(value(q) == 0.5f);
    doc->commandStack().undo();
    CHECK(value(p) == 0.75f);
  });
}

TEST_CASE("An item filling its column stays as wide as it in the nodal view", "[avnd][ui][layout]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& layers = const_cast<Process::LayerFactoryList&>(
        ctx.interfaces<Process::LayerFactoryList>());
    layers.insert(std::make_unique<oscr::LayerFactory<FillObject>>());
    const_cast<Process::ProcessFactoryList&>(ctx.interfaces<Process::ProcessFactoryList>())
        .insert(std::make_unique<oscr::ProcessFactory<FillObject>>());

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& interval = score::test::base_interval(*doc);
    auto* proc = new oscr::ProcessModel<FillObject>{
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{4243}, doc->context(),
        &interval};
    interval.processes.add(proc);

    Process::DataflowManager dfm;
    FocusDispatcher fd;
    Process::Context pctx{doc->context(), dfm, fd};
    QGraphicsScene scene;
    auto* parent = new QGraphicsRectItem{QRectF{0., 0., 1000., 1000.}};
    scene.addItem(parent);
    auto* node = new Process::NodeItem{*proc, pctx, TimeVal::fromMsecs(1000.), parent};
    spin();

    score::FillsWidth* fill{};
    QGraphicsItem* fillItem{};
    for(auto* item : scene.items())
      if(auto f = dynamic_cast<score::FillsWidth*>(item))
      {
        fill = f;
        fillItem = item;
      }
    REQUIRE(fillItem);
    CHECK(fillItem->boundingRect().width() >= 200.);
    CHECK(fillItem->boundingRect().width() < 210.);

    // A child changing size lays the node out again, through its root: the
    // item stays as wide as the widest row, rather than at its smallest
    score::GraphicsLayout::relayoutAround(fillItem);
    spin();
    CHECK(fillItem->boundingRect().width() >= 200.);
    CHECK(fillItem->boundingRect().width() < 210.);

    (void)node;
    scene.removeItem(parent);
    delete parent;
    interval.processes.remove(proc);
  });
}

TEST_CASE("Pending ui edits and the history", "[avnd][ui][command]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Fixture<Object> f{*doc};
    auto& stack = doc->commandStack();
    CommandDispatcher<>{doc->context().commandStack}.submit<Process::SetControlValue>(
        f.b(), 0.5f);
    stack.markCurrentIndexAsSaved();
    const int before = f.steps();

    SECTION("they count as a change of the document")
    {
      f.ui().a.set(0.25f);
      CHECK(!stack.isAtSavedIndex());
      spin();
      CHECK(!stack.isAtSavedIndex());
      stack.undo();
      CHECK(stack.isAtSavedIndex());
    }
    SECTION("a redo pushes them, and has nothing left to redo")
    {
      stack.undo();
      REQUIRE(stack.canRedo());
      f.ui().a.set(0.25f);
      stack.redoQuiet();
      CHECK(!stack.canRedo());
      CHECK(value(f.a()) == 0.25f);
      CHECK(value(f.b()) == 0.f);
      CHECK(f.steps() == before);
    }
    SECTION("moving forward in the history pushes them, and stops")
    {
      stack.undo();
      f.ui().a.set(0.25f);
      stack.setIndex(stack.currentIndex() + 1);
      CHECK(!stack.canRedo());
      CHECK(value(f.a()) == 0.25f);
      CHECK(f.steps() == before);
    }
    SECTION("moving back in the history reaches the state it names")
    {
      // The index names a state of the history as it was: before the edit
      // of B. The pending edit is pushed after it, and undone too.
      const int target = stack.currentIndex() - 1;
      f.ui().a.set(0.25f);
      stack.setIndex(target);
      CHECK(stack.currentIndex() == target);
      CHECK(value(f.a()) == 0.f);
      CHECK(value(f.b()) == 0.f);
    }
  });
}
