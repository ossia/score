#include "StateInspectorWidget.hpp"
#include "Document/State/StateModel.hpp"
#include <Inspector/InspectorSectionWidget.hpp>
#include <Process/ScenarioModel.hpp>
#include <State/Widgets/StateWidget.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include "Commands/Event/RemoveStateFromEvent.hpp"
#include "Inspector/SelectionButton.hpp"
#include <QPushButton>
#include <QFormLayout>
#include <QLabel>
StateInspectorWidget::StateInspectorWidget(const StateModel *object, QWidget *parent):
    InspectorWidgetBase {object, parent},
    m_model {object}
{
    setObjectName("StateInspectorWidget");
    setParent(parent);

    // Connections
    // TODO connections not necessary since we'll have a tree widget, right ?

    updateDisplayedValues(object);
}

#include <QTreeView>
void StateInspectorWidget::updateDisplayedValues(const StateModel* state)
{
    // Cleanup
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
    lay->addRow("Id", new QLabel{QString::number(state->id().val().get())});

    auto scenar = state->parentScenario();
    auto event = m_model->eventId();
    if(event)
    {
        auto btn = SelectionButton::make(
                &scenar->event(event),
                selectionDispatcher(),
                this);

        lay->addRow(tr("Event"), btn);
    }

    // Constraints setup
    if(state->previousConstraint())
    {
        auto btn = SelectionButton::make(
                &scenar->constraint(state->previousConstraint()),
                selectionDispatcher(),
                this);

        lay->addRow(tr("Previous constraint"), btn);
    }
    if(state->nextConstraint())
    {
        auto btn = SelectionButton::make(
                &scenar->constraint(state->nextConstraint()),
                selectionDispatcher(),
                this);

        lay->addRow(tr("Next constraint"), btn);
    }
    m_properties.push_back(widget);


    // State setup
    m_stateSection = new InspectorSectionWidget{"States", this};

    auto tv = new QTreeView;
    tv->setModel(const_cast<iscore::StateItemModel*>(&m_model->states()));
    m_stateSection->addContent(tv);
    m_properties.push_back(m_stateSection);
    ISCORE_TODO;
    /*
    for(const auto& data_state : state->states())
    {
        auto widg = new StateWidget{data_state, *commandDispatcher(), this};
        m_stateSection->addContent(widg);

        connect(widg, &StateWidget::removeMe,
                this, [=] () { // note : here a copy of iscore::State is made.
            auto cmd = new Scenario::Command::RemoveStateFromStateModel{iscore::IDocument::path(m_model), data_state};
            emit commandDispatcher()->submitCommand(cmd);
        });
    }
    m_properties.push_back(m_stateSection);

*/
    updateAreaLayout(m_properties);
}
