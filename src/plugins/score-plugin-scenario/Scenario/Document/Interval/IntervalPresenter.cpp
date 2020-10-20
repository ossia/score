// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "IntervalPresenter.hpp"

#include "IntervalHeader.hpp"
#include "IntervalView.hpp"

#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/LayerData.hpp>
#include <Scenario/Document/Interval/Temporal/Braces/LeftBrace.hpp>
#include <Scenario/Document/ModelConsistency.hpp>

#include <score/model/EntityMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/selection/Selectable.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/std/Optional.hpp>

#include <QDebug>
#include <qnamespace.h>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::IntervalPresenter)
namespace Scenario
{
IntervalPresenter::IntervalPresenter(
    ZoomRatio zoom,
    const IntervalModel& model,
    IntervalView* view,
    IntervalHeader* header,
    const Process::Context& ctx,
    QObject* parent)
  : QObject{parent}
  , m_model{model}
  , m_zoomRatio{zoom}
  , m_view{view}
  , m_header{header}
  , m_context{ctx}
{
  auto& interval = m_model;
  m_header->setParentItem(m_view);
  m_header->setIntervalView(m_view);
  // m_header->hide();
  // m_header->setPos(0, -m_header->headerHeight());

  con(interval.duration, &IntervalDurations::minNullChanged, this, [&](bool b) {
    updateBraces();
  });
  con(interval.duration, &IntervalDurations::minDurationChanged, this, [&](const TimeVal& val) {
    on_minDurationChanged(val);
    updateChildren();
  });
  con(interval.duration, &IntervalDurations::maxDurationChanged, this, [&](const TimeVal& val) {
    on_maxDurationChanged(val);
    updateChildren();
  });

  con(interval,
      &IntervalModel::heightPercentageChanged,
      this,
      &IntervalPresenter::heightPercentageChanged);

  con(interval.consistency, &ModelConsistency::validChanged, m_view, &IntervalView::setValid);
  con(interval.consistency, &ModelConsistency::warningChanged, m_view, &IntervalView::setWarning);
}

IntervalPresenter::~IntervalPresenter() { }

void IntervalPresenter::updateScaling()
{
  const auto& cm = m_model;

  on_minDurationChanged(cm.duration.minDuration());
  on_maxDurationChanged(cm.duration.maxDuration());
  on_playPercentageChanged(cm.duration.playPercentage());

  updateChildren();
}

void IntervalPresenter::on_zoomRatioChanged(ZoomRatio val)
{
  m_zoomRatio = val;
  updateScaling();
}

const Id<IntervalModel>& IntervalPresenter::id() const
{
  return model().id();
}

void IntervalPresenter::on_minDurationChanged(const TimeVal& min)
{
  auto x = min.toPixels(m_zoomRatio);
  m_view->setMinWidth(x);
  m_view->leftBrace().setX(x);
  updateBraces();
}

void IntervalPresenter::on_maxDurationChanged(const TimeVal& max)
{
  auto x = max.toPixels(m_zoomRatio);
  m_view->setMaxWidth(m_model.duration.isMaxInfinite(), m_model.duration.isMaxInfinite() ? -1 : x);
  m_view->rightBrace().setX(x + 2);
  updateBraces();
}

double IntervalPresenter::on_playPercentageChanged(double t)
{
  if (!m_view->infinite())
    return m_view->setPlayWidth(m_view->maxWidth() * t);
  else
    return m_view->setPlayWidth(m_view->defaultWidth() * t);
}

void IntervalPresenter::updateAllSlots() const
{
  for(auto& proc : this->m_slots)
  {
    if(auto p = proc.getLayerSlot())
    {
      for(auto& lay : p->layers)
      {
        lay.parentGeometryChanged();
      }
    }
  }
}

void IntervalPresenter::updateChildren()
{
  askUpdate();

  m_view->update();
  m_header->update();
}

bool IntervalPresenter::isSelected() const
{
  return m_model.selection.get();
}

const IntervalModel& IntervalPresenter::model() const
{
  return m_model;
}

IntervalView* IntervalPresenter::view() const
{
  return m_view;
}

void IntervalPresenter::updateBraces()
{
  const auto& dur = m_model.duration;
  auto& lb = m_view->leftBrace();
  auto& rb = m_view->rightBrace();
  const bool rigid = dur.isRigid();

  lb.setVisible(!dur.isMinNull() && !rigid);
  rb.setVisible(!dur.isMaxInfinite() && !rigid);
}
}
