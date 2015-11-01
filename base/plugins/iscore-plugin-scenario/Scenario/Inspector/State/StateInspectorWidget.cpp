#include "StateInspectorWidget.hpp"
#include <Scenario/Document/State/StateModel.hpp>
#include <Inspector/InspectorSectionWidget.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <Scenario/Inspector/SelectionButton.hpp>
#include <core/document/Document.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Scenario/DialogWidget/MessageTreeView.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <Scenario/Commands/Event/SplitEvent.hpp>
#include <QPushButton>
#include <QFormLayout>
#include <QLabel>
StateInspectorWidget::StateInspectorWidget(
        const StateModel& object,
        iscore::Document& doc,
        QWidget *parent):
    InspectorWidgetBase {object, doc, parent},
    m_model {object}
{
    setObjectName("StateInspectorWidget");
    setParent(parent);

    updateDisplayedValues();
}

void StateInspectorWidget::updateDisplayedValues()
{
    // Cleanup
    // OPTIMIZEME
    qDeleteAll(m_properties);
    m_properties.clear();

    auto widget = new QWidget;
    auto lay = new iscore::MarginLess<QFormLayout>;
    widget->setLayout(lay);
    // State id
    //lay->addRow("Id", new QLabel{QString::number(m_model.id().val().get())});

    auto scenar = m_model.parentScenario();
    auto event = m_model.eventId();
    if(event)
    {
        auto btn = SelectionButton::make(
                    tr("Parent Event"),
                &scenar->event(event),
                selectionDispatcher(),
                this);

        lay->addWidget(btn);
    }

    // Constraints setup
    if(m_model.previousConstraint())
    {
        auto btn = SelectionButton::make(
                    tr("Previous Constraint"),
                &scenar->constraint(m_model.previousConstraint()),
                selectionDispatcher(),
                this);

        lay->addWidget(btn);
    }
    if(m_model.nextConstraint())
    {
        auto btn = SelectionButton::make(
                    tr("Next Constraint"),
                &scenar->constraint(m_model.nextConstraint()),
                selectionDispatcher(),
                this);

        lay->addWidget(btn);
    }

    auto newEvBtn = new QPushButton{"Split"};
    lay->addWidget(newEvBtn);

    connect(newEvBtn, &QPushButton::pressed,
            this,   &StateInspectorWidget::splitEvent);

    m_properties.push_back(widget);

    // State setup
    m_stateSection = new InspectorSectionWidget{"States", this};
    auto tv = new MessageTreeView{m_model,
                                m_stateSection};

    m_stateSection->addContent(tv);
    m_properties.push_back(m_stateSection);

    updateAreaLayout(m_properties);
}

using namespace Scenario;

void StateInspectorWidget::splitEvent()
{
    auto scenar = dynamic_cast<const ScenarioModel*>(m_model.parentScenario());
    if (scenar)
    {
        auto cmd = new Command::SplitEvent{
                    *scenar,
                    m_model.eventId(),
                    {m_model.id()}};

        commandDispatcher()->submitCommand(cmd);
    }
}
