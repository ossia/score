// Integration test: a document must survive a save -> load -> save cycle
// unchanged. This exercises the full serialization path of the document model
// and all of its document plugins.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <core/document/Document.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("A document round-trips through binary serialization", "[integration][serialization]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    const QByteArray original = doc->saveAsByteArray();
    REQUIRE(original.size() > 0);

    score::Document* reloaded = score::test::reload_via_bytes(ctx, *doc);
    REQUIRE(reloaded != nullptr);

    // Re-serializing the reloaded document must yield identical bytes:
    // save -> load -> save is a fixed point.
    CHECK(reloaded->saveAsByteArray() == original);
  });
}
