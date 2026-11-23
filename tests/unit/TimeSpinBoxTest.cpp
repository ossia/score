// The time spin box honours a minimum: 0 unless set, which is what every
// duration wants; a negative minimum lets it show and edit times before zero,
// e.g. a start offset delaying a process's content.

#include <score/widgets/TimeSpinBox.hpp>

#include <ossia/detail/flicks.hpp>

#include <QApplication>
#include <QLineEdit>
#include <QMouseEvent>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

namespace
{
constexpr int64_t bar = 4 * ossia::quarter_duration<int64_t>;

//! Drags vertically in the bars field by `dy` pixels (down is negative).
void drag_bars(score::TimeSpinBox& box, int dy)
{
  const QPointF at{10., 10.};
  const QPointF global = box.mapToGlobal(at);
  QMouseEvent press{QEvent::MouseButtonPress,
                    at,
                    global,
                    Qt::LeftButton,
                    Qt::LeftButton,
                    Qt::NoModifier};
  QApplication::sendEvent(&box, &press);
  QMouseEvent move{
      QEvent::MouseMove,
      at + QPointF{0., double(dy)},
      global + QPointF{0., double(dy)},
      Qt::NoButton,
      Qt::LeftButton,
      Qt::NoModifier};
  QApplication::sendEvent(&box, &move);
  QMouseEvent release{
      QEvent::MouseButtonRelease,
      at + QPointF{0., double(dy)},
      global + QPointF{0., double(dy)},
      Qt::LeftButton,
      Qt::NoButton,
      Qt::NoModifier};
  QApplication::sendEvent(&box, &release);
}

//! Types `text` as the double-click editor does, and answers what it showed.
QString type(score::TimeSpinBox& box, const QString& text)
{
  QMouseEvent dbl{
      QEvent::MouseButtonDblClick,
      QPointF{10., 10.},
      box.mapToGlobal(QPointF{10., 10.}),
      Qt::LeftButton,
      Qt::LeftButton,
      Qt::NoModifier};
  QApplication::sendEvent(&box, &dbl);
  auto* le = box.findChild<QLineEdit*>();
  REQUIRE(le);
  const QString shown = le->text();
  le->setText(text);
  Q_EMIT le->editingFinished();
  delete le;
  return shown;
}
}

TEST_CASE("A time spin box stays at or above its minimum", "[widgets][time]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::TimeSpinBox box;
    box.resize(150, 20);
    box.setTime(ossia::time_value{bar});

    // Dragged down ten bars: stops at 0
    drag_bars(box, 60);
    CHECK(box.time().impl == 0);

    // Typed negative: 0
    type(box, "-2 . 0 . 0 . 00");
    CHECK(box.time().impl == 0);
  });
}

TEST_CASE("A negative minimum shows and edits times before zero", "[widgets][time]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::TimeSpinBox box;
    box.resize(150, 20);
    box.setMinimumTime(ossia::time_value{-100 * bar});

    drag_bars(box, 60);
    CHECK(box.time().impl == -10 * bar);

    // Shown with its sign, and read back the same
    const QString shown = type(box, "-3 . 1 . 0 . 00");
    CHECK(shown.startsWith('-'));
    CHECK(box.time().impl == -(3 * bar + ossia::quarter_duration<int64_t>));

    // Below the minimum: the minimum
    type(box, "-500 . 0 . 0 . 00");
    CHECK(box.time().impl == -100 * bar);

    // A minimum above the time moves the time to it
    box.setMinimumTime(ossia::time_value{0});
    CHECK(box.time().impl == 0);
  });
}
