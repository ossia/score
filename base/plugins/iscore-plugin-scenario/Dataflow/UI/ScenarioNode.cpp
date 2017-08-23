#include "ScenarioNode.hpp"
#include <Dataflow/UI/ConstraintNode.hpp>
#include <Dataflow/UI/NodeItem.hpp>
#include <Dataflow/DocumentPlugin.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
namespace Dataflow
{

ScenarioBase::ScenarioBase(
        Scenario::ProcessModel &scenario,
        DocumentPlugin &doc,
        const Id<iscore::Component> &id,
        QObject *parent_obj):
    ProcessComponent_T<Scenario::ProcessModel>{scenario, doc, id, "ScenarioComponent", parent_obj}
{
  {
    auto item = new Dataflow::NodeItem{doc.context(), scenario.m_node};
    ui = item;
    scenario.m_node.ui = ui;
    doc.scenario.registerNode(ui);

    item->setParentItem(doc.window.view.contentItem());
    item->setPosition(QPointF(rand() % 500, rand() % 500));
  }
  {
    auto item = new NodeItem{doc.context(), scenario.slider};
    item->setParentItem(doc.window.view.contentItem());
    item->setPosition(ui->position() + QPointF{0, 10});
    slider = item;
    scenario.slider.ui = slider;
    doc.scenario.registerNode(slider);

    sliderUI = new SliderUI{};
    sliderUI->setParentItem(item);
    sliderUI->setX(6);
    sliderUI->setY(6);
    sliderUI->setWidth(item->width() - 12);
    sliderUI->setHeight(item->height() - 12);
    connect(sliderUI, &SliderUI::valueChanged,
            this, [&] (double v) {
      scenario.slider.setVolume(v);
    });
  }
}

Constraint *ScenarioBase::make(
        const Id<iscore::Component> &id,
        Scenario::ConstraintModel &elt)
{
    auto comp = new Constraint{id, elt, system(), this};
    setupConstraint(elt, comp);
    return comp;
}

void ScenarioBase::setupConstraint(
        Scenario::ConstraintModel &c,
        Constraint *comp)
{
}

void ScenarioBase::teardownConstraint(
        const Scenario::ConstraintModel& c, const Constraint& comp)
{
}
}
