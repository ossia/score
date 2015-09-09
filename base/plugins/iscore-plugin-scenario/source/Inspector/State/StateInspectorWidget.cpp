#include "StateInspectorWidget.hpp"
#include "Document/State/StateModel.hpp"
#include <Inspector/InspectorSectionWidget.hpp>
#include <Process/ScenarioModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include "Inspector/SelectionButton.hpp"
#include <core/document/Document.hpp>
#include <DeviceExplorer/../Plugin/Panel/DeviceExplorerModel.hpp>
#include "DialogWidget/StateTreeView.hpp"
#include <iscore/widgets/MarginLess.hpp>
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
    qDeleteAll(m_stateWidgets);
    m_stateWidgets.clear();
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
    m_properties.push_back(widget);


    // State setup
    m_stateSection = new InspectorSectionWidget{"States", this};

    auto deviceexplorer = iscore::IDocument::documentFromObject(m_model)
            ->findChild<DeviceExplorerModel*>("DeviceExplorerModel");
    auto tv = new StateTreeView{m_model,
                                deviceexplorer,
                                m_stateSection};

    m_stateSection->addContent(tv);
    m_properties.push_back(m_stateSection);

    updateAreaLayout(m_properties);
}
