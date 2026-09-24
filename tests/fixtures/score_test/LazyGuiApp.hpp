#pragma once

// A bare QGuiApplication for tests that need Qt's font / painting machinery but
// no score application: created on first use (Catch2 owns main()), destroyed at
// the end of the run. Including this header registers the teardown listener,
// so include it from one translation unit of the test executable only.

#include <QDir>
#include <QGuiApplication>

#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

namespace score::test
{

//! Creates the application on the offscreen platform unless another one is
//! requested. A static Qt build has no font directory of its own: point it at a
//! system one when none is set.
inline void ensure_gui_app()
{
  if(qApp)
    return;
  if(!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
  if(!qEnvironmentVariableIsSet("QT_QPA_FONTDIR"))
  {
    for(const char* dir :
        {"/usr/share/fonts/truetype/dejavu", "/usr/share/fonts/TTF",
         "/usr/share/fonts/dejavu", "/usr/share/fonts/truetype"})
    {
      if(QDir(QString::fromLatin1(dir)).exists())
      {
        qputenv("QT_QPA_FONTDIR", dir);
        break;
      }
    }
  }
  static int argc = 1;
  static char arg0[] = "score-test";
  static char* argv[] = {arg0, nullptr};
  // Not a static object: destroyed from atexit, after Qt's own static state,
  // it faults. lazy_gui_app_teardown deletes it at the end of the run instead.
  new QGuiApplication(argc, argv);
}

//! Destroys the application while Qt is still whole; otherwise the font engines
//! and glyph caches ~QGuiApplication releases are reported as leaks.
struct lazy_gui_app_teardown final : Catch::EventListenerBase
{
  using Catch::EventListenerBase::EventListenerBase;
  void testRunEnded(Catch::TestRunStats const&) override { delete qApp; }
};
CATCH_REGISTER_LISTENER(lazy_gui_app_teardown)

}
