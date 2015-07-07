#include "StateInspectorWidget.hpp"
#include "Document/State/DisplayedStateModel.hpp"
#include <Inspector/InspectorSectionWidget.hpp>
#include <Process/ScenarioModel.hpp>
#include <State/Widgets/StateWidget.hpp>
#include <QPushButton>
#include <QFormLayout>
StateInspectorWidget::StateInspectorWidget(const StateModel *object, QWidget *parent):
    InspectorWidgetBase {object, parent},
    m_model {object}
{
    setObjectName("StateInspectorWidget");
    setInspectedObject(object);
    setParent(parent);

    updateDisplayedValues(object);
}

void StateInspectorWidget::updateDisplayedValues(const StateModel* state)
{
    // Cleanup
    qDeleteAll(m_stateWidgets);
    m_stateWidgets.clear();


    if(state)
    {
        // Constraints setup
        auto widget = new QWidget;
        auto lay = new QFormLayout(widget);
        lay->setMargin(0);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setHorizontalSpacing(0);
        lay->setVerticalSpacing(0);

        lay->addRow("Id", new QLabel{QString::number(state->id().val().get())});

        auto scenar = state->parentScenario();
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

        for(auto& data_state : state->states())
        {
            auto widg = new StateWidget{data_state, this};
            m_stateSection->addContent(widg);

            //connect(widg, &StateInspectorWidget::removeMe,
            //        this, [&] () { removeState(state);});
        }
        m_properties.push_back(m_stateSection);
    }

    updateAreaLayout(m_properties);
}


void StateInspectorWidget::updateInspector()
{
    updateDisplayedValues(m_model);
}
