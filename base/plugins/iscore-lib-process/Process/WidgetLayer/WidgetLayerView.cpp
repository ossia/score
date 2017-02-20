#include "WidgetLayerView.hpp"
#include <QGraphicsProxyWidget>
#include <QPalette>
#include <QWidget>
namespace WidgetLayer
{

View::View(QQuickPaintedItem* parent) : LayerView{parent}
{
	/*
  m_widg = new QGraphicsProxyWidget{this};
  connect(this, &LayerView::heightChanged, this, [=] {
    m_widg->setGeometry(QRectF{0, 0, this->width() - 10, this->height() - 10});
  });
  connect(this, &LayerView::widthChanged, this, [=] {
    m_widg->setGeometry(QRectF{0, 0, this->width() - 10, this->height() - 10});
  });*/
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
}

void View::paint_impl(QPainter* painter) const
{
}

void View::mousePressEvent(QMouseEvent* ev)
{
  emit pressed();
}
void View::mouseMoveEvent(QMouseEvent* ev)
{
}
void View::mouseReleaseEvent(QMouseEvent* ev)
{
}
}
