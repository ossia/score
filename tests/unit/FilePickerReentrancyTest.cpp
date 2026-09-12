// Clicking a file control while its dialog is already up.
//
// QFileDialog::getOpenFileName runs a nested event loop, and that loop keeps
// delivering clicks to the widget underneath: a second click queued a second
// dialog, which appeared the instant the first closed. Same for
// getExistingDirectory behind the folder controls.
//
// The dialogs themselves cannot be driven from a test, so what is pinned here
// is the guard's rule: while a pick is in flight, another request is dropped
// rather than queued. The lambda below stands in for the dialog and re-enters
// exactly as a queued click does.

#include <catch2/catch_test_macros.hpp>

namespace
{
// Mirrors the guard in openFileToImport / openFolderToImport. The flag lives
// OUTSIDE the template deliberately: a static inside one is per callable type,
// so it would only catch a repeat click on the same control and two different
// controls could still stack dialogs. The first version of this test caught
// exactly that.
int pick_calls = 0;
int picked = 0;

inline bool& picking() noexcept
{
  static bool b = false;
  return b;
}

template <typename Dialog, typename F>
void guarded_pick(Dialog&& dialog, F onPicked)
{
  if(picking())
    return;
  picking() = true;
  ++pick_calls;
  const bool ok = dialog();
  picking() = false;
  if(ok)
    onPicked();
}
}

TEST_CASE("a second click cannot open a second file dialog", "[unit][ui]")
{
  pick_calls = 0;
  picked = 0;

  // The dialog re-enters once, the way a click delivered by its own nested
  // event loop does.
  bool reentered = false;
  auto dialog = [&] {
    if(!reentered)
    {
      reentered = true;
      guarded_pick([] { return true; }, [] { ++picked; });
    }
    return true;
  };

  guarded_pick(dialog, [] { ++picked; });

  CHECK(reentered);        // the re-entry really happened
  CHECK(pick_calls == 1);  // and opened nothing
  CHECK(picked == 1);      // the outer pick still delivered its result
}

TEST_CASE("a cancelled pick leaves the next one free", "[unit][ui]")
{
  pick_calls = 0;
  picked = 0;

  guarded_pick([] { return false; }, [] { ++picked; });
  CHECK(picked == 0);

  guarded_pick([] { return true; }, [] { ++picked; });
  CHECK(pick_calls == 2);
  CHECK(picked == 1);
}
