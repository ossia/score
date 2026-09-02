// DEFECT PIN (OPEN-7), expected-red until fixed. Its own single-test
// executable, because the defect is a SIGABRT.
//
// A QMimeData that declares score::mime::nodelist() but carries an EMPTY
// payload takes Gfx::Filter::VideoTextureDropHandler::dropCustom straight into
// Mime<Device::FreeNodeList>::Deserializer::deserialize(), which hands "" to
// rapidjson and trips its IsArray() assertion -- SIGABRT, no recoverable
// error. Any drag source can produce that mime (an interrupted drag, another
// application echoing the type with no data), and the cost is the whole
// application.
//
// GfxProcessLibrary.cpp:313 recorded this as un-encodable "even as
// [!shouldfail]" because the abort kills the binary. Two things fix that:
//
//   1. the drop runs in a FORKED CHILD, so the abort kills the child and the
//      test observes its wait status. (CMake's WILL_FAIL alone would not do:
//      CTest does not invert abnormal terminations -- a SIGABRT stays failed
//      under WILL_FAIL -- so the child process is what turns the crash into
//      an assertable value.)
//   2. the case asserts the CORRECT behaviour -- the child survives the drop
//      and reports zero drops -- and is tagged [!shouldfail], so today's
//      SIGABRT is failed-as-expected, and the day the deserializer refuses an
//      empty payload gracefully the tag has to come off. Never the reverse:
//      asserting "it aborts" would bless the defect.
//
// The child only touches the drop handler (JSON parsing, no event loop), so
// running it post-fork without exec is safe.

#include "GfxProcessDoc.hpp"

#include <Process/Drop/ProcessDropHandler.hpp>

#include <Device/Node/NodeListMimeSerialization.hpp>

#include <QMimeData>

#include <catch2/catch_test_macros.hpp>

#if defined(__unix__)
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace
{
constexpr auto UUID_D_FILTER_TEX = "e9bf6cf8-c872-4638-b98a-ed76edc8e2dd";

Process::ProcessDropHandler*
dropper(const score::GUIApplicationContext& ctx, const char* uuid)
{
  return ctx.interfaces<Process::ProcessDropHandlerList>().get(
      UuidKey<Process::ProcessDropHandler>::fromString(QString::fromUtf8(uuid)));
}
}

#if defined(__unix__)
TEST_CASE(
    "an empty nodelist payload must be refused, not abort the app",
    "[gfx][library][gui][!shouldfail]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* h = dropper(ctx, UUID_D_FILTER_TEX);
    REQUIRE(h != nullptr);
    REQUIRE(h->mimeTypes().contains(score::mime::nodelist()));

    const pid_t pid = ::fork();
    REQUIRE(pid >= 0);
    if(pid == 0)
    {
      // Child: the drop under test. _exit codes: 0 = handled gracefully with
      // no drops (the correct answer for an empty payload), 3 = it produced
      // drops from nothing (also wrong). A SIGABRT never reaches _exit.
      QMimeData mime;
      mime.setData(score::mime::nodelist(), QByteArray{});
      std::vector<Process::ProcessDropHandler::ProcessDrop> drops;
      h->getCustomDrops(drops, mime, doc->context());
      ::_exit(drops.empty() ? 0 : 3);
    }

    int status = 0;
    REQUIRE(::waitpid(pid, &status, 0) == pid);

    // CORRECT behaviour: the child survives and reports no drops. Today it is
    // killed by rapidjson's IsArray() assertion instead, which is the
    // expected-red this file exists to keep visible.
    INFO(
        "child status: exited=" << WIFEXITED(status) << " code="
                                << (WIFEXITED(status) ? WEXITSTATUS(status) : -1)
                                << " signaled=" << WIFSIGNALED(status) << " sig="
                                << (WIFSIGNALED(status) ? WTERMSIG(status) : 0));
    REQUIRE(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 0);
  });
}
#else
TEST_CASE("an empty nodelist payload must be refused, not abort the app")
{
  SKIP("fork-based child isolation is a unix-only harness");
}
#endif
