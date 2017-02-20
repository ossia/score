#include <Process/Style/ProcessFonts.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <QBrush>
#include <QFont>
#include <QQuickPaintedItem>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QCursor>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <qnamespace.h>

#include <Scenario/Document/Constraint/ViewModels/ConstraintMenuOverlay.hpp>
#include "TemporalConstraintPresenter.hpp"
#include "TemporalConstraintView.hpp"
#include <Scenario/Document/Constraint/ViewModels/ConstraintPresenter.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintView.hpp>
#include <iscore/model/Skin.hpp>

class QHoverEvent;

class QWidget;

namespace Scenario
{
TemporalConstraintView::TemporalConstraintView(
    TemporalConstraintPresenter& presenter, QQuickPaintedItem* parent)
  : ConstraintView{presenter, parent}
  , m_bgColor{ScenarioStyle::instance().ConstraintDefaultBackground}
{
  //this->setCacheMode(QQuickPaintedItem::NoCache);
  this->setParentItem(parent);
  //this->setAcceptDrops(true);
  this->setCursor(QCursor(Qt::SizeVerCursor));

  this->setZ(ZPos::Constraint);
}

void TemporalConstraintView::paint(
    QPainter* p)
{
  QPainterPath solidPath, dashedPath, playedPath;
  auto& painter = *p;
  painter.setRenderHint(QPainter::Antialiasing, false);
  auto& skin = ScenarioStyle::instance();

  const qreal min_w = minWidth();
  const qreal max_w = maxWidth();
  const qreal def_w = defaultWidth();
  const qreal play_w = playWidth();

  // Draw the stuff present if there is a rack *in the model* ?
  if (presenter().rack())
  {
    // Background
    auto rect = boundingRect();
    rect.adjust(0, 4, 0, -10);
    rect.setWidth(def_w);

    QColor bgColor = m_bgColor.getColor();
    bgColor.setAlpha(m_hasFocus ? 84 : 76);
    painter.fillRect(rect, bgColor);

    // Fake timenode continuation
    skin.ConstraintRackPen.setColor(skin.RackSideBorder.getColor());
    painter.setPen(skin.ConstraintRackPen);
    painter.drawLine(rect.topLeft(), rect.bottomLeft());
    painter.drawLine(rect.topRight(), rect.bottomRight());
  }

  // Paths
  if (infinite())
  {
    if (min_w != 0.)
    {
      solidPath.lineTo(min_w, 0);
    }

    // TODO end state should be hidden
    dashedPath.moveTo(min_w, 0);
    dashedPath.lineTo(def_w, 0);
  }
  else if (min_w == max_w) // TODO rigid()
  {
    solidPath.lineTo(def_w, 0);
  }
  else
  {
    if (min_w != 0.)
    {
      solidPath.lineTo(min_w, 0);
    }

    dashedPath.moveTo(min_w, 0);
    dashedPath.lineTo(max_w, 0);
  }

  if (play_w != 0.)
  {
    playedPath.lineTo(std::min(play_w, std::max(def_w, max_w)), 0);
  }

  // Colors
  auto defaultColor = this->constraintColor(skin);

  skin.ConstraintSolidPen.setColor(defaultColor);
  skin.ConstraintDashPen.setColor(defaultColor);

  // Drawing
  if (!solidPath.isEmpty())
  {
    painter.setPen(skin.ConstraintSolidPen);
    painter.drawPath(solidPath);
  }

  if (!dashedPath.isEmpty())
  {
    painter.setPen(skin.ConstraintDashPen);
    painter.drawPath(dashedPath);
  }

  if (!playedPath.isEmpty())
  {
    skin.ConstraintPlayPen.setColor(skin.ConstraintPlayFill.getColor());
    painter.setPen(skin.ConstraintPlayPen);
    painter.drawPath(playedPath);
  }

#if defined(ISCORE_SCENARIO_DEBUG_RECTS)
  painter.setPen(Qt::darkRed);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(boundingRect());
#endif
}

void TemporalConstraintView::hoverEnterEvent(QHoverEvent* h)
{
  QQuickPaintedItem::hoverEnterEvent(h);
  setShadow(true);
  emit constraintHoverEnter();
}

void TemporalConstraintView::hoverLeaveEvent(QHoverEvent* h)
{
  QQuickPaintedItem::hoverLeaveEvent(h);
  setShadow(false);
  emit constraintHoverLeave();
}

void TemporalConstraintView::dragEnterEvent(QDragEnterEvent* event)
{
  QQuickPaintedItem::dragEnterEvent(event);
  setShadow(true);
  event->accept();
}

void TemporalConstraintView::dragLeaveEvent(QDragLeaveEvent* event)
{
  QQuickPaintedItem::dragLeaveEvent(event);
  setShadow(false);
  event->accept();
}

void TemporalConstraintView::dropEvent(QDropEvent* event)
{
  emit dropReceived(event->pos(), event->mimeData());

  event->accept();
}

void TemporalConstraintView::setLabel(const QString& label)
{
  m_labelItem->setText(label);
  updateLabelPos();
}

void TemporalConstraintView::enableOverlay(bool b)
{
  if(b)
  {
    m_overlay = new ConstraintMenuOverlay{this};
    updateOverlayPos();
  }
  else
  {
    delete m_overlay;
    m_overlay = nullptr;
  }
}

void TemporalConstraintView::setExecutionDuration(const TimeVal& progress)
{
  // FIXME this should be merged with the slot in ConstraintPresenter!!!
  // Also make a setting to disable it since it may take a lot of time
  if (!qFuzzyCompare(progress.msec(), 0))
  {
    if (!m_counterItem->isVisible())
      m_counterItem->setVisible(true);
    updateCounterPos();

    m_counterItem->setText(progress.toString());
  }
  else
  {
    m_counterItem->setVisible(false);
  }
  update();
}

void TemporalConstraintView::setLabelColor(iscore::ColorRef labelColor)
{
  m_labelItem->setColor(labelColor);
  update();
}

}
