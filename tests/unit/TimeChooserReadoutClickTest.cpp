// The time chooser in a view, clicked the way a user does it.
//
// Clicks on the readout come in quick succession, and Qt then delivers every
// second one as a double-click (press, release, double-click, release): the
// cycle free -> straight -> dotted -> triplet -> free must go on through them.
// A right-click on a free-running time types in the seconds, through the same
// command as a drag.

#include <Process/Dataflow/ControlWidgets.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <score/graphics/RightClickWidget.hpp>
#include <score/graphics/widgets/QGraphicsTimeChooser.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <QDoubleSpinBox>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsView>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Mouse.hpp>
#include <score_test/Process.hpp>
#include <score_test/Project.hpp>

namespace
{
using Feel = score::QGraphicsTimeChooser::Feel;
const QString metro_uuid = QStringLiteral("6e001674-d4a5-478f-9f7e-d7ad1bdcee2b");

enum class Mode
{
  Free,
  Straight,
  Dotted,
  Triplet
};

Mode modeOf(const score::QGraphicsTimeChooser& item)
{
  if(!item.synced())
    return Mode::Free;
  switch(item.feel())
  {
    case Feel::Dotted:
      return Mode::Dotted;
    case Feel::Triplet:
      return Mode::Triplet;
    default:
      return Mode::Straight;
  }
}

Mode next(Mode m)
{
  return Mode((int(m) + 1) % 4);
}

const char* name(Mode m)
{
  constexpr const char* names[] = {"free", "straight", "dotted", "triplet"};
  return names[int(m)];
}

// On the readout chip, below the dial
constexpr QPointF readout{17., 30.};
constexpr QPointF dial{17., 12.};

//! A time chooser shown in a view, the one of the process scene or of the
//! inspector.
struct Harness
{
  QGraphicsScene scene;
  QGraphicsView view{&scene};
  score::QGraphicsTimeChooser* item{};
  int released{};

  explicit Harness(score::QGraphicsTimeChooser* it)
      : item{it}
  {
    scene.setSceneRect(0, 0, 200, 100);
    scene.addItem(item);
    item->setPos(10, 10);
    view.resize(220, 120);
    QObject::connect(
        item, &score::QGraphicsTimeChooser::sliderReleased, item, [this] { released++; });
    view.show();
    QApplication::processEvents();
  }

  ~Harness()
  {
    score::closeRightClickWidget();
    scene.removeItem(item);
    delete item;
  }

  QPoint inView(QPointF itemPos) const
  {
    return view.mapFromScene(item->mapToScene(itemPos));
  }

  //! One click; \p second: within the double-click interval of the previous
  //! one, which the platform then turns into a double-click (qguiapplication.cpp
  //! processMouseEvent, and QWidgetWindow drops the press that made it).
  void click(QPointF itemPos, bool second, Qt::MouseButton b = Qt::LeftButton)
  {
    auto& vp = *view.viewport();
    const QPoint p = inView(itemPos);
    score::test::mouseEvent(
        vp, second ? QEvent::MouseButtonDblClick : QEvent::MouseButtonPress, p, b, b,
        Qt::NoModifier);
    score::test::mouseEvent(vp, QEvent::MouseButtonRelease, p, b, Qt::NoButton, Qt::NoModifier);
    QApplication::processEvents();
    QApplication::processEvents();
  }

  //! \p n clicks on the readout as fast as a user clicks: every second one is
  //! a double-click. Checks that each one moves on to the next mode.
  void clickThrough(int n)
  {
    for(int i = 0; i < n; i++)
    {
      const Mode before = modeOf(*item);
      const int releasedBefore = released;
      click(readout, i % 2 == 1);
      INFO("click " << i << (i % 2 == 1 ? " (double-click)" : "") << " from "
                    << name(before));
      REQUIRE(modeOf(*item) == next(before));
      // One edit per click
      CHECK(released - releasedBefore == 1);
    }
  }
};

score::QGraphicsTimeChooser* freshItem()
{
  auto* item = new score::QGraphicsTimeChooser{nullptr};
  item->setRange(0., 5., 0.25);
  return item;
}
}

TEST_CASE("time chooser: quick clicks on the readout go through every mode, free included", "[time_chooser]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    // Values of every kind: free, the knob's detents in each feel, one only
    // reachable with Alt, both ends of the table, and what a file may hold
    // (off any detent).
    const ossia::vec2f starts[] = {
        {0.3f, 0.f},         {0.f, 0.f},         {1.f, 0.f},
        {1.f / 8.f, 1.f},    {3.f / 8.f, 1.f},   {1.f / 24.f, 1.f},
        {3.f / 64.f, 1.f},   {1.f / 64.f, 1.f},  {4.f, 1.f},
        {3.f, 1.f},          {2.f / 3.f, 1.f},   {0.3f, 1.f},
        {0.2f, 0.5f},
    };
    for(auto v : starts)
    {
      INFO("start {" << v[0] << ", " << v[1] << "}");
      Harness h{freshItem()};
      h.item->setValue(v);
      // Three full cycles and a bit: every mode is left by a single click and
      // by a double-click
      h.clickThrough(13);
    }
  });
}

