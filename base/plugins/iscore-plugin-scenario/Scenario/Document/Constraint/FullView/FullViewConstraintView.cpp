// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Style/ScenarioStyle.hpp>
#include <QColor>
#include <QCursor>
#include <QGraphicsItem>
#include <QPainter>
#include <QPen>
#include <QtGlobal>
#include <qnamespace.h>

#include "FullViewConstraintPresenter.hpp"
#include "FullViewConstraintView.hpp"
#include <Scenario/Document/Constraint/ConstraintView.hpp>
#include <QGraphicsScene>
#include <QGraphicsView>
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
FullViewConstraintView::FullViewConstraintView(
    FullViewConstraintPresenter& presenter, QGraphicsItem* parent)
    : ConstraintView{presenter, parent}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setParentItem(parent);
  this->setFlag(ItemIsSelectable);

  this->setZValue(1);
}

void FullViewConstraintView::updatePaths()
{
}
void FullViewConstraintView::updatePlayPaths()
{
}

void FullViewConstraintView::updateOverlayPos()
{
}

void FullViewConstraintView::setSelected(bool selected)
{
  m_selected = selected;
  setZValue(m_selected ? ZPos::SelectedConstraint : ZPos::Constraint);
  update();
}
QRectF FullViewConstraintView::boundingRect() const
{
  return {0, 0, qreal(std::max(defaultWidth(), m_guiWidth)) + 3, qreal(constraintAndRackHeight()) + 3};
}

void FullViewConstraintView::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& skin = ScenarioStyle::instance();
  const qreal min_w = minWidth();
  const qreal max_w = maxWidth();
  const qreal def_w = defaultWidth();
  const qreal play_w = playWidth();

  auto& p = *painter;

  p.setRenderHint(QPainter::Antialiasing, false);

  QBrush c;
  if (isSelected())
  {
    c = skin.ConstraintSelected.getBrush();
  }
  else if (parentItem()->isSelected())
  {
    c = skin.ConstraintFullViewParentSelected.getBrush();
  }
  else
  {
    c = skin.ConstraintBase.getBrush();
  }

  skin.ConstraintSolidPen.setBrush(c);

  if (min_w == max_w)
  {
    p.setPen(skin.ConstraintSolidPen);
    p.drawLine(QPointF{0., 0.}, QPointF{def_w, 0.});
  }
  else
  {
    // Then the dashed line
    skin.ConstraintDashPen.setBrush(c);
    p.setPen(skin.ConstraintDashPen);

    p.fillRect(QRectF{def_w, (double)ConstraintHeaderHeight, m_guiWidth, this->height() - ConstraintHeaderHeight}, skin.SlotOverlay.getBrush());

    p.drawLine(QPointF{min_w, 0.}, QPointF{infinite()? m_guiWidth : max_w, 0.});


    painter->setPen(skin.FullViewConstraintHeaderSeparator);
    p.drawLine(QPointF{def_w, (double)ConstraintHeaderHeight}, QPointF{m_guiWidth, (double)ConstraintHeaderHeight});

    // First the line going from 0 to the min
    p.setPen(skin.ConstraintSolidPen);
    p.drawLine(QPointF{0., 0.}, QPointF{min_w, 0.});

  }

  auto pw = playWidth();
  if (pw != 0.)
  {
    skin.ConstraintPlayPen.setBrush(skin.ConstraintPlayFill.getBrush());
    p.setPen(skin.ConstraintPlayPen);
    p.drawLine(QPointF{0., 0.}, QPointF{std::min(play_w, std::max(def_w, max_w)), 0.});
  }

#if defined(ISCORE_SCENARIO_DEBUG_RECTS)
  p.setPen(Qt::red);
  p.drawRect(boundingRect());
#endif
}

void FullViewConstraintView::setGuiWidth(double w)
{
  m_guiWidth = w;
  update();
}
}
