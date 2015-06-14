#include "ScenarioViewInterface.hpp"

#include "TemporalScenarioPresenter.hpp"
#include "source/Process/ScenarioModel.hpp"
#include "source/Process/Temporal/TemporalScenarioViewModel.hpp"
#include "source/Process/Temporal/TemporalScenarioView.hpp"

#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventPresenter.hpp"
#include "Document/Event/EventView.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"

#include <QGraphicsScene>

ScenarioViewInterface::ScenarioViewInterface(TemporalScenarioPresenter* presenter) :
    QObject{presenter},
    m_presenter(presenter)
{
    connect(&m_presenter->m_viewModel, &TemporalScenarioViewModel::eventMoved,
            this, &ScenarioViewInterface::on_eventMoved);

    connect(&m_presenter->m_viewModel, &TemporalScenarioViewModel::constraintMoved,
            this, &ScenarioViewInterface::on_constraintMoved);
}

void ScenarioViewInterface::on_eventMoved(const id_type<EventModel>& eventId)
{
    auto rect = m_presenter->m_view->boundingRect();
    auto ev = m_presenter->m_events.at(eventId);

    ev->view()->setPos({(ev->model().date().msec() / m_presenter->m_zoomRatio),
                        rect.height() * ev->model().heightPercentage()
                       });

    auto timeNode = m_presenter->m_timeNodes.at(ev->model().timeNode());
    timeNode->view()->setPos({(timeNode->model().date().msec() / m_presenter->m_zoomRatio),
                              rect.height() * timeNode->model().y()});

    updateTimeNode(timeNode->id());
    m_presenter->m_view->update();
}

void ScenarioViewInterface::on_constraintMoved(const id_type<ConstraintModel>& constraintId)
{
    auto rect = m_presenter->m_view->boundingRect();
    auto msPerPixel = m_presenter->m_zoomRatio;

    TemporalConstraintPresenter* pres = m_presenter->m_constraints.at(constraintId);
    const auto& cstr_model = pres->model();

    auto startPos = cstr_model.startDate().toPixels(msPerPixel);
    auto delta = view(pres)->x() - startPos;
    bool dateChanged = (delta * delta > 1); // Magnetism

    if(dateChanged)
    {
        view(pres)->setPos({startPos,
                            rect.height() * cstr_model.heightPercentage()});
    }
    else
    {
        view(pres)->setY(qreal(rect.height() * cstr_model.heightPercentage()));
    }

    view(pres)->setDefaultWidth(cstr_model.defaultDuration().toPixels(msPerPixel));
    view(pres)->setMinWidth(cstr_model.minDuration().toPixels(msPerPixel));
    view(pres)->setMaxWidth(cstr_model.maxDuration().isInfinite(),
                            cstr_model.maxDuration().isInfinite()? -1 :
                                                                   cstr_model.maxDuration().toPixels(msPerPixel));

    auto endTimeNode = m_presenter->m_events.at(cstr_model.endEvent())->model().timeNode();
    updateTimeNode(endTimeNode);

    auto startTimeNode = m_presenter->m_events.at(cstr_model.startEvent())->model().timeNode();
    updateTimeNode(startTimeNode);

    m_presenter->m_view->update();
}

template<typename T>
void update_min_max(const T& val, T& min, T& max)
{
    min = val < min ? val : min;
    max = val > max ? val : max;
}

void ScenarioViewInterface::updateTimeNode(const id_type<TimeNodeModel>& timeNodeId)
{
    auto timeNode = m_presenter->m_timeNodes.at(timeNodeId);
    auto rect = m_presenter->m_view->boundingRect();

    double min = 1.0;
    double max = 0.0;

    for(const auto& eventId : timeNode->model().events())
    {
        EventPresenter* event = m_presenter->m_events.at(eventId);

        double y = event->model().heightPercentage();

        update_min_max(y, min, max);

        for(const auto& constraint_id : event->model().constraints())
        {
            auto cstr_pres = m_presenter->m_constraints.at(constraint_id);
            y = cstr_pres->model().heightPercentage(); // TODO here we have to find the length of the current box.
            update_min_max(y, min, max);
        }
    }

    min -= timeNode->model().y();
    max -= timeNode->model().y();

    timeNode->view()->setExtremities(int (rect.height() * min), int (rect.height() * max));
}

void ScenarioViewInterface::on_hoverOnConstraint(const id_type<ConstraintModel>& constraintId, bool enter)
{
    const auto& constraint = m_presenter->m_constraints.at(constraintId)->model();
    EventPresenter* start = m_presenter->m_events.at(constraint.startEvent());
    start->view()->setShadow(enter);
    EventPresenter* end = m_presenter->m_events.at(constraint.endEvent());
    end->view()->setShadow(enter);
}

void ScenarioViewInterface::on_hoverOnEvent(const id_type<EventModel>& eventId, bool enter)
{
    const auto& event = m_presenter->m_events.at(eventId)->model();
    for (const auto& cstr : event.constraints())
    {
        auto cstrView = view(m_presenter->m_constraints.at(cstr));
        cstrView->setShadow(enter);
    }
}
