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

#include <Audio/Settings/Model.hpp>
#include <Avnd/Factories.hpp>
#include <Crousti/ExecutorUpdateControlValueInUi.hpp>
#include <Crousti/Layer.hpp>
#include <Crousti/ProcessModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/graphics/RectItem.hpp>
#include <score/graphics/TextItem.hpp>
#include <score/graphics/layouts/GraphicsBoxLayout.hpp>
#include <score/graphics/layouts/GraphicsGridLayout.hpp>
#include <score/graphics/layouts/GraphicsSplitLayout.hpp>
#include <score/graphics/layouts/GraphicsStripDetailLayout.hpp>
#include <score/graphics/layouts/GraphicsTabLayout.hpp>
#include <score/graphics/widgets/QGraphicsCombo.hpp>
#include <score/graphics/widgets/QGraphicsTextButton.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFocusEvent>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QTimer>
#include <QWheelEvent>
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
#include <algorithm>
#include <array>
#include <memory>
#include <set>
#include <string_view>
#include <vector>

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

//! Two independently editable regions, with the same raw transaction contract
//! as the EQ diagram. The host, not the widget, decides when a wheel ends.
struct WheelObject
{
  halp_meta(name, "Ui wheel test")
  halp_meta(c_name, "avnd_ui_wheel_test")
  halp_meta(category, "Test")
  halp_meta(author, "ossia score")
  halp_meta(description, "Two wheel and drag targets")
  halp_meta(uuid, "8f2945b6-1cb9-4e76-91e5-f3d200cd872a")

  struct ins
  {
    halp::hslider_f32<"A", halp::range{0., 1., 0.}> a;
    halp::hslider_f32<"B", halp::range{0., 1., 0.}> b;
  } inputs;
  struct { } outputs;
  void operator()() { }

  //! The pads alive: a host calling one that is gone counts it, as its
  //! storage outlives it in the root item, where reading it would pass.
  static inline std::set<const void*> livePads;
  static inline int callsAfterDestruction{};

  struct Pad
  {
    Pad() { livePads.insert(this); }
    Pad(const Pad& other)
        : values{other.values}
    {
      livePads.insert(this);
    }
    Pad& operator=(const Pad&) = default;
    ~Pad() { livePads.erase(this); }

    static constexpr double width() { return 160.; }
    static constexpr double height() { return 60.; }
    std::tuple<float, float> values{};
    std::tuple<halp::transaction<float>, halp::transaction<float>> transactions;
    int wheelTarget{-1};
    int dragTarget{-1};
    bool dragging{};
    void paint(auto ctx) { }

    int wheel_target(double x, double y) const
    {
      if(x < 0. || x >= width() || y < 0. || y >= height() - 10.)
        return -1;
      return x < width() / 2. ? 0 : 1;
    }

    auto& transaction(int target)
    {
      return target == 0 ? std::get<0>(transactions) : std::get<1>(transactions);
    }

    float current(int target) const
    {
      return target == 0 ? std::get<0>(values) : std::get<1>(values);
    }

    bool wheel(double x, double y, double delta)
    {
      const int target = wheel_target(x, y);
      if(target < 0 || delta == 0.)
        return false;
      const float next = std::clamp(current(target) + float(delta / 960.), 0.f, 1.f);
      if(next != current(target))
      {
        if(wheelTarget < 0)
        {
          transaction(target).start();
          wheelTarget = target;
        }
        transaction(target).update(next);
      }
      return true;
    }

    void finish_wheel()
    {
      if(!livePads.contains(this))
      {
        ++callsAfterDestruction;
        return;
      }
      if(wheelTarget >= 0)
      {
        transaction(wheelTarget).commit();
        wheelTarget = -1;
      }
    }

    bool mouse_press(double x, double y)
    {
      dragTarget = wheel_target(x, y);
      return dragTarget >= 0;
    }

    void mouse_move(double x, double y)
    {
      if(dragTarget < 0)
        return;
      const float next = std::clamp(float(y / 64.), 0.f, 1.f);
      if(next == current(dragTarget))
        return;
      if(!dragging)
      {
        transaction(dragTarget).start();
        dragging = true;
      }
      transaction(dragTarget).update(next);
    }

    void mouse_release(double x, double y)
    {
      if(dragging)
        transaction(dragTarget).commit();
      dragging = false;
      dragTarget = -1;
    }
  };
  struct OtherPad : Pad { };

  struct ui
  {
    halp_meta(layout, halp::layouts::vbox)
    halp::custom_multi_control<Pad, &ins::a, &ins::b> pad;
    halp::custom_multi_control<OtherPad, &ins::a, &ins::b> other;
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

//! Runs the event loop until \p done, for at most two seconds
template <typename F>
bool waitUntil(F done)
{
  QElapsedTimer elapsed;
  elapsed.start();
  while(!done() && elapsed.elapsed() < 2000)
  {
    QEventLoop loop;
    QTimer::singleShot(5, &loop, &QEventLoop::quit);
    loop.exec();
  }
  return done();
}

//! Whether a wheel gesture waits for its idle deadline
bool wheelIdlePending()
{
  auto* gesture = oscr::WheelGesture::active();
  return gesture && gesture->idleTimer().isActive();
}

//! What the idle deadline of the wheel gesture does once it passes, now
bool wheelIdleTimeout()
{
  auto* gesture = oscr::WheelGesture::active();
  if(!gesture || !gesture->idleTimer().isActive())
    return false;
  auto& timer = gesture->idleTimer();
  timer.stop();
  return QMetaObject::invokeMethod(&timer, "timeout", Qt::DirectConnection);
}

struct WheelFixture : Fixture<WheelObject>
{
  std::unique_ptr<QGraphicsView> viewOwner{std::make_unique<QGraphicsView>(&scene)};
  QGraphicsView& view{*viewOwner};
  QGraphicsItem* pad{};
  QGraphicsItem* other{};

