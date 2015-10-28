#include "DummyLayerView.hpp"

DummyLayerView::DummyLayerView(QGraphicsItem* parent):
    LayerView{parent}
{

}

void DummyLayerView::paint_impl(QPainter*) const
{
}
