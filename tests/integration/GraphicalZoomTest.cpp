// Does the live zoom actually reach a running score UI?
#include <score_test/App.hpp>

#include <score/model/Skin.hpp>

#include <QApplication>
#include <QScreen>
#include <QWidget>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("The graphical zoom applies to a running application", "[integration][skin]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    if(!score::canSetGlobalScaleFactorLive())
    {
      // Below Qt 6.6, or no high-DPI scaling: nothing to assert beyond the
      // refusal. Same predicate the settings UI uses.
      CHECK_FALSE(score::setGlobalScaleFactor(2.0));
      return;
    }

    auto w = new QWidget;
    w->resize(300, 200);
    w->show();
    QApplication::processEvents();

    const double before = QApplication::primaryScreen()->devicePixelRatio();

    REQUIRE(score::setGlobalScaleFactor(2.0));
    QApplication::processEvents();

    const double after = QApplication::primaryScreen()->devicePixelRatio();
    INFO("screen dpr " << before << " -> " << after);
    CHECK(after == 2.0);
    // Cycled by setGlobalScaleFactor, so it adopts the new ratio.
    CHECK(w->devicePixelRatioF() == 2.0);

    // And back: the window has to follow in this direction too.
    REQUIRE(score::setGlobalScaleFactor(1.0));
    QApplication::processEvents();
    CHECK(QApplication::primaryScreen()->devicePixelRatio() == 1.0);
    CHECK(w->devicePixelRatioF() == 1.0);

    // Out of range is refused rather than silently clamped.
    CHECK_FALSE(score::setGlobalScaleFactor(0.4));
    CHECK_FALSE(score::setGlobalScaleFactor(25.0));

    delete w;
  });
}
