#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <qnamespace.h>

#include "ConstraintHeader.hpp"
#include "ConstraintPresenter.hpp"
#include "ConstraintView.hpp"
#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Constraint/Temporal/Braces/LeftBrace.hpp>
#include <Scenario/Document/ModelConsistency.hpp>
#include <iscore/selection/Selectable.hpp>

#include <iscore/model/EntityMap.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/tools/Todo.hpp>

class QObject;
namespace Scenario
{
ConstraintPresenter::ConstraintPresenter(
    const ConstraintModel& model,
    ConstraintView* view,
    ConstraintHeader* header,
    const Process::ProcessPresenterContext& ctx,
    QObject* parent)
    : QObject{parent}
    , m_model{model}
    , m_view{view}
    , m_header{header}
    , m_context{ctx}
{
  auto& constraint = m_model;
  m_header->setParentItem(m_view);
  m_header->setConstraintView(m_view);
  m_header->hide();

  con(constraint.selection, &Selectable::changed, m_view,
      &ConstraintView::setSelected);

  con(constraint.duration, &ConstraintDurations::minNullChanged, this,
      [&](bool b) { updateBraces(); });
  con(constraint.duration, &ConstraintDurations::minDurationChanged, this,
      [&](const TimeVal& val) {
        on_minDurationChanged(val);
        updateChildren();
      });
  con(constraint.duration, &ConstraintDurations::maxDurationChanged, this,
      [&](const TimeVal& val) {
        on_maxDurationChanged(val);
        updateChildren();
      });

  con(constraint, &ConstraintModel::heightPercentageChanged, this,
      &ConstraintPresenter::heightPercentageChanged);
  con(constraint, &ConstraintModel::executionStarted,
      this, [=] { m_view->setExecuting(true); });
  con(constraint, &ConstraintModel::executionStopped,
      this, [=] { m_view->setExecuting(false); });

  con(constraint.consistency, &ModelConsistency::validChanged, m_view,
      &ConstraintView::setValid);
  con(constraint.consistency, &ModelConsistency::warningChanged, m_view,
      &ConstraintView::setWarning);
}

ConstraintPresenter::~ConstraintPresenter()
{
}

void ConstraintPresenter::updateScaling()
{
  const auto& cm = m_model;

  on_minDurationChanged(cm.duration.minDuration());
  on_maxDurationChanged(cm.duration.maxDuration());
  on_playPercentageChanged(cm.duration.playPercentage());

  updateChildren();
}

void ConstraintPresenter::on_zoomRatioChanged(ZoomRatio val)
{
  m_zoomRatio = val;
  updateScaling();
}

const Id<ConstraintModel>& ConstraintPresenter::id() const
{
  return model().id();
}

void ConstraintPresenter::on_minDurationChanged(const TimeVal& min)
{
  auto x = min.toPixels(m_zoomRatio);
  m_view->setMinWidth(x);
  m_view->leftBrace().setX(x);
  updateBraces();
}

void ConstraintPresenter::on_maxDurationChanged(const TimeVal& max)
{
  auto x = max.toPixels(m_zoomRatio);
  m_view->setMaxWidth(max.isInfinite(), max.isInfinite() ? -1 : x);
  m_view->rightBrace().setX(x + 2);
  updateBraces();
}

bool ConstraintPresenter::on_playPercentageChanged(double t)
{
  if (!m_view->infinite())
    return m_view->setPlayWidth(m_view->maxWidth() * t);
  else
    return m_view->setPlayWidth(m_view->defaultWidth() * t);
}

void ConstraintPresenter::updateChildren()
{
  emit askUpdate();

  m_view->update();
  m_header->update();
}

bool ConstraintPresenter::isSelected() const
{
  return m_model.selection.get();
}

const ConstraintModel& ConstraintPresenter::model() const
{
  return m_model;
}

ConstraintView* ConstraintPresenter::view() const
{
  return m_view;
}

void ConstraintPresenter::updateBraces()
{
  const auto& dur = m_model.duration;
  auto& lb = m_view->leftBrace();
  auto& rb = m_view->rightBrace();
  const bool rigid = dur.isRigid();

  lb.setVisible(!dur.isMinNull() && !rigid);
  rb.setVisible(!dur.isMaxInfinite() && !rigid);
}

}
