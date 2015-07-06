#include "StateInspectorWidget.hpp"
#include "Document/State/DisplayedStateModel.hpp"
#include <Inspector/InspectorSectionWidget.hpp>
#include <Process/ScenarioModel.hpp>
#include <QPushButton>
#include <QFormLayout>
StateInspectorWidget::StateInspectorWidget(const StateModel *object, QWidget *parent):
    InspectorWidgetBase {object, parent},
    m_model {object}
{
    setObjectName("StateInspectorWidget");
    setInspectedObject(object);
    setParent(parent);

    // States
    //m_stateSection = new InspectorSectionWidget{"States", this};

    //m_properties.push_back(m_stateSection);

    updateDisplayedValues(object);
}

void StateInspectorWidget::updateDisplayedValues(const StateModel* state)
{
    // Cleanup
    qDeleteAll(m_stateWidgets);
    m_stateWidgets.clear();

    auto constraintsWidget = new QWidget;
    auto lay = new QFormLayout(constraintsWidget);
    lay->setMargin(0);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setHorizontalSpacing(0);
    lay->setVerticalSpacing(0);

    if(state)
    {
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
    }
    m_properties.push_back(constraintsWidget);

    updateAreaLayout(m_properties);
    /*
   if(timeNode)
    {
        m_date->setText(QString::number(m_model->date().msec()));

        for(const auto& event : timeNode->events())
        {
            ScenarioModel* scenar = timeNode->parentScenario();
            EventModel* evModel {};
            if (scenar)
                evModel = &scenar->event(event);

            auto eventWid = new EventShortCut(QString::number((*event.val())));

            m_events.push_back(eventWid);
            m_eventList->addContent(eventWid);

            connect(eventWid, &EventShortCut::eventSelected,
                    [=]()
            {
                selectionDispatcher()->setAndCommit(Selection{evModel});
            });
        }
    }
    */
}


void StateInspectorWidget::updateInspector()
{
    updateDisplayedValues(m_model);
}
