#include "ProcessGraphicsView.hpp"
#include <Process/Style/ScenarioStyle.hpp>
#include <QEvent>
#include <QFlags>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QWheelEvent>
#include <qnamespace.h>

ProcessGraphicsView::ProcessGraphicsView(
    QGraphicsScene* scene, QWidget* parent)
    : QGraphicsView{scene, parent}
{
  setAlignment(Qt::AlignTop | Qt::AlignLeft);
  setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
  setRenderHints(
      QPainter::Antialiasing | QPainter::SmoothPixmapTransform
      | QPainter::TextAntialiasing);

  setFrameStyle(0);
  setCacheMode(QGraphicsView::CacheBackground);

  setOptimizationFlag(QGraphicsView::DontSavePainterState, true);
#if !defined(ISCORE_OPENGL)
// setAttribute(Qt::WA_PaintOnScreen, true);
// setAttribute(Qt::WA_OpaquePaintEvent, true);
#endif

#if defined(__APPLE__)
  setRenderHints(0);
  setOptimizationFlag(QGraphicsView::IndirectPainting, true);
#endif

  this->setBackgroundBrush(ScenarioStyle::instance().Background.getColor());
}

void ProcessGraphicsView::scrollHorizontal(double dx)
{
  if (auto bar = horizontalScrollBar())
  {
    bar->setValue(bar->value() + dx);
  }
}

void ProcessGraphicsView::resizeEvent(QResizeEvent* ev)
{
  QGraphicsView::resizeEvent(ev);
  emit sizeChanged(size());
}

void ProcessGraphicsView::scrollContentsBy(int dx, int dy)
{
  QGraphicsView::scrollContentsBy(dx, dy);

  this->scene()->update();
  if(dx != 0)
    emit scrolled(dx);
}

void ProcessGraphicsView::wheelEvent(QWheelEvent* event)
{
  QPoint d = event->angleDelta();
  QPointF delta = {d.x() / 8., d.y() / 8.};
  if (m_hZoom)
  {
    emit horizontalZoom(delta, mapToScene(event->pos()));
    return;
  }
  else if(m_vZoom)
  {
    emit verticalZoom(delta, mapToScene(event->pos()));
    return;
  }

  QGraphicsView::wheelEvent(event);
}

void ProcessGraphicsView::keyPressEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Control)
    m_hZoom = true;
  else if(event->key() == Qt::Key_Shift)
    m_vZoom = true;

  event->ignore();

  QGraphicsView::keyPressEvent(event);
}

void ProcessGraphicsView::keyReleaseEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Control)
    m_hZoom = false;
  else if(event->key() == Qt::Key_Shift)
    m_vZoom = false;

  event->ignore();

  QGraphicsView::keyReleaseEvent(event);
}

void ProcessGraphicsView::focusOutEvent(QFocusEvent* event)
{
  m_hZoom = false;
  m_vZoom = false;
  emit focusedOut();
  event->ignore();

  QGraphicsView::focusOutEvent(event);
}

void ProcessGraphicsView::leaveEvent(QEvent* event)
{
  m_hZoom = false;
  m_vZoom = false;
  emit focusedOut();
  QGraphicsView::leaveEvent(event);
}
