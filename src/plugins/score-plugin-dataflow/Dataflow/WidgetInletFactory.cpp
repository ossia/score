#include "WidgetInletFactory.hpp"

#include <score/graphics/widgets/QGraphicsValueList.hpp>

#include <QGraphicsScene>
#include <QGraphicsView>

#include <cmath>

namespace Dataflow
{

QWidget* makeGraphicsViewForInspectorItem(QGraphicsItem* item, QWidget* parent)
{
  auto s = new QGraphicsScene;
  auto widg = new QGraphicsView(s, parent);
  s->setParent(widg);
  widg->setMinimumWidth(item->boundingRect().width());
  widg->setMinimumHeight(item->boundingRect().height());
  // The form layout of the inspector gives extra height to expanding widgets:
  // the view is exactly as tall as its item.
  widg->setMaximumHeight(std::ceil(item->boundingRect().height()));
  widg->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
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

  // Lists grow and shrink with their rows: the view follows
  if(auto list = qgraphicsitem_cast<score::QGraphicsValueList*>(item))
  {
    QObject::connect(list, &score::QGraphicsValueList::sizeChanged, widg, [widg, s, list] {
      const QRectF r = list->boundingRect();
      s->setSceneRect(r);
      widg->setMinimumWidth(r.width());
      widg->setMinimumHeight(r.height());
      widg->setMaximumHeight(std::ceil(r.height()));
      widg->updateGeometry();
    });
    s->setSceneRect(list->boundingRect());
  }
  return widg;
}

}
