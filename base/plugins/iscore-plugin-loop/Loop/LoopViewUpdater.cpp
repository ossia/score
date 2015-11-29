#include <Loop/LoopLayer.hpp>
#include <Loop/LoopPresenter.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <Loop/LoopView.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp>
#include <Scenario/Document/Event/EventView.hpp>
#include <Scenario/Document/TimeNode/TimeNodeView.hpp>
#include <QtGlobal>
#include <QPoint>
#include <QRect>

#include "LoopViewUpdater.hpp"
#include <Process/TimeValue.hpp>
#include "Scenario/Document/Constraint/ConstraintDurations.hpp"
#include "Scenario/Document/Constraint/ConstraintModel.hpp"
#include "Scenario/Document/Constraint/ViewModels/ConstraintPresenter.hpp"
#include "Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"
#include "Scenario/Document/Event/EventModel.hpp"
#include "Scenario/Document/Event/EventPresenter.hpp"
#include "Scenario/Document/State/StateModel.hpp"
#include "Scenario/Document/State/StatePresenter.hpp"
#include "Scenario/Document/State/StateView.hpp"
#include "Scenario/Document/TimeNode/TimeNodeModel.hpp"
#include "Scenario/Document/TimeNode/TimeNodePresenter.hpp"
#include "Scenario/Document/VerticalExtent.hpp"

LoopViewUpdater::LoopViewUpdater(LoopPresenter& presenter):
    m_presenter{presenter}
{

}

void LoopViewUpdater::updateEvent(const EventPresenter& event)
{
    auto h = m_presenter.m_view->boundingRect().height();

    event.view()->setExtent(event.model().extent() * h);

    event.view()->setPos({event.model().date().msec() / m_presenter.m_zoomRatio,
                          event.model().extent().top() * h});

    // We also have to move all the relevant states
    if(&event == m_presenter.m_startEventPresenter)
    {
        updateState(*m_presenter.m_startStatePresenter);
    }
    else if(&event == m_presenter.m_endEventPresenter)
    {
        updateState(*m_presenter.m_endStatePresenter);
    }

    m_presenter.m_view->update();

}

void LoopViewUpdater::updateConstraint(const TemporalConstraintPresenter& pres)
{
    auto rect = m_presenter.m_view->boundingRect();
    auto msPerPixel = m_presenter.m_zoomRatio;

    const auto& cstr_model = pres.model();
    TemporalConstraintView& cstr_view = ::view(pres);

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

    cstr_view.setDefaultWidth(cstr_model.duration.defaultDuration().toPixels(msPerPixel));
    cstr_view.setMinWidth(cstr_model.duration.minDuration().toPixels(msPerPixel));
    cstr_view.setMaxWidth(cstr_model.duration.maxDuration().isInfinite(),
                          cstr_model.duration.maxDuration().isInfinite()? -1 :
                                                                          cstr_model.duration.maxDuration().toPixels(msPerPixel));

    m_presenter.m_view->update();
}

void LoopViewUpdater::updateTimeNode(const TimeNodePresenter& timenode)
{
    auto h = m_presenter.m_view->boundingRect().height();
    timenode.view()->setExtent(timenode.model().extent() * h);

    timenode.view()->setPos({timenode.model().date().msec() / m_presenter.m_zoomRatio,
                             timenode.model().extent().top() * h});

    m_presenter.m_view->update();
}

void LoopViewUpdater::updateState(const StatePresenter& state)
{
    auto rect = m_presenter.m_view->boundingRect();

    if(&state == m_presenter.m_startStatePresenter)
    {
        const auto& ev = m_presenter.m_layer.model().startEvent();
        state.view()->setPos({ev.date().msec() / m_presenter.m_zoomRatio,
                              rect.height() * state.model().heightPercentage()});
    }
    else if(&state == m_presenter.m_endStatePresenter)
    {
        const auto& ev = m_presenter.m_layer.model().endEvent();
        state.view()->setPos({ev.date().msec() / m_presenter.m_zoomRatio,
                              rect.height() * state.model().heightPercentage()});
    }

    m_presenter.m_view->update();
}
