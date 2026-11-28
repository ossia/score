#include "WindowedOutputCommon.hpp"

#include <Gfx/Graph/IdleInhibitor.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test;
using namespace score::test::gfx;
using score::gfx::IdleInhibitor;
using score::gfx::ScreenSaverInhibition;

TEST_CASE("the screen saver inhibition is sent once and undone once", "[gfx][idle]")
{
  ScreenSaverInhibition s;
  CHECK(s.want());
  CHECK(s.pending());
  CHECK_FALSE(s.want());
  CHECK_FALSE(s.replied(7));
  CHECK(s.held());
  CHECK(s.unwant());
  CHECK(s.cookie() == 7);
  CHECK_FALSE(s.held());
  CHECK_FALSE(s.unwant());
}

TEST_CASE("a release before the Inhibit reply undoes it when the reply comes",
          "[gfx][idle]")
{
  ScreenSaverInhibition s;
  CHECK(s.want());
  CHECK_FALSE(s.unwant());
  CHECK(s.replied(42));
  CHECK(s.cookie() == 42);
  CHECK_FALSE(s.held());
  CHECK_FALSE(s.pending());
}

TEST_CASE("wanted again before the Inhibit reply, the inhibition is kept", "[gfx][idle]")
{
  ScreenSaverInhibition s;
  CHECK(s.want());
  CHECK_FALSE(s.unwant());
  CHECK_FALSE(s.want());
  CHECK_FALSE(s.replied(3));
  CHECK(s.held());

  ScreenSaverInhibition t;
  CHECK(t.want());
  CHECK_FALSE(t.unwant());
  CHECK_FALSE(t.want());
  CHECK_FALSE(t.unwant());
  CHECK(t.replied(5));
  CHECK_FALSE(t.held());
}

TEST_CASE("a failed Inhibit is sent again on the next want", "[gfx][idle]")
{
  ScreenSaverInhibition s;
  CHECK(s.want());
  s.failed();
  CHECK_FALSE(s.pending());
  CHECK_FALSE(s.unwant());
  CHECK(s.want());
}

TEST_CASE("inhibitors count their holders once each", "[gfx][idle]")
{
  const int before = IdleInhibitor::holders();
  {
    IdleInhibitor a, b;
    a.acquire();
    a.acquire();
    CHECK(IdleInhibitor::holders() == before + 1);
    b.acquire();
    CHECK(IdleInhibitor::holders() == before + 2);
    a.release();
    a.release();
    CHECK_FALSE(a.held());
    CHECK(IdleInhibitor::holders() == before + 1);
  }
  CHECK(IdleInhibitor::holders() == before);
}

TEST_CASE("a shown output window holds the display awake, a hidden one does not",
          "[gfx][window][idle]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  bool skipped{};
  std::string skipReason, error, backend;
  bool heldShown{}, heldHidden{}, heldReshown{};
  int holdersShown{}, holdersHidden{}, holdersAfter{-1};

  run_in_gui_app([&](const score::GUIApplicationContext&) {
    const int before = IdleInhibitor::holders();
    {
      ScreenRig rig;
      if(!rig.build(api, {256, 192}))
      {
        skipped = rig.skipped();
        skipReason = rig.skipReason();
        error = rig.error();
        backend = rig.backend();
        return;
      }
      backend = rig.backend();
      rig.render(2);
      heldShown = rig.window()->inhibitsIdle();
      holdersShown = IdleInhibitor::holders() - before;

      rig.window()->hide();
      pump_until([&] { return !rig.window()->isExposed(); }, 3000);
      heldHidden = rig.window()->inhibitsIdle();
      holdersHidden = IdleInhibitor::holders() - before;

      rig.window()->show();
      pump_until([&] { return rig.window()->isExposed(); }, 5000);
      rig.render(2);
      heldReshown = rig.window()->inhibitsIdle();
    }
    pump_for(50);
    holdersAfter = IdleInhibitor::holders() - before;
  });

  if(skipped)
    SKIP(backend << ": " << skipReason);
  REQUIRE(error.empty());
  INFO("backend: " << backend);
  CHECK(heldShown);
  CHECK(holdersShown == 1);
  CHECK_FALSE(heldHidden);
  CHECK(holdersHidden == 0);
  CHECK(heldReshown);
  CHECK(holdersAfter == 0);
}
