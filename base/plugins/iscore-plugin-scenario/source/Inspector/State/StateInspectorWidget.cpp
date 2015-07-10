#include "StateInspectorWidget.hpp"
#include "Document/State/DisplayedStateModel.hpp"
#include <Inspector/InspectorSectionWidget.hpp>
#include <Process/ScenarioModel.hpp>
#include <State/Widgets/StateWidget.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include "Commands/Event/RemoveStateFromEvent.hpp"
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
    connect(m_model, &StateModel::stateAdded,
            this, [&] (const iscore::State&)
        { updateDisplayedValues(m_model); });

    connect(m_model, &StateModel::stateRemoved,
            this, [&] (const iscore::State&)
        { updateDisplayedValues(m_model); });

    connect(m_model, &StateModel::statesReplaced,
            this, [&] ()
        { updateDisplayedValues(m_model); });

    updateDisplayedValues(object);
}

void StateInspectorWidget::updateDisplayedValues(const StateModel* state)
{
    // Cleanup
    qDeleteAll(m_stateWidgets);
    m_stateWidgets.clear();
    qDeleteAll(m_properties);
    m_properties.clear();

    if(state) // TODO should always be true
    {
        auto widget = new QWidget;
        auto lay = new QFormLayout(widget);
        lay->setMargin(0);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setHorizontalSpacing(0);
        lay->setVerticalSpacing(0);

        // State id
        lay->addRow("Id", new QLabel{QString::number(state->id().val().get())});

        // TODO Add event id

        auto scenar = state->parentScenario();
        // Constraints setup
        if(state->previousConstraint())
        {
            auto cstr = &scenar->constraint(state->previousConstraint());
            auto cstrBtn = new QPushButton;

            cstrBtn->setText(QString::number(*cstr->id().val()));
            cstrBtn->setFlat(true);

            lay->addRow(tr("Previous constraint"), cstrBtn);

            connect(cstrBtn, &QPushButton::clicked,
                    [=]()
            {
                selectionDispatcher()->setAndCommit(Selection{cstr});
            });
        }
        if(state->nextConstraint())
        {
            auto cstr = &scenar->constraint(state->nextConstraint());
            auto cstrBtn = new QPushButton;

            cstrBtn->setText(QString::number(*cstr->id().val()));
            cstrBtn->setFlat(true);

            lay->addRow(tr("Next constraint"), cstrBtn);

            connect(cstrBtn, &QPushButton::clicked,
                    [=]()
            {
                selectionDispatcher()->setAndCommit(Selection{cstr});
            });
        }
        m_properties.push_back(widget);


        // State setup
        m_stateSection = new InspectorSectionWidget{"States", this};

        for(const auto& data_state : state->states())
        {
            auto widg = new StateWidget{data_state, this};
            m_stateSection->addContent(widg);

            connect(widg, &StateWidget::removeMe,
                    this, [=] () { // note : here a copy of iscore::State is made.
                auto cmd = new Scenario::Command::RemoveStateFromStateModel{iscore::IDocument::path(m_model), data_state};
                emit commandDispatcher()->submitCommand(cmd);
            });
        }
        m_properties.push_back(m_stateSection);
    }

    updateAreaLayout(m_properties);
}

void StateInspectorWidget::updateInspector()
{
    updateDisplayedValues(m_model);
}
