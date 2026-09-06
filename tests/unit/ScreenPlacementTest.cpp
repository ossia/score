// Choosing which screen an output window gets.
//
// This only matters on the platforms with no window manager -- eglfs and
// friends on an embedded board -- where a screen backs exactly one window and
// asking for a second one calls qFatal inside Qt. There is no catching that,
// so the choice has to be right before the window is shown.
//
// The rule itself is arithmetic on a list and is tested as such; whether eglfs
// then honours it was checked on hardware (rk3588, two HDMI outputs) and is
// what ScreenNode's geometry-before-show relies on.

#include <Gfx/Graph/ScreenPlacement.hpp>

#include <score/gfx/DisplayConfig.hpp>

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>

#include <score_test/App.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("A free screen is chosen for an output window", "[gfx]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext&) {
    const auto screens = QGuiApplication::screens();
    REQUIRE(!screens.isEmpty());

    auto* first = screens[0];

    SECTION("the screen the user picked wins when nothing holds it")
    {
      CHECK(score::gfx::freeScreen(first, screens, {}) == first);
    }

    SECTION("no preference takes the first free one")
    {
      CHECK(score::gfx::freeScreen(nullptr, screens, {}) == first);
    }

    SECTION("a preference that is taken falls through to another screen")
    {
      auto* got = score::gfx::freeScreen(first, screens, {first});

      // With one screen there is nowhere to fall through to, and saying so is
      // the point: the caller must not open the window at all.
      if(screens.size() == 1)
        CHECK(got == nullptr);
      else
        CHECK((got != nullptr && got != first));
    }

    SECTION("every screen taken means no window")
    {
      QSet<QScreen*> all;
      for(auto* s : screens)
        all.insert(s);

      CHECK(score::gfx::freeScreen(first, screens, all) == nullptr);
      CHECK(score::gfx::freeScreen(nullptr, screens, all) == nullptr);
    }

    SECTION("no screens at all")
    {
      CHECK(score::gfx::freeScreen(nullptr, {}, {}) == nullptr);
    }

    SECTION("the main window holds its screen")
    {
      // occupiedScreens() is what feeds `taken`: the widget UI has to count,
      // otherwise an output would be placed on top of it and abort.
      QWindow w;
      w.setGeometry(first->geometry());
      w.show();

      const auto taken = score::gfx::occupiedScreens();
      CHECK(taken.contains(w.screen()));
      CHECK(score::gfx::freeScreen(w.screen(), screens, taken) != w.screen());
    }
  });
}

TEST_CASE("A window manager places windows itself", "[gfx]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext&) {
    // The tests run under offscreen or a desktop platform, where screens are
    // not exclusive and none of the above applies.
    CHECK(!score::gfx::oneWindowPerScreen());
  });
}