  explicit WheelFixture(score::Document& d)
      : Fixture{d}
  {
    using Item = decltype(ui().pad);
    using OtherItem = decltype(ui().other);
    for(auto* item : scene.items())
    {
      if(dynamic_cast<oscr::CustomMultiControl<Item&>*>(item))
        pad = item;
      else if(dynamic_cast<oscr::CustomMultiControl<OtherItem&>*>(item))
        other = item;
    }
    REQUIRE(pad);
    REQUIRE(other);
    view.setSceneRect(root->sceneBoundingRect());
    view.resize(480, 320);
    view.show();
    view.activateWindow();
    view.setFocus();
    spin();
    REQUIRE(view.windowHandle());
    QWindowSystemInterface::handleFocusWindowChanged(view.windowHandle());
    QWindowSystemInterface::flushWindowSystemEvents();
    spin();
  }

  QPoint position(QGraphicsItem* item, QPointF local) const
  {
    return view.mapFromScene(item->mapToScene(local));
  }

  void wheelAt(
      QGraphicsItem* item, QPointF local, int delta,
      Qt::ScrollPhase phase = Qt::NoScrollPhase)
  {
    const auto pos = position(item, local);
    REQUIRE(view.viewport()->rect().contains(pos));
    QWheelEvent event{
        QPointF{pos}, QPointF{view.viewport()->mapToGlobal(pos)}, QPoint{},
        QPoint{0, delta}, Qt::NoButton, Qt::NoModifier, phase, false};
    QApplication::sendEvent(view.viewport(), &event);
  }

  void wheel(int target, int delta = 120, Qt::ScrollPhase phase = Qt::NoScrollPhase)
  {
    wheelAt(pad, QPointF{target == 0 ? 40. : 120., 20.}, delta, phase);
  }

  void mouse(QEvent::Type type, QPointF local, Qt::MouseButtons buttons,
             Qt::MouseButton button)
  {
    const auto pos = view.viewport()->mapTo(&view, position(pad, local));
    QWindowSystemInterface::handleMouseEvent(
        view.windowHandle(), QPointF{pos}, QPointF{view.mapToGlobal(pos)},
        buttons, button, type);
    QWindowSystemInterface::flushWindowSystemEvents();
  }

  void key(int key, Qt::KeyboardModifiers modifiers)
  {
    QWindowSystemInterface::handleKeyEvent(
        view.windowHandle(), QEvent::KeyPress, key, modifiers);
    QWindowSystemInterface::handleKeyEvent(
        view.windowHandle(), QEvent::KeyRelease, key, modifiers);
    QWindowSystemInterface::flushWindowSystemEvents();
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

TEST_CASE("Native wheel bursts become one undo step each", "[avnd][ui][command][wheel]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    WheelFixture f{*doc};
    auto& stack = doc->commandStack();
    const int before = f.steps();

    f.wheel(0);
    CHECK(value(f.a()) == 0.125f);
    CHECK(f.steps() == before);
    REQUIRE(wheelIdlePending());
    CHECK(oscr::WheelGesture::active()->idleTimer().interval() == 250);
    // A deadline that passed: the next event of the burst starts it again
    oscr::WheelGesture::active()->idleTimer().stop();
    f.wheel(0);
    CHECK(value(f.a()) == 0.25f);
    CHECK(wheelIdlePending());
    CHECK(f.steps() == before);
    f.wheel(0);
    CHECK(value(f.a()) == 0.375f);
    CHECK(f.steps() == before);
    REQUIRE(wheelIdleTimeout());
    REQUIRE(f.steps() == before + 1);
    stack.undo();
    CHECK(value(f.a()) == 0.f);
    stack.redo();
    CHECK(value(f.a()) == 0.375f);

    f.wheel(0);
    CHECK(value(f.a()) == 0.5f);
    REQUIRE(wheelIdleTimeout());
    REQUIRE(f.steps() == before + 2);
    stack.undo();
    CHECK(value(f.a()) == 0.375f);
    stack.undo();
    CHECK(value(f.a()) == 0.f);
    stack.redo();
    stack.redo();
    CHECK(value(f.a()) == 0.5f);
    CHECK(value(f.b()) == 0.f);
  });
}

TEST_CASE("Native wheel gestures separate targets and items", "[avnd][ui][command][wheel]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    WheelFixture f{*doc};
    auto& stack = doc->commandStack();
    const int before = f.steps();

    f.wheel(0);
    f.wheel(0);
    f.wheel(1);
    CHECK(value(f.a()) == 0.25f);
    CHECK(value(f.b()) == 0.125f);
    REQUIRE(f.steps() == before + 1);

    // Same control and target number, but a different custom item.
    f.wheelAt(f.other, {120., 20.}, 120);
    CHECK(value(f.b()) == 0.25f);
    REQUIRE(f.steps() == before + 2);
    REQUIRE(wheelIdleTimeout());
    REQUIRE(f.steps() == before + 3);
    stack.undo();
    CHECK(value(f.a()) == 0.25f);
    CHECK(value(f.b()) == 0.125f);
    stack.undo();
    CHECK(value(f.b()) == 0.f);
    stack.undo();
    CHECK(value(f.a()) == 0.f);
    stack.redo();
    stack.redo();
    stack.redo();
    CHECK(value(f.a()) == 0.25f);
    CHECK(value(f.b()) == 0.25f);
  });
}

