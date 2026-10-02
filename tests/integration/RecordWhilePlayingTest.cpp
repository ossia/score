// A recording armed with "play while recording" asks the execution to start
// playing when its first message comes. Playback may have been started by
// then, by the play button, a transport, a script: the recording then runs
// along with it, and is told when playback starts.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Engine/ApplicationPlugin.hpp>
#include <Execution/ExecutionController.hpp>

#include <core/document/Document.hpp>

#include <QApplication>
#include <QElapsedTimer>

#include <catch2/catch_test_macros.hpp>

namespace
{
void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
    QApplication::processEvents();
}
}

TEST_CASE(
    "the first recorded message while playing leaves the playback running",
    "[integration][execution][recording][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& exec = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>().execution();

    int started = 0;
    QObject::connect(
        &exec, &Execution::ExecutionController::playStarted, &exec,
        [&] { started++; });

    exec.request_play_global(true);
    spin(200);
    CHECK(started == 1);
    const auto before = exec.execution_time();

    // What Recording::ApplicationPlugin does on the first message
    exec.on_record(TimeVal::zero());
    spin(200);
    CHECK(started == 1);
    CHECK(exec.execution_time() > before);

    exec.request_stop();
    spin(200);
  });
}
