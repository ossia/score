// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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
#include <iostream>

ProcessGraphicsView::ProcessGraphicsView(
    QGraphicsScene* scene, QWidget* parent)
    : QGraphicsView{scene, parent}
{
  m_lastwheel = std::chrono::steady_clock::now();
  setAlignment(Qt::AlignTop | Qt::AlignLeft);
  setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
  setRenderHints(
      QPainter::Antialiasing | QPainter::SmoothPixmapTransform
      | QPainter::TextAntialiasing);

  setFrameStyle(0);
  //setCacheMode(QGraphicsView::CacheBackground);
  setDragMode(QGraphicsView::NoDrag);

  setOptimizationFlag(QGraphicsView::DontSavePainterState, true);
  setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
#if !defined(SCORE_OPENGL)
 setAttribute(Qt::WA_PaintOnScreen, true);
 setAttribute(Qt::WA_OpaquePaintEvent, true);
#endif

#if defined(__APPLE__)
  //setRenderHints(0);
  //setOptimizationFlag(QGraphicsView::IndirectPainting, true);
#endif

}

void ProcessGraphicsView::drawBackground(QPainter* painter, const QRectF& rect)
{
  /*
  const constexpr int N = 16;
  QImage img(QSize(N, N), QImage::Format_RGB32);
  auto light = ScenarioStyle::instance().Background.getColor().color().lighter(120);
  img.fill(ScenarioStyle::instance().Background.getColor().color());
  for(int i = 0; i < N; i++)
  {
    img.setPixelColor(i, 0, light);
    img.setPixelColor(0, i, light);
  }
  QPixmap par = QPixmap::fromImage(img);
  */
  painter->fillRect(rect, ScenarioStyle::instance().Background.getBrush());

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
  auto t = std::chrono::steady_clock::now();
  if(std::chrono::duration_cast<std::chrono::milliseconds>(t - m_lastwheel).count() < 16)
  {
    return;
  }

  m_lastwheel = t;
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
  struct MyWheelEvent : public QWheelEvent
  {
      MyWheelEvent(const QWheelEvent& other): QWheelEvent{other}
      { p.ry() /= 4.; pixelD.ry() /= 4.; angleD /= 4.; qt4D /= 4.; }
  };
  MyWheelEvent e{*event};
  QGraphicsView::wheelEvent(&e);
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