TEST_CASE("Native scroll phases determine the undo boundary", "[avnd][ui][command][wheel]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    WheelFixture f{*doc};
    auto& stack = doc->commandStack();
    const int before = f.steps();

    SECTION("update and momentum survive a pause until a zero-delta end")
    {
      f.wheel(0, 0, Qt::ScrollBegin);
      CHECK(f.steps() == before);
      f.wheel(0, 120, Qt::ScrollUpdate);
      CHECK(value(f.a()) == 0.125f);
      // A pause: phases, not the idle deadline, end the gesture
      CHECK(!wheelIdlePending());
      CHECK(f.steps() == before);
      CHECK(!stack.canUndo());
      f.wheel(0, 120, Qt::ScrollUpdate);
      f.wheel(0, 120, Qt::ScrollMomentum);
      CHECK(value(f.a()) == 0.375f);
      CHECK(f.steps() == before);
      // End over an unsupported part of the same widget, without a delta.
      f.wheelAt(f.pad, {40., 55.}, 0, Qt::ScrollEnd);
      REQUIRE(f.steps() == before + 1);
      stack.undo();
      CHECK(value(f.a()) == 0.f);
      stack.redo();
      CHECK(value(f.a()) == 0.375f);
      CHECK(!oscr::WheelGesture::active());
      CHECK(f.steps() == before + 1);
    }

    SECTION("a new begin ends the previous gesture even on the same target")
    {
      f.wheel(0, 120, Qt::ScrollBegin);
      f.wheel(0, 120, Qt::ScrollUpdate);
      f.wheel(0, 0, Qt::ScrollBegin);
      REQUIRE(f.steps() == before + 1);
      f.wheel(0, 120, Qt::ScrollUpdate);
      f.wheel(0, 0, Qt::ScrollEnd);
      REQUIRE(f.steps() == before + 2);
      CHECK(value(f.a()) == 0.375f);
      stack.undo();
      CHECK(value(f.a()) == 0.25f);
      stack.undo();
      CHECK(value(f.a()) == 0.f);
      stack.redo();
      stack.redo();
      CHECK(value(f.a()) == 0.375f);
    }

    SECTION("an end carrying a final delta belongs to the same gesture")
    {
      f.wheel(0, 120, Qt::ScrollBegin);
      f.wheel(0, 120, Qt::ScrollUpdate);
      f.wheel(0, 120, Qt::ScrollEnd);
      CHECK(value(f.a()) == 0.375f);
      REQUIRE(f.steps() == before + 1);
      stack.undo();
      CHECK(value(f.a()) == 0.f);
      stack.redo();
      CHECK(value(f.a()) == 0.375f);
      CHECK(!oscr::WheelGesture::active());
      CHECK(f.steps() == before + 1);
    }
  });
}

TEST_CASE("Native no-op wheel events do not edit history", "[avnd][ui][command][wheel]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    WheelFixture f{*doc};
    auto& stack = doc->commandStack();
    const int before = f.steps();

    f.wheel(0, 0);
    f.wheel(0, -120);
    f.wheelAt(f.pad, {40., 55.}, 120);
    wheelIdleTimeout();
    CHECK(value(f.a()) == 0.f);
    CHECK(f.steps() == before);
    CHECK(!stack.canUndo());

    f.wheel(0, 0, Qt::ScrollBegin);
    f.wheel(0, -120, Qt::ScrollUpdate);
    f.wheel(0, 0, Qt::ScrollEnd);
    CHECK(f.steps() == before);

    f.wheel(0, 1920, Qt::ScrollBegin);
    CHECK(value(f.a()) == 1.f);
    f.wheel(0, 120, Qt::ScrollUpdate);
    f.wheel(0, 0, Qt::ScrollEnd);
    REQUIRE(f.steps() == before + 1);
    f.wheel(0);
    wheelIdleTimeout();
    CHECK(value(f.a()) == 1.f);
    REQUIRE(f.steps() == before + 1);
    stack.undo();
    CHECK(value(f.a()) == 0.f);
    stack.redo();
    CHECK(value(f.a()) == 1.f);
  });
}

TEST_CASE("Native wheel edits finish on input and lifetime interruptions", "[avnd][ui][command][wheel]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    WheelFixture f{*doc};
    auto& stack = doc->commandStack();
    const int before = f.steps();
    WheelObject::callsAfterDestruction = 0;

    f.wheel(0);
    CHECK(value(f.a()) == 0.125f);
    CHECK(f.steps() == before);

    SECTION("a click finishes the wheel without adding an edit")
    {
      f.mouse(QEvent::MouseButtonPress, {40., 20.}, Qt::LeftButton, Qt::LeftButton);
      CHECK(f.steps() == before + 1);
      f.mouse(QEvent::MouseButtonRelease, {40., 20.}, Qt::NoButton, Qt::LeftButton);
    }

    SECTION("a drag is a separate transaction after the wheel")
    {
      f.mouse(QEvent::MouseButtonPress, {40., 20.}, Qt::LeftButton, Qt::LeftButton);
      CHECK(f.steps() == before + 1);
      f.mouse(QEvent::MouseMove, {40., 32.}, Qt::LeftButton, Qt::NoButton);
      CHECK(value(f.a()) == 0.5f);
      CHECK(f.steps() == before + 1);
      f.mouse(QEvent::MouseButtonRelease, {40., 32.}, Qt::NoButton, Qt::LeftButton);
      REQUIRE(f.steps() == before + 2);
      stack.undo();
      CHECK(value(f.a()) == 0.125f);
      stack.redo();
      CHECK(value(f.a()) == 0.5f);
      stack.undo();
      stack.undo();
      CHECK(value(f.a()) == 0.f);
      stack.redo();
      stack.redo();
      CHECK(value(f.a()) == 0.5f);
      CHECK(!oscr::WheelGesture::active());
      CHECK(f.steps() == before + 2);
      return;
    }

    SECTION("a context menu finishes the wheel")
    {
      const auto pos = f.position(f.pad, {40., 20.});
      QContextMenuEvent menu{
          QContextMenuEvent::Mouse, pos, f.view.viewport()->mapToGlobal(pos)};
      QApplication::sendEvent(f.view.viewport(), &menu);
    }

    SECTION("focus loss commits before the idle deadline")
    {
      QFocusEvent lost{QEvent::FocusOut, Qt::OtherFocusReason};
      QApplication::sendEvent(f.view.viewport(), &lost);
    }

    SECTION("native window deactivation commits before the idle deadline")
    {
      QWindowSystemInterface::handleFocusWindowChanged(nullptr);
      QWindowSystemInterface::flushWindowSystemEvents();
    }

    SECTION("deleting the custom item commits while its bindings are live")
    {
      delete f.root;
      f.root = nullptr;
      f.pad = nullptr;
      f.other = nullptr;
    }

    SECTION("deleting the view finishes the wheel with the controls still alive")
    {
      f.viewOwner.reset();
    }

    REQUIRE(f.steps() == before + 1);
    stack.undo();
    CHECK(value(f.a()) == 0.f);
    stack.redo();
    CHECK(value(f.a()) == 0.125f);
    CHECK(!oscr::WheelGesture::active());
    CHECK(f.steps() == before + 1);
    CHECK(WheelObject::callsAfterDestruction == 0);
  });
}

