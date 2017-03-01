#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackPresenter.hpp>
#include <Scenario/Document/Constraint/Rack/RackView.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <qnamespace.h>

#include "ConstraintHeader.hpp"
#include "ConstraintPresenter.hpp"
#include "ConstraintView.hpp"
#include "ConstraintViewModel.hpp"
#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/Braces/LeftBrace.hpp>
#include <Scenario/Document/ModelConsistency.hpp>
#include <iscore/selection/Selectable.hpp>

#include <iscore/model/EntityMap.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/tools/Todo.hpp>

class QObject;
/**
 * TODO Mettre dans la doc :
 * L'abstract constraint presenter a deux interfaces :
 *  - une qui est relative à la gestion de la vue (setScaleFactor)
 *  - une qui est là pour interagir avec le modèle
 * (on_defaul/min/maxDurationChanged)
 */
namespace Scenario
{
ConstraintPresenter::ConstraintPresenter(
    const ConstraintViewModel& model,
    ConstraintView* view,
    ConstraintHeader* header,
    const Process::ProcessPresenterContext& ctx,
    QObject* parent)
    : QObject{parent}
    , m_viewModel{model}
    , m_view{view}
    , m_header{header}
    , m_context{ctx}
{
  auto& constraint = m_viewModel.model();
  m_header->setParentItem(m_view);
  m_header->setConstraintView(m_view);
  m_header->setVisible(false);

  con(constraint.selection, &Selectable::changed, m_view,
      &ConstraintView::setSelected);

  con(constraint.duration, &ConstraintDurations::minNullChanged, this,
      [&](bool b) { updateBraces(); });
  con(constraint.duration, &ConstraintDurations::minDurationChanged, this,
      [&](const TimeVal& val) {
        on_minDurationChanged(val);
        updateChildren();
      });
  con(constraint.duration, &ConstraintDurations::defaultDurationChanged, this,
      [&](const TimeVal& val) {
        on_defaultDurationChanged(val);
        updateChildren();
      });
  con(constraint.duration, &ConstraintDurations::maxDurationChanged, this,
      [&](const TimeVal& val) {
        on_maxDurationChanged(val);
        updateChildren();
      });

  // As an optimization, this has been replaced
  // by a timer in the parent processes, scenario and loop.
  // con(constraint.duration, &ConstraintDurations::playPercentageChanged,
  //         this, &ConstraintPresenter::on_playPercentageChanged,
  //         Qt::QueuedConnection);

  con(constraint, &ConstraintModel::heightPercentageChanged, this,
      &ConstraintPresenter::heightPercentageChanged);
  con(constraint, &ConstraintModel::executionStarted,
      this, [=] { m_view->setExecuting(true); });
  con(constraint, &ConstraintModel::executionStopped,
      this, [=] { m_view->setExecuting(false); });

  con(m_viewModel, &ConstraintViewModel::rackShown, this,
      &ConstraintPresenter::on_rackShown);

  con(m_viewModel, &ConstraintViewModel::rackHidden, this,
      &ConstraintPresenter::on_rackHidden);

  con(m_viewModel, &ConstraintViewModel::lastRackRemoved, this,
      &ConstraintPresenter::on_noRacks);

  con(constraint.consistency, &ModelConsistency::validChanged, m_view,
      &ConstraintView::setValid);
  con(constraint.consistency, &ModelConsistency::warningChanged, m_view,
      &ConstraintView::setWarning);

  constraint.racks.added
      .connect<ConstraintPresenter, &ConstraintPresenter::on_racksChanged>(
          this);
  constraint.racks.removed
      .connect<ConstraintPresenter, &ConstraintPresenter::on_racksChanged>(
          this);
  on_racksChanged();
}

ConstraintPresenter::~ConstraintPresenter()
{
  delete m_rack;
}

void ConstraintPresenter::updateScaling()
{
  const auto& cm = m_viewModel.model();
  // prendre en compte la distance du clic à chaque côté

  on_defaultDurationChanged(cm.duration.defaultDuration());
  on_minDurationChanged(cm.duration.minDuration());
  on_maxDurationChanged(cm.duration.maxDuration());
  on_playPercentageChanged(cm.duration.playPercentage());

  updateChildren();
  updateHeight();
}

void ConstraintPresenter::on_zoomRatioChanged(ZoomRatio val)
{
  m_zoomRatio = val;
  updateScaling();

  if (rack())
  {
    rack()->on_zoomRatioChanged(m_zoomRatio);
  }
}

const Id<ConstraintModel>& ConstraintPresenter::id() const
{
  return model().id();
}

void ConstraintPresenter::on_defaultDurationChanged(const TimeVal& val)
{
  auto width = val.toPixels(m_zoomRatio);
  m_view->setDefaultWidth(width);
  m_view->updateLabelPos();
  m_view->updateCounterPos();
  m_view->updateOverlayPos();
  m_header->setWidth(width);

  if (rack())
  {
    rack()->setWidth(m_view->defaultWidth());
  }
  updateBraces();
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

void ConstraintPresenter::on_playPercentageChanged(double t)
{
  if (!m_view->infinite())
    m_view->setPlayWidth(m_view->maxWidth() * t);
  else
    m_view->setPlayWidth(m_view->defaultWidth() * t);
}

void ConstraintPresenter::updateHeight()
{
  if (rack() && m_viewModel.isRackShown())
  {
    m_view->setHeight(rack()->height() + ConstraintHeader::headerHeight());
  }
  else if (rack() && !m_viewModel.isRackShown())
  {
    m_view->setHeight(ConstraintHeader::headerHeight());
  }
  else
  {
    m_view->setHeight(8);
  }

  updateChildren();
  emit heightChanged();
}

void ConstraintPresenter::updateChildren()
{
  emit askUpdate();

  m_view->update();
  m_header->update();
}

bool ConstraintPresenter::isSelected() const
{
  return m_viewModel.model().selection.get();
}

RackPresenter* ConstraintPresenter::rack() const
{
  return m_rack;
}

const ConstraintModel& ConstraintPresenter::model() const
{
  return m_viewModel.model();
}

ConstraintView* ConstraintPresenter::view() const
{
  return m_view;
}

void ConstraintPresenter::on_rackShown(const OptionalId<RackModel>& rackId)
{
  clearRackPresenter();
  if (rackId)
  {
    createRackPresenter(m_viewModel.model().racks.at(*rackId));

    m_header->setState(ConstraintHeader::State::RackShown);
  }
  else
  {
    m_header->setState(ConstraintHeader::State::RackHidden);
  }
  updateHeight();
}

void ConstraintPresenter::on_rackHidden()
{
  if (!model().racks.empty())
  {
    clearRackPresenter();

    m_header->setState(ConstraintHeader::State::RackHidden);
    updateHeight();
  }
  else
  {
    on_noRacks();
  }
}

void ConstraintPresenter::on_noRacks()
{
  // m_header->hide();
  clearRackPresenter();

  m_header->setState(ConstraintHeader::State::Hidden);
  updateHeight();
}

void ConstraintPresenter::on_racksChanged(const RackModel&)
{
  on_racksChanged();
}

void ConstraintPresenter::on_racksChanged()
{
  auto& constraint = m_viewModel.model();
  if (!constraint.racks.empty())
  {
    if (m_viewModel.isRackShown())
    {
      on_rackShown(*m_viewModel.shownRack());
    }
    else if (!constraint.processes.empty()) // TODO why isn't this when there
                                            // are racks but hidden ?
    {
      on_rackHidden();
    }
    else
    {
      on_noRacks();
    }
  }
  else
  {
    on_noRacks();
  }
}

void ConstraintPresenter::clearRackPresenter()
{
  if (m_rack)
  {
    m_rack->deleteLater();
    m_rack = nullptr;
  }
}

void ConstraintPresenter::updateBraces()
{
  const auto& dur = m_viewModel.model().duration;
  auto& lb = m_view->leftBrace();
  auto& rb = m_view->rightBrace();
  const bool rigid = dur.isRigid();

  lb.setVisible(!dur.isMinNul() && !rigid);
  rb.setVisible(!dur.isMaxInfinite() && !rigid);
}

void ConstraintPresenter::createRackPresenter(const RackModel& rackModel)
{
  auto rackView = new RackView{m_view};
  rackView->setPosition(QPointF(0, ConstraintHeader::headerHeight()));

  // Cas par défaut
  m_rack = new RackPresenter{rackModel, rackView, m_context, this};

  m_rack->on_zoomRatioChanged(m_zoomRatio);

  connect(
      m_rack, &RackPresenter::askUpdate, this,
      &ConstraintPresenter::updateHeight);

  connect(
      m_rack, &RackPresenter::pressed, this, &ConstraintPresenter::pressed);
  connect(m_rack, &RackPresenter::moved, this, &ConstraintPresenter::moved);
  connect(
      m_rack, &RackPresenter::released, this, &ConstraintPresenter::released);
}
}
