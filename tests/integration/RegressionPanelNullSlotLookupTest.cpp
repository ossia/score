// Regression test for ace6e70ff — "explorer: null-tolerant panel lookup in
// findDeviceExplorerWidgetInstance".
//
// findDeviceExplorerWidgetInstance iterated ctx.panels(), whose indirect
// iterator dereferences every slot unconditionally: with an empty (null) slot
// in ApplicationComponents::panels this is UB, and aborts outright on hardened
// stdlibs (_GLIBCXX_ASSERTIONS unique_ptr deref assertion). The fix routes the
// lookup through findPanel<T>(), which skips empty slots.
//
// We inject a null slot at the FRONT of the panel list (so the lookup must
// walk over it before reaching the real device-explorer panel) and check the
// widget is still found. On a hardened-stdlib build the unfixed code aborts
// here; on a plain build the unfixed code only exercises UB, so this test's
// red/green signal is strongest with _GLIBCXX_ASSERTIONS enabled.

#include <score_test/App.hpp>

#include <Explorer/Explorer/DeviceExplorerWidget.hpp>

#include <score/plugins/panel/PanelDelegate.hpp>

#include <core/application/MinimalApplication.hpp>

#include <QApplication>

#include <catch2/catch_test_macros.hpp>

TEST_CASE(
    "findDeviceExplorerWidgetInstance tolerates an empty panel slot",
    "[integration][regression][panels][gui]")
{
  score::test::prepare_test_environment(/*headless=*/false);
  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");

  static int argc = 1;
  static char arg0[] = "score-test";
  static char* argv[] = {arg0, nullptr};
  score::MinimalGUIApplication app{argc, argv, /*show=*/false};
  QApplication::processEvents();

  auto& panels = app.componentsData().panels;

  // The GUI application registers the device-explorer panel during plugin
  // load; the lookup must find it.
  REQUIRE(Explorer::findDeviceExplorerWidgetInstance(app.context()) != nullptr);

  // Inject an empty slot in front, as left behind by a declining panel
  // factory before the db8477a5b/f0d3ad966 guards existed.
  panels.insert(panels.begin(), nullptr);

  // Without the fix: UB / hardened-stdlib abort while iterating. With it:
  // the real panel is still found, null slot skipped.
  auto* widget = Explorer::findDeviceExplorerWidgetInstance(app.context());
  CHECK(widget != nullptr);

  // Remove the injected slot before teardown (other panel iterations at
  // shutdown are not the subject of this test).
  panels.erase(panels.begin());

  score::test::close_all_documents(app.context());
  QApplication::processEvents();
}
