// OPEN-7, fixed. Its own single-test executable because the defect was a
// SIGABRT and the child-fork isolation must stay to guard against a regress.
//
// A QMimeData that declares score::mime::nodelist() but carries an unusable
// payload takes Gfx::Filter::VideoTextureDropHandler::dropCustom straight into
// Mime<Device::FreeNodeList>::Deserializer::deserialize() and into one of
// rapidjson's assertions -- SIGABRT, no recoverable error. Any drag source can
// produce that mime (an interrupted drag, another application echoing the type
// with no data, a stale clipboard), and the cost is the whole application.
//
// There are TWO levels of it. The original OPEN-7 report was an EMPTY payload,
// which is not a JSON array, caught by IsArray(). The array's CONTENTS are the
// same hazard and that guard does not reach them: rapidjson's operator[]
// asserts IsObject() on each element and asserts again when the requested
// member is absent, so `[1,2,3]` and `[{}]` -- both perfectly well-formed JSON
// arrays -- abort just as hard. This case covers both levels.
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
//      and reports zero drops. The deserializer now refuses a non-array
//      payload (returns an empty list) instead of tripping rapidjson's
//      IsArray() assertion, so the child exits 0 and the case runs green.
//      The child-process isolation is kept so a regression re-abort is
//      caught as a wait-status, not a killed test binary.
//
// The child only touches the drop handler (JSON parsing, no event loop), so
// running it post-fork without exec is safe.

#include "GfxProcessDoc.hpp"

#include <Process/Drop/ProcessDropHandler.hpp>

#include <Device/Node/NodeListMimeSerialization.hpp>

#include <QMimeData>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

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
    "a malformed nodelist payload must be refused, not abort the app",
    "[gfx][library][gui]")
{
  // Every one of these declares score::mime::nodelist() and carries something
  // a drag source can plausibly produce. The empty payload is the original
  // OPEN-7 defect (json.IsArray() false). The rest are the SAME defect one
  // level down and were NOT covered by that fix: the guard admitted the array
  // and rapidjson then asserted on its CONTENTS -- operator[] asserts
  // IsObject() on the element and asserts again when the member is absent.
  const auto [label, payload] = GENERATE(
      table<const char*, QByteArray>(
          {{"empty", QByteArray{}},
           {"whitespace", QByteArray{"   "}},
           {"not json at all", QByteArray{"<html>nope</html>"}},
           {"a json object, not an array", QByteArray{R"({"Address": {}})"}},
           {"an array of numbers", QByteArray{"[1,2,3]"}},
           {"an array of empty objects", QByteArray{"[{},{}]"}},
           {"entries missing Node", QByteArray{R"([{"Address": {}}])"}},
           {"entries missing Address", QByteArray{R"([{"Node": {}}])"}}}));

  INFO("payload: " << label);

  score::test::run_in_gui_app([&, payload = payload](
                                  const score::GUIApplicationContext& ctx) {
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
      // no drops (the correct answer for an unusable payload), 3 = it produced
      // drops from nothing (also wrong). A SIGABRT never reaches _exit.
      QMimeData mime;
      mime.setData(score::mime::nodelist(), payload);
      std::vector<Process::ProcessDropHandler::ProcessDrop> drops;
      h->getCustomDrops(drops, mime, doc->context());
      ::_exit(drops.empty() ? 0 : 3);
    }

    int status = 0;
    REQUIRE(::waitpid(pid, &status, 0) == pid);

    // CORRECT behaviour: the child survives and reports no drops. Before the
    // fix it was killed by one of rapidjson's assertions; the deserializer now
    // refuses an unusable payload -- and an unreadable entry inside a readable
    // array -- with a qWarning instead.
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
TEST_CASE("a malformed nodelist payload must be refused, not abort the app")
{
  SKIP("fork-based child isolation is a unix-only harness");
}
#endif