TEST_CASE("Native undo shortcuts finish a pending wheel before action dispatch", "[avnd][ui][command][wheel]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    WheelFixture f{*doc};
    auto& stack = doc->commandStack();
    const int before = f.steps();
    const int index = stack.currentIndex();
    QAction undo{&f.view};
    QAction redo{&f.view};
    undo.setShortcut(QKeySequence{Qt::CTRL | Qt::Key_Z});
    redo.setShortcut(QKeySequence{Qt::CTRL | Qt::SHIFT | Qt::Key_Z});
    undo.setShortcutContext(Qt::WidgetWithChildrenShortcut);
    redo.setShortcutContext(Qt::WidgetWithChildrenShortcut);
    f.view.addAction(&undo);
    f.view.addAction(&redo);
    undo.setEnabled(stack.canUndo());
    redo.setEnabled(stack.canRedo());
    QObject::connect(&stack, &score::CommandStack::canUndoChanged, &undo, &QAction::setEnabled);
    QObject::connect(&stack, &score::CommandStack::canRedoChanged, &redo, &QAction::setEnabled);
    int undos = 0;
    int redos = 0;
    QObject::connect(&undo, &QAction::triggered, &f.view, [&] {
      ++undos;
      stack.undo();
    });
    QObject::connect(&redo, &QAction::triggered, &f.view, [&] {
      ++redos;
      stack.redo();
    });

    f.wheel(0);
    f.wheel(0);
    CHECK(value(f.a()) == 0.25f);
    REQUIRE(f.steps() == before);
    REQUIRE(!undo.isEnabled());
    REQUIRE(!redo.isEnabled());
    // No event-loop wait or manual ShortcutOverride: Qt must enable the action
    // while processing the native key, before it resolves the shortcut.
    f.key(Qt::Key_Z, Qt::ControlModifier);
    REQUIRE(undos == 1);
    REQUIRE(f.steps() == before + 1);
    CHECK(stack.currentIndex() == index);
    CHECK(value(f.a()) == 0.f);
    REQUIRE(redo.isEnabled());
    f.key(Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    REQUIRE(redos == 1);
    CHECK(stack.currentIndex() == index + 1);
    CHECK(value(f.a()) == 0.25f);
    CHECK(undo.isEnabled());
    CHECK(!redo.isEnabled());
    CHECK(!oscr::WheelGesture::active());
    CHECK(f.steps() == before + 1);
    CHECK(undos == 1);
    CHECK(redos == 1);
  });
}

namespace uiedits_test
{
//! A filter whose model decides which responses there are, and which other
//! controls it has: the native controls follow the ports they depend on.
struct DependentObject
{
  halp_meta(name, "Ui dependent test")
  halp_meta(c_name, "avnd_ui_dependent_test")
  halp_meta(category, "Test")
  halp_meta(author, "ossia score")
  halp_meta(description, "Native controls depending on other ports")
  halp_meta(uuid, "0f6a2d1e-93c4-4b7a-a5e8-6d21c0b4f9a3")

  struct Model
  {
    static constexpr auto name() { return "Model"; }
    enum widget { combobox };
    struct range
    {
      std::array<std::string_view, 3> values{"Full", "Narrow", "Single"};
      int init = 0;
    };
    int value = 0;
  };
  struct Response
  {
    static constexpr auto name() { return "Response"; }
    enum widget { combobox };
    struct range
    {
      std::array<std::string_view, 3> values{"Low", "Band", "High"};
      int init = 0;
    };
    int value = 0;
  };

  struct ins
  {
    Model model;
    Response response;
    halp::hslider_f32<"Amount", halp::range{0., 1., 0.}> amount;
  } inputs;
  struct { } outputs;
  void operator()() { }

  struct Choice
  {
    int value;
    std::string_view name;
  };
  using Config = std::tuple<int, int>;
  static constexpr std::array<std::string_view, 3> responseNames{"Low", "Band", "High"};

