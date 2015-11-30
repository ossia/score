#include <Process/Style/ProcessFonts.hpp>
#include <QFont>
#include <qnamespace.h>
#include <QPainter>

#include "DummyLayerView.hpp"
#include <Process/LayerView.hpp>

class QGraphicsItem;

DummyLayerView::DummyLayerView(QGraphicsItem* parent):
    LayerView{parent}
{

}

void DummyLayerView::paint_impl(QPainter* painter) const
{
    auto f = ProcessFonts::Sans();
    f.setPointSize(30);
    painter->setFont(f);
    painter->setPen(Qt::lightGray);

    painter->drawText(boundingRect(), Qt::AlignCenter, m_text);
}
