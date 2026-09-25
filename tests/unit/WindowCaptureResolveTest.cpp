// Which window or screen a capture device opens, given what its settings
// remember. Window ids change whenever the captured application restarts,
// and settings written by a script may only name the window.

#include <Gfx/WindowCapture/WindowCaptureBackend.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace Gfx::WindowCapture;

TEST_CASE("a capture window is found by id, then by title", "[unit][windowcapture]")
{
  const std::vector<CapturableWindow> w{
      {"Terminal", 11}, {"Video - player", 22}, {"Video", 33}};

  CHECK(resolveWindowId(w, 22, "Terminal") == 22);   // a live id wins
  CHECK(resolveWindowId(w, 99, "Terminal") == 11);   // a stale id: the title
  CHECK(resolveWindowId(w, 0, "Video") == 33);       // exact before partial
  CHECK(resolveWindowId(w, 0, "player") == 22);      // then partial
  CHECK(resolveWindowId(w, 0, "nothing") == 0);      // no match: unchanged
  CHECK(resolveWindowId(w, 99, "") == 99);
  CHECK(resolveWindowId({}, 0, "Video") == 0);
}

TEST_CASE("a capture screen is found by id, then by name", "[unit][windowcapture]")
{
  const std::vector<CapturableScreen> s{{"DP-1", 1}, {"HDMI-A-1", 2}};
  CHECK(resolveScreenId(s, 2, "DP-1") == 2);
  CHECK(resolveScreenId(s, 0, "DP-1") == 1);
  CHECK(resolveScreenId(s, 0, "HDMI") == 2);
  CHECK(resolveScreenId(s, 0, "") == 0);
}