  //! Full has every response, Narrow two, Single only Low
  static std::vector<int> responsesOf(int model)
  {
    if(model == 0)
      return {0, 1, 2};
    if(model == 1)
      return {1, 2};
    return {0};
  }
  //! A response the model lacks becomes its first one
  static Config resolve(Config v)
  {
    auto [model, response] = v;
    const auto responses = responsesOf(model);
    if(std::ranges::find(responses, response) == responses.end())
      response = responses.front();
    return {model, response};
  }

  struct ui
  {
    halp_meta(layout, halp::layouts::vbox)
    struct : halp::item<&ins::model>
    {
      static constexpr auto dependent_models()
      {
        return std::tuple{&ins::model, &ins::response};
      }
      static constexpr bool dependent_choices() { return true; }
      static bool visible(const Config&) { return true; }
      static std::string_view control_label(const Config&) { return "Model"; }
      static std::vector<Choice> control_choices(const Config&)
      {
        return {{0, "Full"}, {1, "Narrow"}, {2, "Single"}};
      }
      static int selected(const Config& v) { return std::get<0>(v); }
      static Config edited(const Config& v, int value)
      {
        return resolve({value, std::get<1>(v)});
      }
    } model;
    struct : halp::item<&ins::response>
    {
      static constexpr auto dependent_models()
      {
        return std::tuple{&ins::model, &ins::response};
      }
      static constexpr bool dependent_choices() { return true; }
      static bool visible(const Config& v)
      {
        return responsesOf(std::get<0>(v)).size() > 1;
      }
      static std::string_view control_label(const Config&) { return "Response"; }
      static std::vector<Choice> control_choices(const Config& v)
      {
        std::vector<Choice> choices;
        for(int r : responsesOf(std::get<0>(v)))
          choices.push_back({r, responseNames[r]});
        return choices;
      }
      //! As the port has it, listed or not
      static int selected(const Config& v) { return std::get<1>(v); }
      static Config edited(const Config& v, int value)
      {
        return resolve({std::get<0>(v), value});
      }
    } response;
    struct : halp::item<&ins::amount>
    {
      static constexpr auto dependent_models() { return std::tuple{&ins::model}; }
      static constexpr bool dependent_choices() { return false; }
      static bool visible(const std::tuple<int>& v) { return std::get<0>(v) != 1; }
      static std::string_view control_label(const std::tuple<int>& v)
      {
        return std::get<0>(v) == 2 ? "Width" : "Amount";
      }
    } amount;
  };
};

//! A custom item that polls for work it was given, and takes double clicks
struct PollObject
{
  halp_meta(name, "Ui poll test")
  halp_meta(c_name, "avnd_ui_poll_test")
  halp_meta(category, "Test")
  halp_meta(author, "ossia score")
  halp_meta(description, "A custom item polling for results")
  halp_meta(uuid, "5b8e3c47-1d2a-4f90-8e6b-a7c3f21d0e58")

  struct ins
  {
    halp::hslider_f32<"A", halp::range{0., 1., 0.}> a;
  } inputs;
  struct { } outputs;
  void operator()() { }

  struct Poller
  {
    static constexpr double width() { return 40.; }
    static constexpr double height() { return 20.; }
    std::tuple<float> values{};
    std::tuple<halp::transaction<float>> transactions;
    std::function<void()> request_poll;
    //! Given by the host
    double sample_rate = 0.;
    //! Results ready, results taken, and calls of poll
    int pending{};
    int taken{};
    int polls{};
    std::vector<double> doubleClicks;
    void paint(auto ctx) { }

    bool poll()
    {
      ++polls;
      taken += std::exchange(pending, 0);
      return false;
    }

    //! Only the left half takes them
    bool mouse_double_click(double x, double y)
    {
      doubleClicks.push_back(x);
      return x < width() / 2.;
    }
  };

  struct ui
  {
    halp_meta(layout, halp::layouts::vbox)
    halp::custom_multi_control<Poller, &ins::a> poller;
  };
};

//! A custom item and tabs sharing a named selection
struct SelectionObject
{
  halp_meta(name, "Ui selection test")
  halp_meta(c_name, "avnd_ui_selection_test")
  halp_meta(category, "Test")
  halp_meta(author, "ossia score")
  halp_meta(description, "A custom item selecting the page of its tabs")
  halp_meta(uuid, "c4a1e9f2-6b3d-4e85-9a07-3f5d8b2c1e64")

  struct ins
  {
    halp::hslider_f32<"A", halp::range{0., 1., 0.}> a;
    halp::hslider_f32<"B", halp::range{0., 1., 0.}> b;
  } inputs;
  struct { } outputs;
  void operator()() { }

  struct Picker
  {
    static constexpr double width() { return 40.; }
    static constexpr double height() { return 20.; }
    std::tuple<float> values{};
    std::tuple<halp::transaction<float>> transactions;
    static constexpr auto selection() { return "page"; }
    std::function<void(int)> select;
    std::vector<int> shown;
    void on_selection_changed(int index) { shown.push_back(index); }
    void paint(auto ctx) { }
  };

  struct ui
  {
    halp_meta(layout, halp::layouts::vbox)
    halp::custom_multi_control<Picker, &ins::a> picker;
    struct
    {
      halp_meta(layout, halp::layouts::tabs)
      static constexpr auto selection() { return "page"; }
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        halp_meta(name, "First")
        halp::item<&ins::a> a;
      } first;
      struct
      {
        halp_meta(layout, halp::layouts::vbox)
        halp_meta(name, "Second")
        halp::item<&ins::b> b;
      } second;
    } pages;
  };
};

template <typename T>
T* findItem(QGraphicsScene& scene)
{
  for(auto* item : scene.items())
    if(auto* t = dynamic_cast<T*>(item))
      return t;
  return nullptr;
}

