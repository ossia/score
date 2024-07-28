#include "WidgetInletFactory.hpp"

#include <QGraphicsScene>
#include <QGraphicsView>

namespace Dataflow
{

QWidget* makeGraphicsViewForInspectorItem(QGraphicsItem* item, QWidget* parent)
{
  auto s = new QGraphicsScene;
  auto widg = new QGraphicsView(s, parent);
  s->setParent(widg);
  widg->setMinimumWidth(item->boundingRect().width());
  widg->setMinimumHeight(item->boundingRect().height());
  widg->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  widg->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  widg->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  widg->setFrameStyle(0);
  widg->setDragMode(QGraphicsView::NoDrag);
  widg->setAcceptDrops(true);
  widg->setFocusPolicy(Qt::WheelFocus);
  widg->setRenderHints(
      QPainter::Antialiasing | QPainter::SmoothPixmapTransform
      | QPainter::TextAntialiasing);

#if !defined(__EMSCRIPTEN__)
  widg->setOptimizationFlag(QGraphicsView::DontSavePainterState, true);
  widg->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
  // widg->setAttribute(Qt::WA_PaintOnScreen, false);
  // widg->setAttribute(Qt::WA_OpaquePaintEvent, true);
  // widg->viewport()->setAttribute(Qt::WA_OpaquePaintEvent, true);
#endif
  s->addItem(item);
  widg->centerOn(item);
  return widg;
}

}
