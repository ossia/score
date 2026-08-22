#pragma once

// Application bootstrap for score tests.
//
// Two entry points:
//  * run_in_app(fn)      — headless (MinimalApplication, no window, offscreen
//                          platform). Use for model / command-data /
//                          serialization tests.
//  * run_in_gui_app(fn)  — GUI stack (MinimalGUIApplication) but with the main
//                          window NOT shown, on the real windowing platform.
//                          Use for tests that need valid document presenters,
//                          e.g. undo/redo of scenario commands (selection
//                          pruning routes through the document presenter).
//                          Requires a display (a real X server locally, or
//                          Xvfb in CI).
//
// Both run fn synchronously on the main stack so Catch2 assertions propagate,
// and both close any documents fn left open before tearing the app down (the
// GUI presenter teardown must happen while the factory families are alive).

#include <ossia/context.hpp>

#include <core/application/MinimalApplication.hpp>
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <ossia/detail/thread.hpp>

#include <QApplication>
#include <QDir>

#include <clocale>
#include <utility>

namespace score::test
{

/// Set up a hermetic, audio-free environment. Idempotent; only sets variables
/// that aren't already set so callers can override. When `headless` is true and
/// no platform is set, forces the offscreen QPA platform.
inline void prepare_test_environment(bool headless)
{
  // WebAssembly only ever has the "wasm" platform: asking for another one
  // is a fatal error, and the page is headless anyway.
#if !defined(__EMSCRIPTEN__)
  if(headless && !qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
#endif

  // Nothing interactive: the package manager asks "Download the user library?"
  // one second after its first refresh, and score::question() is a modal
  // QDialog::exec(). A test that runs the event loop for any length of time
  // otherwise wedges in that nested loop with nobody to answer it -- which is
  // the same escape hatch the model itself documents for tests and CI.
  if(!qEnvironmentVariableIsSet("SCORE_SANITIZE_SKIP_CHECKS"))
    qputenv("SCORE_SANITIZE_SKIP_CHECKS", "1");

  // No real audio device: force the dummy backend (honored at startup by
  // Audio::Settings::Model). Avoids connecting to a live PipeWire/JACK server.
  if(!qEnvironmentVariableIsSet("SCORE_AUDIO_BACKEND"))
    qputenv("SCORE_AUDIO_BACKEND", "dummy");

  // Hermetic settings: keep tests from reading or polluting the developer's
  // real score configuration.
  if(!qEnvironmentVariableIsSet("XDG_CONFIG_HOME"))
  {
    const QString cfg = QDir::tempPath() + "/score-tests/config";
    QDir{}.mkpath(cfg);
    qputenv("XDG_CONFIG_HOME", cfg.toUtf8());
  }

  QCoreApplication::setOrganizationName("ossia");
  QCoreApplication::setOrganizationDomain("ossia.io");
  QCoreApplication::setApplicationName("score-test");
}

/// Close every open document while the application is still alive.
inline void close_all_documents(const score::GUIApplicationContext& ctx)
{
  auto& dm = ctx.docManager;
  while(!dm.documents().empty())
    dm.forceCloseDocument(ctx, *dm.documents().front());
  QApplication::processEvents();
}

/// Closes the documents on the way out however fn left the stack: a failed
/// REQUIRE unwinds, and a document still open when the application is destroyed
/// tears its presenter down after the factory families are gone.
struct document_closer
{
  const score::GUIApplicationContext& ctx;
  ~document_closer()
  {
    close_all_documents(ctx);
    QApplication::processEvents();
  }
};

/// Boot a headless score app and invoke fn(const GUIApplicationContext&).
template <typename F>
void run_in_app(F&& fn)
{
  prepare_test_environment(/*headless=*/true);

  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");

  // The same bootstrap score::Application does (Application.cpp): this pins
  // the current thread as the UI one *and* registers the Ossia QML types.
  // Pinning by hand instead, as this used to, left `Ossia.Type` &co undefined,
  // so any QML a test loads would throw the moment it referenced one - which a
  // Mapper device's createTree() does on its first line.
  ossia::context ossia_ctx;

  score::MinimalApplication app;

  QApplication::processEvents();
  QApplication::processEvents();

  document_closer closer{app.context()};
  std::forward<F>(fn)(app.context());
}

/// Boot the GUI stack (window hidden) and invoke fn(const GUIApplicationContext&).
template <typename F>
void run_in_gui_app(F&& fn)
{
  prepare_test_environment(/*headless=*/false);

  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");

  // Same bootstrap as run_in_app(); see the comment there.
  ossia::context ossia_ctx;

  static int argc = 1;
  static char arg0[] = "score-test";
  static char* argv[] = {arg0, nullptr};
  score::MinimalGUIApplication app{argc, argv, /*show=*/false};

  QApplication::processEvents();
  QApplication::processEvents();

  document_closer closer{app.context()};
  std::forward<F>(fn)(app.context());
}

}