//! Left out of its layout by setLayoutExcluded
bool excluded(const QGraphicsItem& item)
{
  return !item.isVisible() && item.scale() == 0.;
}

struct DependentFixture : Fixture<DependentObject>
{
  using Fixture::Fixture;
  Process::ControlInlet& model() { return inlet(0); }
  Process::ControlInlet& response() { return inlet(1); }
  Process::ControlInlet& amount() { return inlet(2); }

  score::QGraphicsCombo& combo(const Process::Port& port)
  {
    for(auto* item : controlItems(port))
      if(auto* c = dynamic_cast<score::QGraphicsCombo*>(item))
        return *c;
    FAIL("no combo for " << port.name().toStdString());
    throw;
  }
  QString label(const Process::Port& port)
  {
    for(auto* item : controlItems(port))
      if(auto* t = dynamic_cast<score::SimpleTextItem*>(item))
        return t->text();
    return {};
  }
  //! The box of the port's control, as the layout sees it
  QGraphicsItem& container(const Process::Port& port)
  {
    for(auto* item : scene.items())
      if(auto* p = dynamic_cast<Dataflow::PortItem*>(item); p && &p->port() == &port)
        return *p->parentItem();
    FAIL("no item for port " << port.name().toStdString());
    throw;
  }
  //! What the combo shows
  static QString shown(const score::QGraphicsCombo& c)
  {
    return c.value() >= 0 && c.value() < c.array.size() ? c.array[c.value()] : QString{};
  }
  //! A pick from the drop-down or the stepper: a move, then a release
  static void pick(score::QGraphicsCombo& c, int index)
  {
    c.setValue(index);
    c.sliderMoved();
    c.sliderReleased();
  }
  int value(const Process::ControlInlet& p) { return ossia::convert<int>(p.value()); }
};

//! A button held on a window, as during a drag
struct HeldButton
{
  QWindow window;
  const QPointF pos{10., 10.};
  HeldButton()
  {
    window.resize(100, 100);
    window.show();
    QWindowSystemInterface::handleMouseEvent(
        &window, pos, window.mapToGlobal(pos), Qt::LeftButton, Qt::LeftButton,
        QEvent::MouseButtonPress);
    QWindowSystemInterface::flushWindowSystemEvents();
  }
  void release()
  {
    QWindowSystemInterface::handleMouseEvent(
        &window, pos, window.mapToGlobal(pos), Qt::NoButton, Qt::LeftButton,
        QEvent::MouseButtonRelease);
    QWindowSystemInterface::flushWindowSystemEvents();
  }
};
}

TEST_CASE("Dependent native controls follow the ports they depend on", "[avnd][ui][dependent]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    DependentFixture f{*doc};
    auto& stack = doc->commandStack();
    auto& response = f.combo(f.response());
    auto& model = f.combo(f.model());
    spin();
    const int before = f.steps();

    // Full: every response, and the amount
    CHECK(model.array == QStringList{"Full", "Narrow", "Single"});
    CHECK(response.array == QStringList{"Low", "Band", "High"});
    CHECK(f.label(f.amount()) == "Amount");
    CHECK(!excluded(f.container(f.amount())));
    CHECK(!excluded(f.container(f.response())));

    SECTION("a pick is one undo step with what it resolves")
    {
      DependentFixture::pick(response, 2);
      spin();
      REQUIRE(f.steps() == before + 1);
      DependentFixture::pick(model, 2);
      spin();
      REQUIRE(f.steps() == before + 2);
      CHECK(f.value(f.model()) == 2);
      CHECK(f.value(f.response()) == 0);
      // Single: one response, so no response control; the amount is a width
      CHECK(excluded(f.container(f.response())));
      CHECK(f.label(f.amount()) == "Width");

      // An undo reaches both ports, and the controls follow without an edit
      stack.undo();
      spin();
      CHECK(f.value(f.model()) == 0);
      CHECK(f.value(f.response()) == 2);
      CHECK(f.steps() == before + 2);
      CHECK(stack.canRedo());
      CHECK(DependentFixture::shown(response) == "High");
      CHECK(!excluded(f.container(f.response())));
      CHECK(f.label(f.amount()) == "Amount");
      stack.redo();
      spin();
      CHECK(f.value(f.response()) == 0);
      CHECK(f.steps() == before + 2);
    }

    SECTION("narrow has no amount")
    {
      DependentFixture::pick(model, 1);
      spin();
      REQUIRE(f.steps() == before + 1);
      CHECK(excluded(f.container(f.amount())));
      CHECK(response.array == QStringList{"Band", "High"});
      CHECK(f.value(f.response()) == 1);
      CHECK(DependentFixture::shown(response) == "Band");
    }

    SECTION("execution and presets set values the choices lack, unresolved")
    {
      // Narrow with Low, which Narrow does not offer
      f.model().setValue(1);
      f.response().setValue(0);
      spin();
      CHECK(f.steps() == before);
      CHECK(f.value(f.response()) == 0);
      // Shown as it is, after the choices, not as one of them
      CHECK(response.array == QStringList{"Band", "High", "Low"});
      CHECK(DependentFixture::shown(response) == "Low");

      const auto preset = f.proc->savePreset();
      f.model().setValue(0);
      f.response().setValue(2);
      spin();
      f.proc->loadPreset(preset);
      spin();
      CHECK(f.steps() == before);
      CHECK(f.value(f.model()) == 1);
      CHECK(f.value(f.response()) == 0);
      CHECK(DependentFixture::shown(response) == "Low");

      // Picking it again changes nothing; picking a listed one is an edit
      DependentFixture::pick(response, 2);
      spin();
      CHECK(f.steps() == before);
      DependentFixture::pick(response, 1);
      spin();
      REQUIRE(f.steps() == before + 1);
      CHECK(f.value(f.response()) == 2);
      CHECK(response.array == QStringList{"Band", "High"});
      CHECK(DependentFixture::shown(response) == "High");
    }

    SECTION("a scrub resolves each step from where it started")
    {
      DependentFixture::pick(response, 2);
      spin();
      REQUIRE(f.steps() == before + 1);

      // Full/High, scrubbed over Narrow and Single back to Narrow
      HeldButton held;
      REQUIRE(QGuiApplication::mouseButtons() == Qt::LeftButton);
      for(int index : {1, 2, 1})
      {
        model.setValue(index);
        model.sliderMoved();
        spin();
      }
      CHECK(f.steps() == before + 1);
      held.release();
      model.sliderReleased();
      spin();
      REQUIRE(f.steps() == before + 2);
      // As if Narrow had been picked from Full/High
      CHECK(f.value(f.model()) == 1);
      CHECK(f.value(f.response()) == 2);
      stack.undo();
      CHECK(f.value(f.model()) == 0);
      CHECK(f.value(f.response()) == 2);

      // The next scrub starts from where this one ended
      stack.redo();
      HeldButton again;
      for(int index : {2, 0})
      {
        model.setValue(index);
        model.sliderMoved();
        spin();
      }
      again.release();
      model.sliderReleased();
      spin();
      REQUIRE(f.steps() == before + 3);
      CHECK(f.value(f.model()) == 0);
      CHECK(f.value(f.response()) == 2);
    }
  });
}

