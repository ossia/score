#include "DummyLayerView.hpp"
#include <QPainter>
DummyLayerView::DummyLayerView(QGraphicsItem* parent):
    LayerView{parent}
{

}

void DummyLayerView::paint_impl(QPainter* painter) const
{
    static const int fontSize = 30;
    static const QFont f{[] () { QFont _f_("Ubuntu"); _f_.setPixelSize(fontSize); return _f_;}()};
    static const QColor c = Qt::lightGray;

    painter->setFont(f);
    painter->setPen(c);
    painter->drawText(boundingRect(), Qt::AlignCenter, m_text);
}
