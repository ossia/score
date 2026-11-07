#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/document/DocumentInterface.hpp>

#include <catch2/catch_all.hpp>

TEST_CASE("a new document syncs its root interval on the bar", "[integration][document]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& root = score::IDocument::get<Scenario::ScenarioDocumentModel>(*doc).baseInterval();
    CHECK(root.quantizationRate() == 1.);

    // Saved and loaded, in both formats, the document keeps what it had
    root.setQuantizationRate(-1.);
    for(auto* reloaded :
        {score::test::reload_via_json(ctx, *doc), score::test::reload_via_bytes(ctx, *doc)})
    {
      REQUIRE(reloaded);
      auto& r
          = score::IDocument::get<Scenario::ScenarioDocumentModel>(*reloaded).baseInterval();
      CHECK(r.quantizationRate() == -1.);
    }
  });
}
