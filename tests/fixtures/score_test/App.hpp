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
  if(headless && !qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");

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

/// Boot a headless score app and invoke fn(const GUIApplicationContext&).
template <typename F>
void run_in_app(F&& fn)
{
  prepare_test_environment(/*headless=*/true);

  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");

  // Register this (main) thread as the UI thread, exactly as ossia::context
  // does during a normal score::Application bootstrap.
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

  score::MinimalApplication app;

  QApplication::processEvents();
  QApplication::processEvents();

  std::forward<F>(fn)(app.context());

  close_all_documents(app.context());
  QApplication::processEvents();
}

/// Boot the GUI stack (window hidden) and invoke fn(const GUIApplicationContext&).
template <typename F>
void run_in_gui_app(F&& fn)
{
  prepare_test_environment(/*headless=*/false);

  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");

  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

  static int argc = 1;
  static char arg0[] = "score-test";
  static char* argv[] = {arg0, nullptr};
  score::MinimalGUIApplication app{argc, argv, /*show=*/false};

  QApplication::processEvents();
  QApplication::processEvents();

  std::forward<F>(fn)(app.context());

  close_all_documents(app.context());
  QApplication::processEvents();
}

}