TEST_CASE("time chooser: a double-click on the dial still resets it", "[time_chooser]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Harness h{freshItem()};
    h.item->setValue({3.f / 8.f, 1.f}); // 1/4.
    h.click(dial, false);
    h.click(dial, true);
    CHECK(h.item->synced());
    CHECK(h.item->value() == ossia::vec2f{0.125f, 1.f});
    CHECK(h.item->feel() == Feel::Straight);
  });
}

TEST_CASE("time chooser: a right-click on a free time types in its seconds", "[time_chooser]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Harness h{freshItem()};
    h.item->setValue({0.5f, 0.f}); // 2.5 s, linear
    REQUIRE(score::currentRightClickWidget().data() == nullptr);

    h.click(dial, false, Qt::RightButton);
    auto* proxy = score::currentRightClickWidget().data();
    REQUIRE(proxy != nullptr);
    auto* box = qobject_cast<QDoubleSpinBox*>(proxy->widget());
    REQUIRE(box != nullptr);
    CHECK(box->value() == Catch::Approx(2.5));
    CHECK(box->minimum() == 0.);
    CHECK(box->maximum() == 5.);

    box->setValue(1.25);
    CHECK(!h.item->synced());
    CHECK(h.item->value()[0] == Catch::Approx(0.25f));
    CHECK(h.item->freeText() == QStringLiteral("1.25 s"));
  });
}

TEST_CASE("time chooser: a right-click on a synced time opens nothing", "[time_chooser]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Harness h{freshItem()};
    h.item->setValue({1.f / 8.f, 1.f});
    h.click(dial, false, Qt::RightButton);
    h.click(readout, false, Qt::RightButton);
    CHECK(score::currentRightClickWidget().data() == nullptr);
    CHECK(h.item->value() == ossia::vec2f{1.f / 8.f, 1.f});
    CHECK(h.released == 0);
  });
}

TEST_CASE("time chooser: readout clicks and typed seconds are one undoable command each", "[time_chooser]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = score::test::add_process(*doc, metro_uuid, {});
    if(!proc)
      SKIP("Metro v2 is not built");
    auto* period
        = qobject_cast<Process::TimeChooser*>(&score::test::control_named(*proc, "Period"));
    REQUIRE(period);
    // As loaded from a file: a dotted quarter
    period->setValue(ossia::vec2f{3.f / 8.f, 1.f});

    auto& stack = doc->commandStack();
    QObject context;
    auto* item = WidgetFactory::TimeChooser::make_item(
        *period, *period, doc->context(), nullptr, &context);
    Harness h{item};
    REQUIRE(modeOf(*item) == Mode::Dotted);

    const int base = stack.size();
    h.clickThrough(6); // triplet, free, straight, dotted, triplet, free
    CHECK(stack.size() == base + 6);
    const auto model = [&] { return ossia::convert<ossia::vec2f>(period->value()); };
    CHECK(model()[1] == 0.f);

    // Typed in, then Enter
    h.click(dial, false, Qt::RightButton);
    auto* proxy = score::currentRightClickWidget().data();
    REQUIRE(proxy != nullptr);
    auto* box = qobject_cast<QDoubleSpinBox*>(proxy->widget());
    REQUIRE(box != nullptr);
    box->setValue(2.);
    Q_EMIT box->editingFinished();
    QApplication::processEvents();
    CHECK(stack.size() == base + 7);
    CHECK(model()[0] == Catch::Approx(2.f));
    CHECK(model()[1] == 0.f);

    // Each undo steps back one edit, and the item follows
    const auto undo = [&] {
      stack.undo();
      QApplication::processEvents();
    };
    undo();
    CHECK(model()[1] == 0.f);
    CHECK(model()[0] != Catch::Approx(2.f));
    CHECK(modeOf(*item) == Mode::Free);
    undo();
    CHECK(model() == ossia::vec2f{1.f / 6.f, 1.f}); // 1/4T
    CHECK(modeOf(*item) == Mode::Triplet);
    undo();
    CHECK(model() == ossia::vec2f{3.f / 8.f, 1.f}); // 1/4.
    CHECK(modeOf(*item) == Mode::Dotted);
    undo();
    CHECK(model() == ossia::vec2f{1.f / 4.f, 1.f}); // 1/4
    CHECK(modeOf(*item) == Mode::Straight);
    undo();
    CHECK(model()[1] == 0.f);
    CHECK(modeOf(*item) == Mode::Free);
    undo();
    CHECK(model() == ossia::vec2f{1.f / 6.f, 1.f});
    undo();
    CHECK(model() == ossia::vec2f{3.f / 8.f, 1.f});
    CHECK(modeOf(*item) == Mode::Dotted);
    CHECK(stack.currentIndex() == base);
  });
}
