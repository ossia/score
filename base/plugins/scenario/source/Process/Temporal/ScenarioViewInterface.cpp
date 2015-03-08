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

#include "QRectF"
#include "QPainterPath"
#include "QGraphicsScene"
#include <iscore/selection/Selection.hpp>

ScenarioViewInterface::ScenarioViewInterface(TemporalScenarioPresenter* presenter) :
    QObject{presenter},
    m_presenter(presenter)
{
    connect(m_presenter->m_viewModel, &TemporalScenarioViewModel::eventMoved,
            [ = ](id_type<EventModel> eventId)
    {
        on_eventMoved(eventId);
    });
    connect(m_presenter->m_viewModel, &TemporalScenarioViewModel::constraintMoved,
            [ = ](id_type<ConstraintModel> constraintId)
    {
        on_constraintMoved(constraintId);
    });
}

void ScenarioViewInterface::on_eventMoved(id_type<EventModel> eventId)
{
    auto rect = m_presenter->m_view->boundingRect();
    auto ev = findById(m_presenter->m_events, eventId);

    ev->view()->setPos({qreal(ev->model()->date().msec() / m_presenter->m_zoomRatio),
                        rect.height() * ev->model()->heightPercentage()
                       });

    // TODO a revoir
    ev->view()->setMoving(m_presenter->ongoingCommand());

    // @todo change when multiple event on a same timeNode
    auto timeNode = findById(m_presenter->m_timeNodes, ev->model()->timeNode());
    timeNode->view()->setPos({qreal(timeNode->model()->date().msec() / m_presenter->m_zoomRatio),
                              rect.height() * timeNode->model()->y()});

    updateTimeNode(timeNode->id());
    m_presenter->m_view->update();
}

void ScenarioViewInterface::on_constraintMoved(id_type<ConstraintModel> constraintId)
{
    auto rect = m_presenter->m_view->boundingRect();
    auto msPerPixel = m_presenter->m_zoomRatio;

    for(TemporalConstraintPresenter* pres : m_presenter->m_constraints)
    {
        ConstraintModel* cstr_model {viewModel(pres)->model() };

        if(cstr_model->id() == constraintId)
        {
            auto startPos = cstr_model->startDate().toPixels(msPerPixel);
            auto delta = view(pres)->x() - startPos;
            bool dateChanged = (delta * delta > 1); // TODO : jm : what????

            if(dateChanged)
            {
                view(pres)->setPos({startPos,
                                    rect.height() * cstr_model->heightPercentage()});
            }
            else
            {
                view(pres)->setY(qreal(rect.height() * cstr_model->heightPercentage()));
            }

            view(pres)->setDefaultWidth(cstr_model->defaultDuration().toPixels(msPerPixel));
            view(pres)->setMinWidth(cstr_model->minDuration().toPixels(msPerPixel));
            view(pres)->setMaxWidth(cstr_model->maxDuration().toPixels(msPerPixel));

            view(pres)->setMoving(m_presenter->ongoingCommand());

            auto endTimeNode = findById(m_presenter->m_events, cstr_model->endEvent())->model()->timeNode();
            updateTimeNode(endTimeNode);

            if(cstr_model->startDate().isZero())
            {
                auto startTimeNode = findById(m_presenter->m_events, cstr_model->startEvent())->model()->timeNode();
                updateTimeNode(startTimeNode);
            }
        }
    }

    m_presenter->m_view->update();
}

template<typename T>
void update_min_max(const T& val, T& min, T& max)
{
    min = val < min ? val : min;
    max = val > max ? val : max;
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

        update_min_max(y, min, max);

        for(TemporalConstraintPresenter* cstr_pres : m_presenter->m_constraints)
        {
            ConstraintModel* cstr_model{cstr_pres->model()};

            auto constraints = event->model()->previousConstraints() + event->model()->nextConstraints();
            for(auto cstrId : constraints)
            {
                if(cstr_model->id() == cstrId)
                {
                    y = cstr_model->heightPercentage();
                    update_min_max(y, min, max);
                }
            }
        }

    }

    min -= timeNode->model()->y();
    max -= timeNode->model()->y();

    timeNode->view()->setExtremities(int (rect.height() * min), int (rect.height() * max));

    timeNode->view()->setMoving(m_presenter->ongoingCommand());
}
