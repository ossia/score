#pragma once

// Headless application bootstrap for score tests.
//
// run_in_app() boots a full score application with the entire plugin system
// loaded but no main window, then runs the supplied callable synchronously on
// the main stack (so Catch2 assertions propagate normally). It pumps the Qt
// event loop before and after so deferred plugin/document initialization
// settles.
//
// In a dynamic-plugin build the plugins are loaded at runtime from
// "<cwd>/plugins"; score_add_test(... APP) runs the test from the build root so
// that <build>/plugins is on that search path. In a static-plugin build the
// plugins are linked in and registered via score_init_static_plugins().

#include <core/application/MinimalApplication.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <ossia/detail/thread.hpp>

#include <QApplication>
#include <QDir>

#include <clocale>
#include <utility>

namespace score::test
{

/// Set up a hermetic, headless, audio-free environment for tests. Idempotent;
/// only sets variables that aren't already set so callers can override.
inline void prepare_test_environment()
{
  // Headless rendering.
  if(!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");

  // No real audio device: force the dummy backend (honored at startup by
  // Audio::Settings::Model). Avoids connecting to a live PipeWire/JACK server.
  if(!qEnvironmentVariableIsSet("SCORE_AUDIO_BACKEND"))
    qputenv("SCORE_AUDIO_BACKEND", "dummy");

  // Hermetic settings: keep tests from reading or polluting the developer's
  // real score configuration. Point QSettings at a throwaway config dir.
  if(!qEnvironmentVariableIsSet("XDG_CONFIG_HOME"))
  {
    const QString cfg = QDir::tempPath() + "/score-tests/config";
    QDir{}.mkpath(cfg);
    qputenv("XDG_CONFIG_HOME", cfg.toUtf8());
  }

  // Match the organization the real application uses so settings paths resolve
  // identically, but under a distinct app name so we never touch real state.
  QCoreApplication::setOrganizationName("ossia");
  QCoreApplication::setOrganizationDomain("ossia.io");
  QCoreApplication::setApplicationName("score-test");
}

/// Boot a headless score app and invoke fn(const GUIApplicationContext&).
template <typename F>
void run_in_app(F&& fn)
{
  prepare_test_environment();

  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");

  // Register this (main) thread as the UI thread, exactly as ossia::context
  // does during a normal score::Application bootstrap. MinimalApplication does
  // not create an ossia::context, so without this the thread type stays unset
  // and code paths guarded by OSSIA_ENSURE_CURRENT_THREAD (e.g. the gfx plugin
  // on document creation) abort in debug builds.
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

  score::MinimalApplication app;

  // Let deferred initialization (plugin setup, etc.) settle.
  QApplication::processEvents();
  QApplication::processEvents();

  std::forward<F>(fn)(app.context());

  QApplication::processEvents();
}

}
