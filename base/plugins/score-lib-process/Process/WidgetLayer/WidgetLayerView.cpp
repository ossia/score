// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "WidgetLayerView.hpp"
#include <QGraphicsProxyWidget>
#include <QPalette>
#include <QWidget>
#include <QGraphicsSceneMouseEvent>
namespace WidgetLayer
{

View::View(QGraphicsItem* parent) : LayerView{parent}
{
  m_widg = new QGraphicsProxyWidget{this};
  connect(this, &LayerView::heightChanged, this, [=] {
    m_widg->setGeometry(QRectF{0, 0, this->width() - 10, this->height() - 10});
  });
  connect(this, &LayerView::widthChanged, this, [=] {
    m_widg->setGeometry(QRectF{0, 0, this->width() - 10, this->height() - 10});
  });
}

void View::setWidget(QWidget* w)
{
  m_widg->setWidget(w);
  m_widg->setContentsMargins(0, 0, 0, 0);

  QPalette palette;
  palette.setBrush(QPalette::Background, Qt::transparent);
  w->setPalette(palette);

  w->setAutoFillBackground(false);
  w->setStyleSheet("QWidget { background-color:transparent }");

  connect(w, SIGNAL(pressed()), this, SIGNAL(pressed()));
  //connect(w, SIGNAL(contextMenuRequested(QPoint)), this, SIGNAL(contextMenuRequested(QPoint)));
}

void View::paint_impl(QPainter* painter) const
{
}

void View::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  if(ev->button() == Qt::RightButton)
  {
    emit askContextMenu(ev->screenPos(), ev->scenePos());
  }
  else
  {
    emit pressed(ev->scenePos());
  }
  ev->accept();
}

void View::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
}

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
}

void View::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
  emit askContextMenu(ev->screenPos(), ev->scenePos());
  ev->accept();
}


}
