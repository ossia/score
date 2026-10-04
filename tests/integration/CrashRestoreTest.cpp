// A document restored after a crash is the one that was open: under the name
// it was saved under, not the one it had when the crash backup started.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QApplication>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

TEST_CASE(
    "a document restored after a crash keeps the name it was saved under",
    "[integration][document][restore][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    REQUIRE(doc->metadata().fileName().startsWith("Untitled"));

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("foo.score");
    REQUIRE(ctx.docManager.saveDocumentAs(*doc, path));

    // What a restart after a crash does with the backups of the open documents.
    const auto before = ctx.docManager.documents();
    ctx.docManager.restoreDocuments(ctx);
    QApplication::processEvents();

    // Every open document is restored: the one saved is found by its file.
    score::Document* restored{};
    for(auto* d : ctx.docManager.documents())
      if(std::find(before.begin(), before.end(), d) == before.end()
         && d->metadata().fileName() == path)
        restored = d;
    REQUIRE(restored);
    auto& model = score::IDocument::get<Scenario::ScenarioDocumentModel>(*restored);
    CHECK(model.baseInterval().metadata().getName() == "foo");
  });
}

TEST_CASE(
    "a document restored after a crash keeps the name its root interval was given",
    "[integration][document][restore][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("bar.score");
    {
      auto doc = score::test::new_document(ctx);
      REQUIRE(doc);
      REQUIRE(ctx.docManager.saveDocumentAs(*doc, path));
      auto& model = score::IDocument::get<Scenario::ScenarioDocumentModel>(*doc);
      model.baseInterval().metadata().setName("My Show");
      REQUIRE(ctx.docManager.saveDocument(*doc));
      ctx.docManager.forceCloseDocument(ctx, *doc);
    }

    // Opened from its file, under the name it was saved with.
    auto opened = ctx.docManager.loadFile(ctx, path);
    REQUIRE(opened);
    QApplication::processEvents();
    CHECK(
        score::IDocument::get<Scenario::ScenarioDocumentModel>(*opened)
            .baseInterval()
            .metadata()
            .getName()
        == "My Show");

    const auto before = ctx.docManager.documents();
    ctx.docManager.restoreDocuments(ctx);
    QApplication::processEvents();

    score::Document* restored{};
    for(auto* d : ctx.docManager.documents())
      if(std::find(before.begin(), before.end(), d) == before.end()
         && d->metadata().fileName() == path)
        restored = d;
    REQUIRE(restored);
    auto& model = score::IDocument::get<Scenario::ScenarioDocumentModel>(*restored);
    CHECK(model.baseInterval().metadata().getName() == "My Show");
  });
}
