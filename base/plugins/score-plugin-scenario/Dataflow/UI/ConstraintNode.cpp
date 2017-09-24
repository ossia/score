#include "ConstraintNode.hpp"
#include <Dataflow/DocumentPlugin.hpp>
#include <Scenario/Document/Components/IntervalComponent.hpp>
#include <Scenario/Document/Components/ProcessComponent.hpp>
#include <Process/Process.hpp>
#include <score/model/Component.hpp>
#include <score/model/ComponentHierarchy.hpp>
#include <score/plugins/customfactory/ModelFactory.hpp>
#include <score/model/ComponentFactory.hpp>
#include <Process/Dataflow/DataflowObjects.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Dataflow/UI/Slider.hpp>
#include <Dataflow/UI/NodeItem.hpp>
/*
namespace Dataflow
{

IntervalBase::IntervalBase(
    const Id<score::Component>& id,
    Scenario::IntervalModel& interval,
    Dataflow::DocumentPlugin& doc,
    QObject* parent_comp):
  parent_t{interval, doc, id, "IntervalComponent", parent_comp}
{
  qDebug("interval node created");
  {
    auto item = new Dataflow::NodeItem{doc.context(), interval.node};
    ui = item;
    interval.node.ui = ui;
    item->setParentItem(doc.window.view.contentItem());
    item->setPosition(QPointF(rand() % 500, rand() % 500));

    doc.scenario.registerNode(ui);
  }

  {
    auto item = new NodeItem{doc.context(), interval.slider};
    item->setParentItem(doc.window.view.contentItem());
    item->setPosition(ui->position() + QPointF{0, 10});
    slider = item;
    interval.slider.ui = slider;

    doc.scenario.registerNode(slider);

    sliderUI = new SliderUI{};
    sliderUI->setParentItem(item);
    sliderUI->setX(6);
    sliderUI->setY(6);
    sliderUI->setWidth(item->width() - 12);
    sliderUI->setHeight(item->height() - 12);
    connect(sliderUI, &SliderUI::valueChanged,
            this, [&] (double v) {
      interval.slider.setVolume(v);
    });
  }
}

ProcessComponent*IntervalBase::make(
    const Id<score::Component>& id,
    ProcessComponentFactory& factory,
    Process::ProcessModel& process)
{
  auto comp = factory.make(process, system(), id, &process);
  return comp;
}

bool IntervalBase::removing(
    const Process::ProcessModel& cst,
    const ProcessComponent& comp)
{
  return true;
}

}
*/
