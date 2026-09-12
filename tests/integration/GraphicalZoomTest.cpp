// Does the live zoom actually reach a running score UI?
#include <score_test/App.hpp>

#include <Scenario/Settings/ScenarioSettingsModel.hpp>


#include <score/model/Skin.hpp>

#include <QApplication>
#include <QScreen>
#include <QWidget>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("The graphical zoom applies to a running application", "[integration][skin]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
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
    // The window is cycled by setGlobalScaleFactor, so it adopts the ratio
    // rather than staying on the one it was created with.
    CHECK(w->devicePixelRatioF() == 2.0);

    // And back. The window has to follow in this direction too, which it only
    // does because of the hide/show.
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
