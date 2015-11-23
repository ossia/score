#include "StateInspectorWidget.hpp"
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Commands/Event/SplitEvent.hpp>
#include <Scenario/Inspector/SelectionButton.hpp>
#include <Scenario/DialogWidget/MessageTreeView.hpp>

#include <Inspector/InspectorSectionWidget.hpp>
#include <core/document/Document.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/document/DocumentInterface.hpp>

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

    // State setup
    m_stateSection = new InspectorSectionWidget{"States", false, this};
    auto tv = new MessageTreeView{m_model,
                                m_stateSection};

    m_stateSection->addContent(tv);
    m_properties.push_back(m_stateSection);

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

    auto scenarModel = dynamic_cast<const ScenarioModel*>(m_model.parentScenario());
    auto& parentEvent = scenarModel->events.at(m_model.eventId());

    if(parentEvent.states().size() > 1)
    {
        auto newEvBtn = new QPushButton{"Split"};
        lay->addWidget(newEvBtn);

        connect(newEvBtn, &QPushButton::pressed,
                this,   &StateInspectorWidget::splitEvent);
    }

    m_properties.push_back(widget);

    updateAreaLayout(m_properties);
}

using namespace Scenario;

void StateInspectorWidget::splitEvent()
{
    auto scenar = dynamic_cast<const ScenarioModel*>(m_model.parentScenario());
    if (scenar)
    {
        auto& parentEvent = scenar->events.at(m_model.eventId());
        if(parentEvent.states().size() > 1)
        {
            auto cmd = new Scenario::Command::SplitEvent{
                        *scenar,
                        m_model.eventId(),
                        {m_model.id()}};

            commandDispatcher()->submitCommand(cmd);
            updateDisplayedValues(); // TODO : boarf boarf
        }
    }
}
