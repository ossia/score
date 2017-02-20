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
    : QQuickWidget{}
{
	/*
  setAlignment(Qt::AlignTop | Qt::AlignLeft);
  setViewportUpdateMode(QQuickWidget::SmartViewportUpdate);
  setRenderHints(
      QPainter::Antialiasing | QPainter::SmoothPixmapTransform
      | QPainter::TextAntialiasing);

  setFrameStyle(0);
  setCacheMode(QQuickWidget::CacheBackground);

  setOptimizationFlag(QQuickWidget::DontSavePainterState, true);
  */
#if !defined(ISCORE_OPENGL)
// setAttribute(Qt::WA_PaintOnScreen, true);
// setAttribute(Qt::WA_OpaquePaintEvent, true);
#endif

#if defined(__APPLE__)
  setRenderHints(0);
  setOptimizationFlag(QQuickWidget::IndirectPainting, true);
#endif

  // m_graduations = new SceneGraduations{this};
  // scene()->addItem(m_graduations);

  // m_graduations->setColor(m_bg.color().lighter());
  //this->setBackgroundBrush(ScenarioStyle::instance().Background.getBrush());
}

void ProcessGraphicsView::setGrid(QPainterPath&& newGrid)
{
  // m_graduations->setGrid(std::move(newGrid));
}

void ProcessGraphicsView::scrollHorizontal(double dx)
{
	/*
  if (auto bar = horizontalScrollBar())
  {
    bar->setValue(bar->value() + dx);
  }
  */
}

void ProcessGraphicsView::resizeEvent(QResizeEvent* ev)
{
  QQuickWidget::resizeEvent(ev);
  emit sizeChanged(size());
}
/*
void ProcessGraphicsView::scrollContentsBy(int dx, int dy)
{
  QQuickWidget::scrollContentsBy(dx, dy);

  this->scene()->update();
  emit scrolled(dx);
}
*/
void ProcessGraphicsView::wheelEvent(QWheelEvent* event)
{
	/*
  QPoint delta = event->angleDelta() / 8;
  if (m_zoomModifier)
  {
    emit zoom(delta, mapToScene(event->pos()));
    return;
  }

  QQuickWidget::wheelEvent(event);
  */
}

void ProcessGraphicsView::keyPressEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Control)
    m_zoomModifier = true;
  event->ignore();

  QQuickWidget::keyPressEvent(event);
}

void ProcessGraphicsView::keyReleaseEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Control)
    m_zoomModifier = false;
  event->ignore();

  QQuickWidget::keyReleaseEvent(event);
}

void ProcessGraphicsView::focusOutEvent(QFocusEvent* event)
{
  m_zoomModifier = false;
  event->ignore();

  QQuickWidget::focusOutEvent(event);
}
