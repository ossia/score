// Integration test for a property command (rename) + undo/redo. Renaming an
// interval only touches metadata, so it runs headless.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Scenario/Commands/Metadata/ChangeElementName.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/model/ModelMetadata.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Renaming the base interval is undoable", "[integration][command][undo]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto& interval
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();

    const QString original = interval.metadata().getName();
    const QString renamed = QStringLiteral("Test Interval");
    REQUIRE(original != renamed);

    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Scenario::Command::ChangeElementName<Scenario::IntervalModel>>(
        interval, renamed);
    CHECK(interval.metadata().getName() == renamed);

    score::CommandStack& stack = doc->commandStack();
    REQUIRE(stack.canUndo());
    stack.undo();
    CHECK(interval.metadata().getName() == original);

    stack.redo();
    CHECK(interval.metadata().getName() == renamed);
  });
}
