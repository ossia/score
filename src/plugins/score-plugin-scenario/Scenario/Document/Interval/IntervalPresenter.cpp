// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "IntervalPresenter.hpp"

#include "IntervalHeader.hpp"
#include "IntervalView.hpp"

#include <Process/LiveTake.hpp>
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
    ZoomRatio zoom, const IntervalModel& model, IntervalView* view,
    IntervalHeader* header, const Process::Context& ctx, QObject* parent)
    : QObject{parent}
    , m_model{model}
    , m_zoomRatio{zoom}
    , m_view{view}
    , m_header{header}
    , m_context{ctx}
{
  auto& interval = m_model;
  // Takes being recorded into the box
  {
    auto& live = Process::LiveTakes::instance();
    const auto add = [this](Process::LiveTake* t) {
      if(t->interval != &m_model)
        return;
      m_liveTakes.push_back(new Process::LiveTakeSpan{*t, m_view});
      layoutLiveTakes();
    };
    for(auto* t : live.takes())
      add(t);
    connect(&live, &Process::LiveTakes::added, this, add);
    connect(&live, &Process::LiveTakes::removed, this, [this](Process::LiveTake* t) {
      for(auto it = m_liveTakes.begin(); it != m_liveTakes.end();)
      {
        if(!*it || (*it)->take() == t)
        {
          delete it->data();
          it = m_liveTakes.erase(it);
        }
        else
          ++it;
      }
      layoutLiveTakes();
    });
    connect(this, &IntervalPresenter::heightChanged, this, [this] { layoutLiveTakes(); });
  }
  m_header->setParentItem(m_view);
  m_header->setIntervalView(m_view);
  // m_header->hide();
  // m_header->setPos(0, -m_header->headerHeight());

  con(interval.duration, &IntervalDurations::minNullChanged, this,
      [&](bool b) { updateBraces(); });
  con(interval.duration, &IntervalDurations::minDurationChanged, this,
      [&](const TimeVal& val) {
    on_minDurationChanged(val);
    updateChildren();
  });
  con(interval.duration, &IntervalDurations::maxDurationChanged, this,
      [&](const TimeVal& val) {
    on_maxDurationChanged(val);
    updateChildren();
  });

  con(interval, &IntervalModel::heightPercentageChanged, this,
      &IntervalPresenter::heightPercentageChanged);

  con(interval.consistency, &ModelConsistency::validChanged, m_view,
      &IntervalView::setValid);
  con(interval.consistency, &ModelConsistency::warningChanged, m_view,
      &IntervalView::setWarning);
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
  layoutLiveTakes();
}

double IntervalPresenter::rackBottom() const
{
  return m_view->height();
}

void IntervalPresenter::layoutLiveTakes()
{
  constexpr double band = 56.;
  const double bottom = rackBottom();
  int k = 0;
  for(auto& span : m_liveTakes)
  {
    if(!span)
      continue;
    auto* t = span->take();
    if(t && t->scenarioY)
    {
      // In a new box of the scenario the box shows
      const double rack = std::max(band, bottom - 2.);
      span->setGeometry(m_zoomRatio, 2. + *t->scenarioY * rack, band);
    }
    else
    {
      span->setGeometry(m_zoomRatio, bottom + 2. + k * (band + 2.), band);
      k++;
    }
  }
}

const Id<IntervalModel>& IntervalPresenter::id() const
{
  return model().id();
}

void IntervalPresenter::on_minDurationChanged(const TimeVal& min)
{
  auto x = min.toPixelsRaw(m_zoomRatio);
  m_view->setMinWidth(x);
  m_view->leftBrace().setX(x);
  updateBraces();
}

void IntervalPresenter::on_maxDurationChanged(const TimeVal& max)
{
  auto x = max.toPixelsRaw(m_zoomRatio);
  m_view->setMaxWidth(
      m_model.duration.isMaxInfinite(), m_model.duration.isMaxInfinite() ? -1 : x);
  m_view->rightBrace().setX(x + 2);
  updateBraces();
}

double IntervalPresenter::on_playPercentageChanged(double t)
{
  if(!m_view->infinite())
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
