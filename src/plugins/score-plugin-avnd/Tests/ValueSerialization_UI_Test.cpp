#include <Process/Dataflow/Port.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>

#include <score/graphics/layouts/GraphicsTabLayout.hpp>
#include <score/graphics/widgets/QGraphicsEnum.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

namespace
{
struct Scene final : QGraphicsScene
{
  using QGraphicsScene::sendEvent;
};
}

TEST_CASE(
    "Format alone selects panes through restore, clicks and undo", "[avnd][codec][ui]")
{
  const auto uuid = GENERATE(
      "5d265cd1-014f-48bb-9c51-d594c8b58b62", "5d265cd1-014f-48bb-9c51-d594c8b58b61");
  score::test::run_in_app([uuid](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto process = score::test::add_process(*doc, uuid, {});
    REQUIRE(process);
    Process::ControlInlet* format{};
    for(auto* inlet : process->inlets())
      if(inlet->name() == "Format")
        format = dynamic_cast<Process::ControlInlet*>(inlet);
    REQUIRE(format);
    format->setValue(std::string{"Binary"});

    Process::DataflowManager dataflow;
    FocusDispatcher focus;
    Process::Context processContext{doc->context(), dataflow, focus};
    auto factory
        = ctx.interfaces<Process::LayerFactoryList>().get(process->concreteKey());
    REQUIRE(factory);
    Scene scene;
    auto root = factory->makeItem(*process, processContext, nullptr);
    REQUIRE(root);
    scene.addItem(root);
    score::GraphicsTabLayout* tabs{};
    for(auto* item : scene.items())
      if(auto* layout = dynamic_cast<score::GraphicsTabLayout*>(item))
        tabs = layout;
    REQUIRE(tabs);
    CHECK(tabs->currentIndex() == 3);

    score::QGraphicsEnum* bar{};
    for(auto* item : scene.items())
      if(auto* selector = dynamic_cast<score::QGraphicsEnum*>(item))
        if(selector->array == std::vector<QString>{"JSON", "CBOR", "Text", "Binary"})
        {
          // Only the real Format control remains, not a duplicate tab selector.
          REQUIRE(bar == nullptr);
          bar = selector;
        }
    REQUIRE(bar);
    CHECK(bar->isVisible());
    CHECK(bar->parentItem() != tabs);
    for(auto type : {QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseRelease})
    {
      QGraphicsSceneMouseEvent event{type};
      event.setButton(Qt::LeftButton);
      event.setButtons(
          type == QEvent::GraphicsSceneMousePress ? Qt::LeftButton : Qt::NoButton);
      event.setPos({3., 3.});
      event.setScenePos(bar->mapToScene(event.pos()));
      scene.sendEvent(bar, &event);
    }
    CHECK(format->value() == ossia::value{std::string{"JSON"}});
    CHECK(tabs->currentIndex() == 0);
    doc->commandStack().undo();
    CHECK(format->value() == ossia::value{std::string{"Binary"}});
    CHECK(tabs->currentIndex() == 3);
    format->setValue(std::string{"CBOR"});
    CHECK(tabs->currentIndex() == 1);
    tabs->layout();
    CHECK(tabs->currentIndex() == 1);
  });
}
