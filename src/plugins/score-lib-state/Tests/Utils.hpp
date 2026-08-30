#pragma once

#include <State/Message.hpp>

#include <score/serialization/VisitorCommon.hpp>

#include <core/application/MinimalApplication.hpp>
#include <core/application/MockApplication.hpp>

#include <ossia/network/domain/domain.hpp>

#include <QMetaType>
#include <QObject>

#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <memory>

namespace
{
struct ScoreTestApplicationStarter final : Catch::EventListenerBase
{
  using Catch::EventListenerBase::EventListenerBase;
  // NOT a function-local static. A static here is destroyed from the atexit
  // chain, after main has returned and after Qt's own static state is gone.
  // Measured on Windows with cdb:
  //   common_exit -> execute_onexit_table
  //     -> QApplication::~QApplication -> QGuiApplication::~QGuiApplication
  //        c0000005, reading 0xffffffffffffffff
  // Thirteen test binaries printed "All tests passed" and THEN faulted, so
  // ctest recorded a SegFault for a run that had entirely succeeded.
  //
  // Owning it here and dropping it in testRunEnded destroys the QApplication
  // while the runtime is still intact. MinimalApplication's own destructor
  // already tears the settings models down before releasing the app, for the
  // same class of reason (see MinimalApplication.hpp) -- it just needs to run
  // at a sane time.
  std::unique_ptr<score::MinimalApplication> app;

  void testRunStarting(const Catch::TestRunInfo&) override
  {
#if !defined(__EMSCRIPTEN__)
    if(!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
      qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
    app = std::make_unique<score::MinimalApplication>();
  }

  void testRunEnded(const Catch::TestRunStats&) override { app.reset(); }
};
}
CATCH_REGISTER_LISTENER(ScoreTestApplicationStarter)
