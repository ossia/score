#include "StateInspectorWidget.hpp"
#include "Document/State/StateModel.hpp"
#include <Inspector/InspectorSectionWidget.hpp>
#include <Process/ScenarioModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include "Inspector/SelectionButton.hpp"
#include <core/document/Document.hpp>
#include <DeviceExplorer/../Plugin/Panel/DeviceExplorerModel.hpp>
#include "DialogWidget/StateTreeView.hpp"
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

    // Connections
    // TODO connections not necessary since we'll have a tree widget, right ?

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
    auto lay = new QFormLayout(widget);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setHorizontalSpacing(0);
    lay->setVerticalSpacing(0);

    // State id
    lay->addRow("Id", new QLabel{QString::number(m_model.id().val().get())});

    auto scenar = m_model.parentScenario();
    auto event = m_model.eventId();
    if(event)
    {
        auto btn = SelectionButton::make(
                &scenar->event(event),
                selectionDispatcher(),
                this);

        lay->addRow(tr("Event"), btn);
    }

    // Constraints setup
    if(m_model.previousConstraint())
    {
        auto btn = SelectionButton::make(
                &scenar->constraint(m_model.previousConstraint()),
                selectionDispatcher(),
                this);

        lay->addRow(tr("Previous constraint"), btn);
    }
    if(m_model.nextConstraint())
    {
        auto btn = SelectionButton::make(
                &scenar->constraint(m_model.nextConstraint()),
                selectionDispatcher(),
                this);

        lay->addRow(tr("Next constraint"), btn);
    }
    m_properties.push_back(widget);


    // State setup
    m_stateSection = new InspectorSectionWidget{"States", this};

    auto deviceexplorer = iscore::IDocument::documentFromObject(m_model)->findChild<DeviceExplorerModel*>("DeviceExplorerModel");
    auto tv = new StateTreeView{m_model,
                                deviceexplorer,
                                m_stateSection};

    m_stateSection->addContent(tv);
    m_properties.push_back(m_stateSection);

    updateAreaLayout(m_properties);
}
