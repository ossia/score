// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurveSegmentView.hpp"

#include "CurveSegmentModel.hpp"

#include <Curve/CurveStyle.hpp>
#include <Curve/Palette/CurvePoint.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <score/graphics/PainterPath.hpp>
#include <score/model/Identifier.hpp>
#include <score/selection/Selectable.hpp>

#include <ossia/detail/math.hpp>

#include <QPainter>
#include <QPen>
#include <qgraphicssceneevent.h>

#include <wobjectimpl.h>

#include <cstddef>
#include <vector>
W_OBJECT_IMPL(Curve::SegmentView)
namespace Curve
{
SegmentView::SegmentView(
    const SegmentModel* model,
    const Curve::Style& style,
    QGraphicsItem* parent)
    : QGraphicsItem{parent}, m_style{style}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setZValue(1);
  this->setFlag(ItemIsFocusable, false);

  setModel(model);
  updatePen();
}

void SegmentView::setModel(const SegmentModel* model)
{
  if (m_model)
  {
    disconnect(&m_model->selection, &Selectable::changed, this, &SegmentView::setSelected);
    disconnect(m_model, &SegmentModel::dataChanged, this, &SegmentView::updatePoints);
  }

  m_model = model;

  if (m_model)
  {
    connect(&m_model->selection, &Selectable::changed, this, &SegmentView::setSelected);
    connect(m_model, &SegmentModel::dataChanged, this, &SegmentView::updatePoints);

    setSelected(m_model->selection.get());
  }
  updatePoints();
}

const Id<SegmentModel>& SegmentView::id() const
{
  return m_model->id();
}

void SegmentView::setRect(const QRectF& theRect)
{
  prepareGeometryChange();
  m_rect = theRect;
  updatePoints();
}

QRectF SegmentView::boundingRect() const
{
  return m_rect;
}

QPainterPath SegmentView::shape() const
{
  recomputeStroke();
  return m_strokedShape;
}

QPainterPath SegmentView::opaqueArea() const
{
  recomputeStroke();
  return m_strokedShape;
}

bool SegmentView::contains(const QPointF& pt) const
{
  recomputeStroke();
  return m_strokedShape.contains(pt);
}

void SegmentView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  painter->setRenderHint(QPainter::RenderHint::Antialiasing, m_enabled && m_rect.width() > 10);
  painter->strokePath(m_unstrokedShape, *m_pen);
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
  updatePoints();
  updatePen();
  update();
}

void SegmentView::disable()
{
  m_enabled = false;
  updatePoints();
  updatePen();
  update();
}

void SegmentView::setTween(bool b)
{
  m_tween = b;
  updatePen();
  update();
}

void SegmentView::recomputeStroke() const
{
  static const QPainterPathStroker CurveSegmentStroker{[] {
    QPen p;
    p.setWidth(12);
    return p;
  }()};
  if (m_needsRecompute)
  {
    m_strokedShape = CurveSegmentStroker.createStroke(m_unstrokedShape);
    m_needsRecompute = false;
  }
}

void SegmentView::updatePoints()
{
  if (m_model)
  {
    // Get the length of the segment to scale.
    double len = m_model->end().x() - m_model->start().x();
    if(len <= 1e-16)
      len = 1e-16;
    double startx = m_model->start().x() * m_rect.width() / len;
    double scalex = m_rect.width() / len;

    if (m_enabled)
      m_model->updateData(
          ossia::clamp(m_rect.width(), 2., 75.)); // Set the number of required points here.
    else
      m_model->updateData(
          ossia::clamp(m_rect.width(), 2., 10.)); // Set the number of required points here.
    const auto& pts = m_model->data();

    const auto rect_height = m_rect.height();
    // Map to the scene coordinates
    if (!pts.empty())
    {
      auto first = pts.front();
      auto first_scaled = QPointF{first.x() * scalex - startx, (1. - first.y()) * rect_height};

      m_unstrokedShape = QPainterPath{first_scaled};
      int n = pts.size();
      for (int i = 1; i < n; i++)
      {
        auto next = pts[i];
        m_unstrokedShape.lineTo(
            QPointF{next.x() * scalex - startx, (1. - next.y()) * rect_height});
      }
    }
  }
  else
  {
    clearPainterPath(m_unstrokedShape);
  }
  m_needsRecompute = true;
  update();
}

void SegmentView::updatePen()
{
  if (m_enabled)
  {
    if (!m_tween)
    {
      if (!m_selected)
        m_pen = &m_style.PenSegment;
      else
        m_pen = &m_style.PenSegmentSelected;
    }
    else
    {
      if (!m_selected)
        m_pen = &m_style.PenSegmentTween;
      else
        m_pen = &m_style.PenSegmentTweenSelected;
    }
  }
  else
  {
    m_pen = &m_style.PenSegmentDisabled;
  }

  SCORE_ASSERT(m_pen);
}

void SegmentView::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
  contextMenuRequested(ev->screenPos(), ev->scenePos());
}
}
