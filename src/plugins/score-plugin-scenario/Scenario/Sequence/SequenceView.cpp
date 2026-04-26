// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SequenceView.hpp"

#include <Process/Style/ScenarioStyle.hpp>

#include <score/model/Skin.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Sequence::SequenceView)

namespace Sequence
{

static constexpr double k_handleHalfWidth = 5.0;

SequenceView::SequenceView(QGraphicsItem* parent)
    : Process::LayerView{parent}
{
  setAcceptedMouseButtons(Qt::LeftButton);
}

SequenceView::~SequenceView() = default;

void SequenceView::setHandles(const QVector<HandleData>& handles)
{
  m_handles = handles;
  update();
}

int SequenceView::handleAt(double x) const
{
  for(int i = 0; i < m_handles.size(); ++i)
  {
    if(std::abs(m_handles[i].x - x) <= k_handleHalfWidth)
      return i;
  }
  return -1;
}

void SequenceView::paint_impl(QPainter* p) const
{
  auto& style = Process::Style::instance();

  // Background
  p->setPen(style.NoPen());
  p->setBrush(style.skin.Background2.main.brush);
  p->drawRect(boundingRect());

  // IS handle lines
  const qreal h = height();
  p->setPen(style.skin.Base2.main.pen1);
  for(const auto& handle : m_handles)
  {
    p->drawLine(QPointF(handle.x, 0.), QPointF(handle.x, h));
  }

  // Hover/drag highlight
  if(m_activeHandle >= 0 && m_activeHandle < m_handles.size())
  {
    p->setPen(style.skin.Base3.main.pen2);
    const double x = m_handles[m_activeHandle].x;
    p->drawLine(QPointF(x, 0.), QPointF(x, h));
  }
}

void SequenceView::mousePressEvent(QGraphicsSceneMouseEvent* e)
{
  const double x = e->pos().x();
  m_activeHandle = handleAt(x);
  if(m_activeHandle >= 0)
  {
    m_dragStartX = m_handles[m_activeHandle].x;
    e->accept();
    update();
  }
  else
  {
    e->ignore();
  }
}

void SequenceView::mouseMoveEvent(QGraphicsSceneMouseEvent* e)
{
  if(m_activeHandle < 0 || m_activeHandle >= m_handles.size())
    return;

  const double newX = qBound(0.0, e->pos().x(), width());
  m_handles[m_activeHandle].x = newX;
  update();
  handleDragMoved(m_handles[m_activeHandle].tsId, newX);
  e->accept();
}

void SequenceView::mouseReleaseEvent(QGraphicsSceneMouseEvent* e)
{
  if(m_activeHandle >= 0 && m_activeHandle < m_handles.size())
  {
    const double finalX = qBound(0.0, e->pos().x(), width());
    handleDragReleased(m_handles[m_activeHandle].tsId, finalX);
    m_activeHandle = -1;
    update();
    e->accept();
  }
  else
  {
    e->ignore();
  }
}

void SequenceView::keyPressEvent(QKeyEvent* e)
{
  if(e->key() == Qt::Key_Escape && m_activeHandle >= 0)
  {
    m_activeHandle = -1;
    update();
    handleDragCancelled();
    e->accept();
  }
  else
  {
    e->ignore();
  }
}

} // namespace Sequence
