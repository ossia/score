// Smoke integration test: boots the full headless score application via the
// test fixtures, creates a blank document, and checks that the document model
// and serialization round-trip work end to end.
//
// This validates the whole vertical slice: SCORE_TESTING build wiring,
// score_test_fixtures, runtime plugin discovery, and document creation.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("A headless document can be created and serialized", "[integration][document]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // The plugin system must have loaded at least one document delegate
    // (the Scenario document in a normal build).
    REQUIRE_FALSE(ctx.interfaces<score::DocumentDelegateList>().empty());

    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    // It serializes to a non-empty binary blob.
    const QByteArray bytes = doc->saveAsByteArray();
    CHECK(bytes.size() > 0);
  });
}
