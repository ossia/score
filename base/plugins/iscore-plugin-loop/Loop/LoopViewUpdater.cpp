#include <Loop/LoopLayer.hpp>
#include <Loop/LoopPresenter.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <Loop/LoopView.hpp>
#include <QPoint>
#include <QRect>
#include <QtGlobal>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp>
#include <Scenario/Document/Event/EventView.hpp>
#include <Scenario/Document/TimeNode/TimeNodeView.hpp>

#include "LoopViewUpdater.hpp"
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintPresenter.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/State/StateView.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodePresenter.hpp>
#include <Scenario/Document/VerticalExtent.hpp>

namespace Loop
{
ViewUpdater::ViewUpdater(LayerPresenter& presenter) : m_presenter{presenter}
{
}

void ViewUpdater::updateEvent(const Scenario::EventPresenter& event)
{
  event.view()->setExtent(extent());

  event.view()->setPosition({event.model().date().toPixels(m_presenter.m_zoomRatio),
                        extent().top()});

  // We also have to move all the relevant states
  if (&event == m_presenter.m_startEventPresenter)
  {
    updateState(*m_presenter.m_startStatePresenter);
  }
  else if (&event == m_presenter.m_endEventPresenter)
  {
    updateState(*m_presenter.m_endStatePresenter);
  }

  m_presenter.m_view->update();
}

void ViewUpdater::updateConstraint(
    const Scenario::TemporalConstraintPresenter& pres)
{
  auto msPerPixel = m_presenter.m_zoomRatio;

  const auto& cstr_model = pres.model();
  Scenario::TemporalConstraintView& cstr_view = Scenario::view(pres);

  auto startPos = cstr_model.startDate().toPixels(msPerPixel);
  auto delta = cstr_view.x() - startPos;
  bool dateChanged = (delta * delta > 1); // Magnetism

  if (dateChanged)
  {
    cstr_view.setPosition({startPos, extent().top()});
  }
  else
  {
    cstr_view.setY(extent().top());
  }

  cstr_view.setDefaultWidth(
      cstr_model.duration.defaultDuration().toPixels(msPerPixel));
  cstr_view.setMinWidth(
      cstr_model.duration.minDuration().toPixels(msPerPixel));
  cstr_view.setMaxWidth(
      cstr_model.duration.maxDuration().isInfinite(),
      cstr_model.duration.maxDuration().isInfinite()
          ? -1
          : cstr_model.duration.maxDuration().toPixels(msPerPixel));

  m_presenter.m_view->update();
}

void ViewUpdater::updateTimeNode(const Scenario::TimeNodePresenter& timenode)
{
  timenode.view()->setExtent(2. * extent());

  timenode.view()->setPosition(
      {timenode.model().date().toPixels(m_presenter.m_zoomRatio),
       extent().top()});

  m_presenter.m_view->update();
}

void ViewUpdater::updateState(const Scenario::StatePresenter& state)
{
  if (&state == m_presenter.m_startStatePresenter)
  {
    const auto& ev = m_presenter.m_layer.model().startEvent();
    state.view()->setPosition(
        {ev.date().toPixels(m_presenter.m_zoomRatio), extent().top()});
  }
  else if (&state == m_presenter.m_endStatePresenter)
  {
    const auto& ev = m_presenter.m_layer.model().endEvent();
    state.view()->setPosition(
        {ev.date().toPixels(m_presenter.m_zoomRatio), extent().top()});
  }

  m_presenter.m_view->update();
}
}