TEST_CASE("A custom item takes what it polls for", "[avnd][ui][poll]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Fixture<PollObject> f{*doc};
    auto& poller = f.ui().poller;
    REQUIRE(poller.request_poll);

    poller.pending = 1;
    poller.request_poll();
    REQUIRE(waitUntil([&] { return poller.taken == 1; }));
    // poll() returning false stops it until the next request
    const int polls = poller.polls;
    waitUntil([] { return false; });
    CHECK(poller.polls == polls);

    // Work asked of the item before its ui is built again in place
    poller.pending = 1;
    f.proc->inletsChanged();
    REQUIRE(waitUntil([&] { return poller.taken == 2; }));
    CHECK(poller.polls > polls);
  });
}

TEST_CASE("A custom item takes the double clicks it handles", "[avnd][ui][mouse]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Fixture<PollObject> f{*doc};
    using Item = decltype(f.ui().poller);
    auto* item = findItem<oscr::CustomMultiControl<Item&>>(f.scene);
    REQUIRE(item);

    auto doubleClick = [&](double x) {
      QGraphicsSceneMouseEvent event{QEvent::GraphicsSceneMouseDoubleClick};
      event.setPos({x, 10.});
      event.setScenePos(item->mapToScene(QPointF{x, 10.}));
      event.setButton(Qt::LeftButton);
      event.setButtons(Qt::LeftButton);
      event.setAccepted(x >= 20.);
      f.scene.sendEvent(item, &event);
      return event.isAccepted();
    };
    CHECK(doubleClick(5.));
    CHECK(!doubleClick(30.));
    CHECK(f.ui().poller.doubleClicks == std::vector<double>{5., 30.});
  });
}

TEST_CASE("A custom item and tabs share a named selection", "[avnd][ui][selection]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Fixture<SelectionObject> f{*doc};
    const int before = f.steps();
    auto& picker = f.ui().picker;
    REQUIRE(picker.select);
    auto* tabs = findItem<score::GraphicsTabLayout>(f.scene);
    REQUIRE(tabs);
    CHECK(tabs->currentIndex() == 0);
    REQUIRE(!picker.shown.empty());
    CHECK(picker.shown.back() == 0);

    // The item selects the page
    picker.select(1);
    CHECK(tabs->currentIndex() == 1);
    CHECK(picker.shown.back() == 1);

    // The tab bar selects what the item shows
    REQUIRE(tabs->onCurrentIndexChanged);
    tabs->setCurrentIndex(0);
    tabs->onCurrentIndexChanged(0);
    CHECK(picker.shown.back() == 0);

    // Built again in place, they still share it
    f.proc->inletsChanged();
    spin();
    tabs = findItem<score::GraphicsTabLayout>(f.scene);
    REQUIRE(tabs);
    picker.select(1);
    CHECK(tabs->currentIndex() == 1);
    spin();
    CHECK(f.steps() == before);
  });
}

TEST_CASE("Execution values reach the ui from the queue only", "[avnd][ui][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Fixture<Object> f{*doc};
    const int before = f.steps();
    using Node = oscr::safe_node<Object>;
    auto node = std::make_shared<Node>(64, 48000., 1);

    // The live instance holds other values than the ones it queued
    node->impl.inputs().a.value = 0.9f;
    node->impl.inputs().b.value = 0.9f;
    node->impl.outputs().out.value = 0.9f;
    typename Node::control_input_values_type ins{};
    typename Node::control_output_values_type outs{};
    {
      using std::get;
      get<0>(ins) = 0.25f;
      get<1>(ins) = 0.5f;
      get<0>(outs) = 0.75f;
    }
    node->control.ins_queue.enqueue(ins);
    node->control.outs_queue.enqueue(outs);

    std::vector<float> a, b;
    QObject::connect(
        &f.a(), &Process::ControlInlet::executionValueChanged, &f.a(),
        [&](const ossia::value& v) { a.push_back(ossia::convert<float>(v)); });
    QObject::connect(
        &f.b(), &Process::ControlInlet::executionValueChanged, &f.b(),
        [&](const ossia::value& v) { b.push_back(ossia::convert<float>(v)); });

    oscr::update_control_value_in_ui<Object>{node, f.proc}();
    CHECK(a == std::vector<float>{0.25f});
    CHECK(b == std::vector<float>{0.5f});
    CHECK(value(f.out()) == 0.75f);
    // Execution is not an edit
    spin();
    CHECK(f.steps() == before);
  });
}

