// Smoke integration test: boots the full headless score application via the
// test fixtures, creates a blank document, and checks that the document model
// and serialization round-trip work end to end.
//
// This validates the whole vertical slice: SCORE_TESTING build wiring,
// score_test_fixtures, runtime plugin discovery, and document creation.

#include <score/tools/FilePath.hpp>

#include <QFile>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentPresenter.hpp>

#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>

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


TEST_CASE("A document that resolves paths while loading can be opened", "[document]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // Deserialization resolves paths -- a sound process turns the stored
    // <PROJECT>: reference into something it can open -- and that happens from
    // the constructors, before any post-construction setup has run. Anything
    // locateFilePath depends on has to exist by then.
    const QString path
        = QStringLiteral("%1/docs/main-page.score").arg(SCORE_ROOT_SOURCE_DIR);
    REQUIRE(QFile::exists(path));

    auto* doc = ctx.docManager.loadFile(ctx, path);
    REQUIRE(doc);

    // And the document can say where its files are, which is what the load
    // path was asking it before it had an answer.
    CHECK(doc->environment().isLocal());
    CHECK_FALSE(
        score::locateFilePath("<PROJECT>:anything", doc->context()).isEmpty());
  });
}


TEST_CASE("A document whose process has no factory can be displayed", "[document]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    // Loading a stand-in was covered; showing one was not, and showing it is
    // what an application does immediately after loading. Several places look
    // up the process factory by the key the stand-in reports -- which is the
    // absent plug-in's -- and dereference the result.
    const QString path
        = QStringLiteral("%1/tests/testdata/missing-plugin.score")
              .arg(SCORE_ROOT_SOURCE_DIR);
    REQUIRE(QFile::exists(path));

    auto* doc = ctx.docManager.loadFile(ctx, path);
    REQUIRE(doc);

    REQUIRE(doc->presenter() != nullptr);

    // Loading builds the presenter but not necessarily the layers. Ask for the
    // interval to be displayed, which is what the application does and what
    // builds them -- otherwise this test passes without reaching the code it
    // is here for.
    auto* pres = safe_cast<Scenario::ScenarioDocumentPresenter*>(
        doc->presenter()->presenterDelegate());
    REQUIRE(pres);
    auto& model = safe_cast<Scenario::ScenarioDocumentModel&>(
        doc->model().modelDelegate());

    pres->setDisplayedInterval(&model.baseScenario().interval());
    QApplication::processEvents();

    // Reached the layers: there is a process in this document and it is shown.
    CHECK(model.baseScenario().interval().processes.size() > 0);
  });
}
