// A slider a JSFX's @gfx changes moves its control, whatever input made the
// change: a wheel, a key, or the @gfx code by itself, with no mouse button
// held. What @block flags for the display only is not an edit.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Effect/EffectLayer.hpp>
#include <Process/Dataflow/Port.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QTemporaryDir>
#include <QWheelEvent>
#include <QWidget>

#include <vector>

#include <YSFX/ProcessModel.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
const QString ysfx_uuid = QStringLiteral("cb6b50d3-82c0-4356-a282-317da8b8022f");

// slider1 steps with the wheel, slider2 is set by the key 'u', slider3 by the
// script on its third frame, slider4 is flagged by @block for the display.
constexpr auto script = R"(desc:gfx edits test
slider1:10<0,100,1>Wheel
slider2:0<0,100,1>Key
slider3:0<0,100,1>Timer
slider4:0<0,100,1>Meter
@block
slider4 = 42;
sliderchange(slider4);
@gfx 200 100
mouse_wheel != 0 ? (
  slider1 += mouse_wheel > 0 ? 1 : -1;
  mouse_wheel = 0;
  slider_automate(slider1);
);
c = gfx_getchar();
c == 'u' ? (
  slider2 = 50;
  sliderchange(slider2);
);
frames += 1;
frames == 3 ? (
  slider3 = 70;
  slider_automate(slider3);
);
)";

void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
    QApplication::processEvents();
}

//! Slider n of the script is the control with id 3 + n
double control(YSFX::ProcessModel& p, int n)
{
  auto* c = qobject_cast<Process::ControlInlet*>(p.inlet(Id<Process::Port>(3 + n)));
  REQUIRE(c);
  return ossia::convert<float>(c->value());
}
}

TEST_CASE(
    "JSFX: a slider changed by @gfx moves its control with no button held",
    "[integration][ysfx][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("gfx-edits.jsfx");
    score::test::write_file(path, script);

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = static_cast<YSFX::ProcessModel*>(
        score::test::add_process(*doc, ysfx_uuid, path));
    if(!proc || !proc->fx)
      SKIP("YSFX not built");

    Process::setupExternalUI(*proc, doc->context(), true);
    spin(100);
    QWidget* win = proc->externalUI;
    REQUIRE(win);
    win->repaint();
    win->repaint();
    win->repaint();
    spin(50);
    CHECK(control(*proc, 3) == 70.);

    {
      const QPointF pos{50, 50};
      QWheelEvent wheel{
          pos,           win->mapToGlobal(pos), QPoint{}, QPoint{0, 120}, Qt::NoButton,
          Qt::NoModifier, Qt::NoScrollPhase,     false};
      QApplication::sendEvent(win, &wheel);
      win->repaint();
      spin(50);
    }
    CHECK(control(*proc, 1) == 11.);

    {
      QKeyEvent press{QEvent::KeyPress, Qt::Key_U, Qt::NoModifier, QStringLiteral("u")};
      QApplication::sendEvent(win, &press);
      QKeyEvent release{
          QEvent::KeyRelease, Qt::Key_U, Qt::NoModifier, QStringLiteral("u")};
      QApplication::sendEvent(win, &release);
      win->repaint();
      spin(50);
    }
    CHECK(control(*proc, 2) == 50.);

    {
      auto* y = proc->fx.get();
      ysfx_set_block_size(y, 64);
      ysfx_set_sample_rate(y, 48000);
      ysfx_init(y);
      std::vector<double> l(64), r(64);
      double* chans[2]{l.data(), r.data()};
      ysfx_process_double(y, chans, chans, 2, 2, 64);
      REQUIRE(ysfx_slider_get_value(y, 3) == 42.);
      win->repaint();
      spin(50);
    }
    CHECK(control(*proc, 4) == 0.);

    Process::setupExternalUI(*proc, doc->context(), false);
    spin(50);
  });
}
