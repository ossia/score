#include "ScenarioViewInterface.hpp"

#include "TemporalScenarioPresenter.hpp"
#include "source/Process/ScenarioModel.hpp"
#include "source/Process/Temporal/TemporalScenarioViewModel.hpp"
#include "source/Process/Temporal/TemporalScenarioView.hpp"

#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ConstraintData.hpp"
#include "Document/Constraint/Box/BoxView.hpp"
#include "Document/Constraint/Box/BoxPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventPresenter.hpp"
#include "Document/Event/EventView.hpp"
#include "Document/Event/EventData.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"


ScenarioViewInterface::ScenarioViewInterface(TemporalScenarioPresenter* presenter) :
    m_presenter(presenter)
{

}

void ScenarioViewInterface::on_eventMoved(id_type<EventModel> eventId)
{
    auto rect = m_presenter->m_view->boundingRect();
    auto ev = findById(m_presenter->m_events, eventId);

    ev->view()->setPos({qreal(ev->model()->date().msec() / m_presenter->m_millisecPerPixel),
                        rect.height() * ev->model()->heightPercentage()
                       });

    // TODO a revoir
    if(m_presenter->ongoingCommand())
    {
        ev->view()->setMoving(true);
    }
    else
    {
        ev->view()->setMoving(false);
    }

    // @todo change when multiple event on a same timeNode
    auto timeNode = findById(m_presenter->m_timeNodes, ev->model()->timeNode());
    timeNode->view()->setPos({qreal(timeNode->model()->date().msec() / m_presenter->m_millisecPerPixel),
                              rect.height() * timeNode->model()->y()
                             });

    updateTimeNode(timeNode->id());
    m_presenter->m_view->update();
}

void ScenarioViewInterface::on_constraintMoved(id_type<ConstraintModel> constraintId)
{
    auto rect = m_presenter->m_view->boundingRect();

    for(TemporalConstraintPresenter* pres : m_presenter->m_constraints)
    {
        ConstraintModel* cstr_model {viewModel(pres)->model() };

        if(cstr_model->id() == constraintId)
        {
            auto delta = (view(pres)->x() - (qreal(cstr_model->startDate().msec()) / m_presenter->m_millisecPerPixel));
            bool dateChanged = (delta * delta > 1);

            if(dateChanged)
                view(pres)->setPos({qreal(cstr_model->startDate().msec()) / m_presenter->m_millisecPerPixel,
                                    rect.height() * cstr_model->heightPercentage()
                                   });
            else
            {
                view(pres)->setY(qreal(rect.height() * cstr_model->heightPercentage()));
            }

            view(pres)->setDefaultWidth(cstr_model->defaultDuration().msec() / m_presenter->m_millisecPerPixel);
            view(pres)->setMinWidth(cstr_model->minDuration().msec() / m_presenter->m_millisecPerPixel);
            view(pres)->setMaxWidth(cstr_model->maxDuration().msec() / m_presenter->m_millisecPerPixel);

            if(m_presenter->ongoingCommand())
            {
                view(pres)->setMoving(true);
            }
            else
            {
                view(pres)->setMoving(false);
            }

            auto endTimeNode = findById(m_presenter->m_events, cstr_model->endEvent())->model()->timeNode();
            updateTimeNode(endTimeNode);

            if(cstr_model->startDate().msec() != 0)
            {
                auto startTimeNode = findById(m_presenter->m_events, cstr_model->startEvent())->model()->timeNode();
                updateTimeNode(startTimeNode);
            }
        }
    }

    m_presenter->m_view->update();
}

void ScenarioViewInterface::updateTimeNode(id_type<TimeNodeModel> timeNodeId)
{
    auto timeNode = findById(m_presenter->m_timeNodes, timeNodeId);
    auto rect = m_presenter->m_view->boundingRect();

    double min = 1.0;
    double max = 0.0;

    for(auto eventId : timeNode->model()->events())
    {
        auto event = findById(m_presenter->m_events, eventId);
        double y = event->model()->heightPercentage();

        if(y < min)
        {
            min = y;
        }

        if(y > max)
        {
            max = y;
        }

        for(TemporalConstraintPresenter* cstr_pres : m_presenter->m_constraints)
        {
            ConstraintModel* cstr_model {cstr_pres->abstractConstraintViewModel()->model() };

            for(auto cstrId : event->model()->previousConstraints())
            {
                if(cstr_model->id() == cstrId)
                {
                    y = cstr_model->heightPercentage();

                    if(y < min)
                    {
                        min = y;
                    }

                    if(y > max)
                    {
                        max = y;
                    }
                }
            }

            for(auto cstrId : event->model()->nextConstraints())
            {
                if(cstr_model->id() == cstrId)
                {
                    y = cstr_model->heightPercentage();

                    if(y < min)
                    {
                        min = y;
                    }

                    if(y > max)
                    {
                        max = y;
                    }
                }
            }
        }

    }

    min -= timeNode->model()->y();
    max -= timeNode->model()->y();

    timeNode->view()->setExtremities(int (rect.height() * min), int (rect.height() * max));

    if(m_presenter->ongoingCommand())
    {
        timeNode->view()->setMoving(true);
    }
    else
    {
        timeNode->view()->setMoving(false);
    }
}