TEST_CASE("Layouts leave excluded items out", "[avnd][ui][layout]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto box = [](QGraphicsItem* parent, double w, double h) {
      auto* item = new score::EmptyRectItem{parent};
      item->setRect({0., 0., w, h});
      return item;
    };
    //! c, after a large b, placed as if b were not there
    auto where = [](QGraphicsItem& layout, QGraphicsItem& item) {
      return item.mapToItem(&layout, QPointF{});
    };
    auto check = [&]<typename L>(std::type_identity<L>, auto setup) {
      L with{nullptr};
      L without{nullptr};
      setup(with);
      setup(without);
      box(&with, 40., 10.);
      auto* b = box(&with, 100., 50.);
      auto* c = box(&with, 30., 20.);
      box(&without, 40., 10.);
      auto* c2 = box(&without, 30., 20.);
      score::setLayoutExcluded(*b, true);
      with.layout();
      without.layout();
      with.centerContent();
      without.centerContent();
      CHECK(where(with, *c) == where(without, *c2));
      CHECK(with.rect() == without.rect());
      CHECK(!b->isVisible());
    };
    auto none = [](auto&) { };
    check(std::type_identity<score::GraphicsVBoxLayout>{}, none);
    check(std::type_identity<score::GraphicsHBoxLayout>{}, none);
    check(std::type_identity<score::GraphicsSectionLayout>{}, [](auto& l) {
      l.setTitle("Section");
    });
    check(std::type_identity<score::GraphicsSplitLayout>{}, none);
    check(std::type_identity<score::GraphicsGridColumnsLayout>{}, [](auto& l) {
      l.setColumns(2);
    });
    check(std::type_identity<score::GraphicsGridColumnsLayout>{}, [](auto& l) {
      l.setColumns(2);
      l.setAlignment(Qt::AlignLeft);
    });
    check(std::type_identity<score::GraphicsGridRowsLayout>{}, [](auto& l) {
      l.setRows(2);
    });

    SECTION("table rows and cells")
    {
      score::GraphicsTableLayout with{nullptr};
      score::GraphicsTableLayout without{nullptr};
      auto row = [](score::GraphicsTableLayout& t) {
        return new score::GraphicsHBoxLayout{&t};
      };
      auto* r1 = row(with);
      box(r1, 40., 10.);
      auto* wide = box(r1, 100., 50.);
      auto* c = box(r1, 30., 20.);
      auto* tall = row(with);
      box(tall, 20., 80.);
      auto* r3 = row(with);
      auto* d = box(r3, 30., 20.);
      score::setLayoutExcluded(*wide, true);
      score::setLayoutExcluded(*tall, true);

      auto* s1 = row(without);
      box(s1, 40., 10.);
      auto* c2 = box(s1, 30., 20.);
      auto* s3 = row(without);
      auto* d2 = box(s3, 30., 20.);
      with.layout();
      without.layout();
      CHECK(where(with, *c) == where(without, *c2));
      CHECK(where(with, *d) == where(without, *d2));
      CHECK(with.rect() == without.rect());
    }

    SECTION("strip cells and pages")
    {
      score::GraphicsStripDetailLayout with{nullptr};
      score::GraphicsStripDetailLayout without{nullptr};
      auto* cell = new score::GraphicsStripCell{nullptr};
      auto* cell2 = new score::GraphicsStripCell{nullptr};
      with.addCell(cell);
      without.addCell(cell2);
      box(cell, 40., 10.);
      score::setLayoutExcluded(*box(cell, 100., 50.), true);
      box(cell2, 40., 10.);
      cell->layout();
      cell2->layout();
      CHECK(cell->rect() == cell2->rect());

      box(&with, 40., 10.);
      auto* huge = box(&with, 300., 200.);
      score::setLayoutExcluded(*huge, true);
      box(&without, 40., 10.);
      box(&without, 1., 1.);
      with.strip().layout();
      without.strip().layout();
      with.layout();
      without.layout();
      CHECK(with.rect() == without.rect());
      with.setCurrentIndex(1);
      CHECK(!huge->isVisible());
    }

    SECTION("tab pages")
    {
      score::GraphicsTabLayout tabs{nullptr};
      tabs.addTab("One");
      tabs.addTab("Two");
      auto* one = box(&tabs, 40., 10.);
      auto* two = box(&tabs, 40., 10.);
      score::setLayoutExcluded(*two, true);
      tabs.layout();
      tabs.setCurrentIndex(1);
      CHECK(!two->isVisible());
      tabs.setCurrentIndex(0);
      CHECK(one->isVisible());
    }
  });
}

TEST_CASE("A custom item knows the audio device's rate", "[avnd][ui][rate]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* audio = ctx.findSettings<Audio::Settings::Model>();
    REQUIRE(audio);
    const int initial = audio->getRate();
    const int other = initial == 96000 ? 44100 : 96000;

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    {
      Fixture<PollObject> f{*doc};
      auto& poller = f.ui().poller;
      CHECK(poller.sample_rate == double(initial));

      audio->adoptRate(other);
      CHECK(poller.sample_rate == double(other));

      // A widget built again in place follows it too
      f.proc->inletsChanged();
      spin();
      audio->adoptRate(22050);
      CHECK(poller.sample_rate == 22050.);
    }
    // Gone with its widget: a later change reaches nothing
    audio->adoptRate(initial);
    CHECK(audio->getRate() == initial);
  });
}
