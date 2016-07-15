#include "WidgetLayerView.hpp"
#include <QGraphicsProxyWidget>
namespace WidgetLayer
{

View::View(QGraphicsItem* parent):
    LayerView{parent}
{
    m_widg = new QGraphicsProxyWidget{this};
    connect(this, &LayerView::heightChanged,
            this, [=] { m_widg->setGeometry(QRectF{0, 0, this->width() - 10, this->height() - 10});});
    connect(this, &LayerView::widthChanged,
            this, [=] { m_widg->setGeometry(QRectF{0, 0, this->width() - 10, this->height() - 10});});
}

void View::setWidget(QWidget* w)
{
    m_widg->setWidget(w);
    m_widg->setContentsMargins(0, 0, 0, 0);
}


void View::paint_impl(QPainter* painter) const
{
}

void View::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
    emit pressed();
}
void View::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
}
void View::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
}
}
