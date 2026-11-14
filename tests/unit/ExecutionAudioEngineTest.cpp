// App test: the playing document and the audio engine it plays in.
//
// What the transport shows and what the audio thread runs must agree: a
// document "playing" while the audio thread runs the pause tick is silent,
// holds the previous engine's device tree, and the next reconnection of its
// audio device then waits for an execution queue nothing drains.
//
// Everything that replaces or parks the engine - the Restart Audio action,
// switching documents (the engine is rebound to the new document's audio
// device), the device failing - stops the playback first; closing another
// document leaves it alone.

#include <Audio/AudioApplicationPlugin.hpp>
#include <Audio/AudioTick.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

namespace
{
void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
    QApplication::processEvents(QEventLoop::AllEvents, 10);
}

bool audioRunsExecution()
{
  return Audio::execution_status.load() == ossia::transport_status::playing;
}

bool isPlaying(score::Document& doc)
{
  return doc.context().plugin<Execution::DocumentPlugin>().isPlaying();
}

Execution::ExecutionController& execution(const score::GUIApplicationContext& ctx)
{
  return ctx.guiApplicationPlugin<Engine::ApplicationPlugin>().execution();
}

void play(const score::GUIApplicationContext& ctx, score::Document& doc)
{
  ctx.docManager.setCurrentDocument(ctx, &doc);
  execution(ctx).request_play_global(true);
  spin(300);
  REQUIRE(isPlaying(doc));
  REQUIRE(audioRunsExecution());
}
}

TEST_CASE("Restarting the audio engine stops the playback first", "[audio][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& audio = ctx.guiApplicationPlugin<Audio::ApplicationPlugin>();
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    play(ctx, *doc);

    QAction* restart{};
    for(auto act : audio.findChildren<QAction*>())
      if(act->text() == QObject::tr("Restart Audio"))
        restart = act;
    REQUIRE(restart);
    const auto before = audio.audio;
    restart->trigger();
    spin(300);

    REQUIRE(audio.audio);
    CHECK(audio.audio != before);
    CHECK(isPlaying(*doc) == audioRunsExecution());
    CHECK(!isPlaying(*doc));
  });
}

TEST_CASE("Closing another document leaves the playback alone", "[audio][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto a = score::test::new_document(ctx);
    auto b = score::test::new_document(ctx);
    REQUIRE(a);
    REQUIRE(b);
    play(ctx, *a);

    ctx.docManager.forceCloseDocument(ctx, *b);
    spin(300);

    CHECK(isPlaying(*a));
    CHECK(audioRunsExecution());
  });
}

TEST_CASE("Switching documents stops the playing one first", "[audio][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto a = score::test::new_document(ctx);
    auto b = score::test::new_document(ctx);
    REQUIRE(a);
    REQUIRE(b);
    play(ctx, *a);

    ctx.docManager.setCurrentDocument(ctx, b);
    spin(300);

    CHECK(isPlaying(*a) == audioRunsExecution());
    CHECK(!isPlaying(*a));
    CHECK(!isPlaying(*b));

    // The new document plays
    play(ctx, *b);
    CHECK(isPlaying(*b));
  });
}
