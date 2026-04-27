// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SequenceView.hpp"

#include <Process/Style/ScenarioStyle.hpp>

#include <score/model/Skin.hpp>

#include <QGraphicsLineItem>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Sequence::SequenceView)

namespace Sequence
{

static constexpr double k_handleHalfWidth = 5.0;
static constexpr double k_handleZValue = 10.0; // on top of child section views

SequenceView::SequenceView(QGraphicsItem* parent)
    : Process::LayerView{parent}
{
  setAcceptedMouseButtons(Qt::LeftButton);
}

SequenceView::~SequenceView() = default;

void SequenceView::setHandles(const QVector<HandleData>& handles)
{
  m_handles = handles;

  // Grow or shrink the handle-line pool to match
  while(m_handleLines.size() > handles.size())
  {
    delete m_handleLines.takeLast();
  }
  while(m_handleLines.size() < handles.size())
  {
    auto* line = new QGraphicsLineItem{this};
    line->setZValue(k_handleZValue);
    m_handleLines.append(line);
  }

  updateHandleLines();
  update();
}

void SequenceView::updateHandleLines()
{
  const qreal h = height();
  auto& style = Process::Style::instance();

  for(int i = 0; i < m_handles.size(); ++i)
  {
    const double x = m_handles[i].x;
    const bool active = (i == m_activeHandle);
    m_handleLines[i]->setLine(x, 0., x, h);
    m_handleLines[i]->setPen(
        active ? style.skin.Base3.main.pen2 : style.skin.Base2.main.pen1);
  }
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

  // IS handle lines are rendered by QGraphicsLineItem children — no manual
  // drawing needed here. The items have k_handleZValue so they float above
  // any child section views that SequencePresenter adds at lower z-values.
}

void SequenceView::mousePressEvent(QGraphicsSceneMouseEvent* e)
{
  const double x = e->pos().x();
  m_activeHandle = handleAt(x);
  if(m_activeHandle >= 0)
  {
    m_dragStartX = m_handles[m_activeHandle].x;
    updateHandleLines();
    e->accept();
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
  updateHandleLines();
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
    updateHandleLines();
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
    updateHandleLines();
    handleDragCancelled();
    e->accept();
  }
  else
  {
    e->ignore();
  }
}

} // namespace Sequence
