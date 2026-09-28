// Clicking a process's script button selects the process, as a click on the
// rest of it does.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Effect/EffectLayer.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/graphics/widgets/QGraphicsPixmapToggle.hpp>
#include <score/selection/SelectionStack.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QApplication>
#include <QPointF>

#include <catch2/catch_test_macros.hpp>

#include <memory>

TEST_CASE("Clicking a process's script button selects it", "[integration][gui][selection]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& interval
        = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
              .baseInterval();

    // A Javascript process: its script can be edited
    auto& factories = ctx.interfaces<Process::ProcessFactoryList>();
    auto* factory = factories.get(UuidKey<Process::ProcessModel>::fromString(
        QStringLiteral("846a5de5-47f9-46c5-a898-013cb20951d0")));
    REQUIRE(factory);
    CommandDispatcher<>{doc->context().commandStack}
        .submit<Scenario::Command::AddOnlyProcessToInterval>(
            interval, factory->concreteKey(), factory->customConstructionData(),
            QPointF{});
    Process::ProcessModel* proc{};
    for(auto& p : interval.processes)
      if(p.flags() & Process::ProcessFlags::ScriptEditingSupported)
        proc = &p;
    REQUIRE(proc);

    auto& stack = doc->context().selectionStack;
    stack.deselect();
    REQUIRE(stack.currentSelection().empty());

    QObject owner;
    std::unique_ptr<QGraphicsItem> btn{
        Process::makeScriptButton(*proc, doc->context(), &owner, nullptr)};
    auto* toggle = dynamic_cast<score::QGraphicsPixmapToggle*>(btn.get());
    REQUIRE(toggle);

    // What a click on it emits; false: nothing to open in a test
    toggle->toggled(false);
    QApplication::processEvents();

    REQUIRE(stack.currentSelection().size() == 1);
    CHECK(stack.currentSelection().at(0) == proc);
  });
}
