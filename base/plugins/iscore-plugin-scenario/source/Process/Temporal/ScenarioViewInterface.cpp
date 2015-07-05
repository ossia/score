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
}

void ScenarioViewInterface::on_eventMoved(const EventPresenter& ev)
{
    auto h = m_presenter->m_view->boundingRect().height();

    ev.view()->setExtent(ev.model().extent() * h);

    ev.view()->setPos({ev.model().date().msec() / m_presenter->m_zoomRatio,
                             ev.model().extent().top() * h});

    // We also have to move all the relevant states
    for(const auto& state : ev.model().states())
    {
        on_stateMoved(*m_presenter->m_displayedStates.at(state));
    }
    m_presenter->m_view->update();
}

void ScenarioViewInterface::on_constraintMoved(const TemporalConstraintPresenter& pres)
{
    auto rect = m_presenter->m_view->boundingRect();
    auto msPerPixel = m_presenter->m_zoomRatio;

    const auto& cstr_model = pres.model();
    auto& cstr_view = view(pres);

    auto startPos = cstr_model.startDate().toPixels(msPerPixel);
    auto delta = cstr_view.x() - startPos;
    bool dateChanged = (delta * delta > 1); // Magnetism

    if(dateChanged)
    {
        cstr_view.setPos({startPos,
                            rect.height() * cstr_model.heightPercentage()});
    }
    else
    {
        cstr_view.setY(qreal(rect.height() * cstr_model.heightPercentage()));
    }

    cstr_view.setDefaultWidth(cstr_model.defaultDuration().toPixels(msPerPixel));
    cstr_view.setMinWidth(cstr_model.minDuration().toPixels(msPerPixel));
    cstr_view.setMaxWidth(cstr_model.maxDuration().isInfinite(),
                            cstr_model.maxDuration().isInfinite()? -1 :
                                                                   cstr_model.maxDuration().toPixels(msPerPixel));

    m_presenter->m_view->update();
}

void ScenarioViewInterface::on_timeNodeMoved(const TimeNodePresenter &timenode)
{
    auto h = m_presenter->m_view->boundingRect().height();
    timenode.view()->setExtent(timenode.model().extent() * h);

    timenode.view()->setPos({timenode.model().date().msec() / m_presenter->m_zoomRatio,
                             timenode.model().extent().top() * h});

    m_presenter->m_view->update();
}

void ScenarioViewInterface::on_stateMoved(const StatePresenter& state)
{
    auto rect = m_presenter->m_view->boundingRect();
    const auto& ev = static_cast<const ScenarioModel&>(m_presenter->viewModel().sharedProcessModel()).event(state.model().eventId());

    state.view()->setPos({ev.date().msec() / m_presenter->m_zoomRatio,
                          rect.height() * state.model().heightPercentage()});

    m_presenter->m_view->update();
}

template<typename T>
void update_min_max(const T& val, T& min, T& max)
{
    min = val < min ? val : min;
    max = val > max ? val : max;
}

void ScenarioViewInterface::on_hoverOnConstraint(const id_type<ConstraintModel>& constraintId, bool enter)
{
    ISCORE_TODO
    /*
    const auto& constraint = m_presenter->m_constraints.at(constraintId)->model();
    EventPresenter* start = m_presenter->m_events.at(constraint.startEvent());
    start->view()->setShadow(enter);
    EventPresenter* end = m_presenter->m_events.at(constraint.endEvent());
    end->view()->setShadow(enter);
    */
}

void ScenarioViewInterface::on_hoverOnEvent(const id_type<EventModel>& eventId, bool enter)
{

    ISCORE_TODO
    /*
    const auto& event = m_presenter->m_events.at(eventId)->model();
    for (const auto& cstr : event.constraints())
    {
        auto cstrView = view(m_presenter->m_constraints.at(cstr));
        cstrView->setShadow(enter);
    }
    */
}
