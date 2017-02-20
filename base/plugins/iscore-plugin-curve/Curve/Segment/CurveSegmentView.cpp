#include <Curve/CurveStyle.hpp>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QPen>
#include <cstddef>
#include <vector>

#include "CurveSegmentModel.hpp"
#include "CurveSegmentView.hpp"
#include <Curve/Palette/CurvePoint.hpp>
#include <iscore/selection/Selectable.hpp>
#include <iscore/tools/Todo.hpp>


class QWidget;
#include <iscore/model/Identifier.hpp>
namespace Curve
{
const QPainterPathStroker CurveSegmentStroker{[]() {
  QPen p;
  p.setWidth(12);
  return p;
}()};
SegmentView::SegmentView(
    const SegmentModel* model,
    const Curve::Style& style,
    QQuickPaintedItem* parent)
    : GraphicsItem{parent}, m_style{style}
{
  //this->setCacheMode(QQuickPaintedItem::NoCache);
  this->setZ(1);
  //this->setFlag(ItemIsFocusable, false);

  setModel(model);
  updatePen();
}

void SegmentView::setModel(const SegmentModel* model)
{
  m_model = model;

  if (m_model)
  {
    con(m_model->selection, &Selectable::changed, this,
        &SegmentView::setSelected);
    connect(
        m_model, &SegmentModel::dataChanged, this, &SegmentView::updatePoints);
  }
}

const Id<SegmentModel>& SegmentView::id() const
{
  return m_model->id();
}

void SegmentView::setRect(const QRectF& theRect)
{
//  prepareGeometryChange();
  m_rect = theRect;
  updatePoints();
}

QRectF SegmentView::boundingRect() const
{
  return m_rect;
}

void SegmentView::paint(
    QPainter* painter)
{
  painter->setRenderHint(QPainter::RenderHint::Antialiasing, m_enabled);

  painter->setPen(*m_pen);
  painter->setBrush(Qt::NoBrush);
  painter->drawPath(m_unstrokedShape);

  painter->setRenderHint(QPainter::RenderHint::Antialiasing, false);
}

void SegmentView::setSelected(bool selected)
{
  m_selected = selected;
  updatePen();
  update();
}

void SegmentView::enable()
{
  m_enabled = true;
  updatePen();
  update();
}

void SegmentView::disable()
{
  m_enabled = false;
  updatePen();
  update();
}

void SegmentView::setTween(bool b)
{
  m_tween = b;
  updatePen();
  update();
}

void SegmentView::updatePoints()
{
  if (!m_model)
    return;

  // Get the length of the segment to scale.
  double len = m_model->end().x() - m_model->start().x();
  double startx = m_model->start().x() * m_rect.width() / len;
  double scalex = m_rect.width() / len;

  m_model->updateData(25); // Set the number of required points here.
  const auto& pts = m_model->data();

  const auto rect_height = m_rect.height();
  // Map to the scene coordinates
  if (!pts.empty())
  {
    auto first = pts.front();
    auto first_scaled
        = QPointF{first.x() * scalex - startx, (1. - first.y()) * rect_height};

    m_unstrokedShape = QPainterPath{first_scaled};
    int n = pts.size();
    for (int i = 1; i < n; i++)
    {
      auto next = pts[i];
      m_unstrokedShape.lineTo(
          QPointF{next.x() * scalex - startx, (1. - next.y()) * rect_height});
    }
  }

  m_strokedShape = CurveSegmentStroker.createStroke(m_unstrokedShape);

  update();
}

void SegmentView::updatePen()
{
  if(m_enabled)
  {
    if(!m_tween)
    {
      if(!m_selected)
        m_pen = &m_style.PenSegment;
      else
        m_pen = &m_style.PenSegmentSelected;
    }
    else
    {
      if(!m_selected)
        m_pen = &m_style.PenSegmentTween;
      else
        m_pen = &m_style.PenSegmentTweenSelected;
    }
  }
  else
  {
    m_pen = &m_style.PenSegmentDisabled;
  }

  ISCORE_ASSERT(m_pen);
}
/*
void SegmentView::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
  emit contextMenuRequested(ev->screenPos(), ev->scenePos());
}*/
}
