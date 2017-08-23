#include "ConstraintNode.hpp"
#include <Dataflow/DocumentPlugin.hpp>
#include <Scenario/Document/Components/ConstraintComponent.hpp>
#include <Scenario/Document/Components/ProcessComponent.hpp>
#include <Process/Process.hpp>
#include <iscore/model/Component.hpp>
#include <iscore/model/ComponentHierarchy.hpp>
#include <iscore/plugins/customfactory/ModelFactory.hpp>
#include <iscore/model/ComponentFactory.hpp>
#include <Process/Dataflow/DataflowObjects.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Dataflow/UI/Slider.hpp>
#include <Dataflow/UI/NodeItem.hpp>

namespace Dataflow
{

ConstraintBase::ConstraintBase(
    const Id<iscore::Component>& id,
    Scenario::ConstraintModel& constraint,
    Dataflow::DocumentPlugin& doc,
    QObject* parent_comp):
  parent_t{constraint, doc, id, "ConstraintComponent", parent_comp}
{
  qDebug("constraint node created");
  {
    auto item = new Dataflow::NodeItem{doc.context(), constraint.node};
    ui = item;
    constraint.node.ui = ui;
    item->setParentItem(doc.window.view.contentItem());
    item->setPosition(QPointF(rand() % 500, rand() % 500));

    doc.scenario.registerNode(ui);
  }

  {
    auto item = new NodeItem{doc.context(), constraint.slider};
    item->setParentItem(doc.window.view.contentItem());
    item->setPosition(ui->position() + QPointF{0, 10});
    slider = item;
    constraint.slider.ui = slider;

    doc.scenario.registerNode(slider);

    sliderUI = new SliderUI{};
    sliderUI->setParentItem(item);
    sliderUI->setX(6);
    sliderUI->setY(6);
    sliderUI->setWidth(item->width() - 12);
    sliderUI->setHeight(item->height() - 12);
    connect(sliderUI, &SliderUI::valueChanged,
            this, [&] (double v) {
      constraint.slider.setVolume(v);
    });
  }
}

ProcessComponent*ConstraintBase::make(
    const Id<iscore::Component>& id,
    ProcessComponentFactory& factory,
    Process::ProcessModel& process)
{
  auto comp = factory.make(process, system(), id, &process);
  return comp;
}

bool ConstraintBase::removing(
    const Process::ProcessModel& cst,
    const ProcessComponent& comp)
{
  return true;
}

}
