// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "WidgetLayerView.hpp"

#include <QGraphicsProxyWidget>
#include <QGraphicsSceneMouseEvent>
#include <QPalette>
#include <QWidget>

#include <wobjectimpl.h>
W_OBJECT_IMPL(WidgetLayer::View)
namespace WidgetLayer
{

View::View(QGraphicsItem* parent) : LayerView{parent}
{
  m_widg = new QGraphicsProxyWidget{this};
}

void View::heightChanged(qreal h)
{
  m_widg->setGeometry(QRectF{0,
                             0,
                             std::max(0., this->width() - 10),
                             std::max(0., this->height() - 10)});
  bool visible = m_widg->isVisible();
  bool enough_space = this->width() > 21 && this->height() > 21;
  if (!enough_space && visible)
    m_widg->setVisible(false);
  else if (enough_space && !visible)
    m_widg->setVisible(true);
}

void View::widthChanged(qreal w)
{
  heightChanged(w);
}
void View::setWidget(QWidget* w)
{
  m_widg->setWidget(w);
  m_widg->setContentsMargins(0, 0, 0, 0);

  QPalette palette;
  palette.setBrush(QPalette::Window, Qt::transparent);
  w->setPalette(palette);

  w->setAutoFillBackground(false);
#ifndef QT_NO_STYLE_STYLESHEET
  w->setStyleSheet("QWidget { background-color:transparent }");
#endif

  connect(w, SIGNAL(pressed()), this, SIGNAL(pressed()));
  // connect(w, SIGNAL(contextMenuRequested(QPoint)), this,
  // SIGNAL(contextMenuRequested(QPoint)));
}

void View::paint_impl(QPainter* painter) const {}

void View::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  if (ev->button() == Qt::RightButton)
  {
    askContextMenu(ev->screenPos(), ev->scenePos());
  }
  else
  {
    pressed(ev->scenePos());
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
  askContextMenu(ev->screenPos(), ev->scenePos());
  ev->accept();
}
}
