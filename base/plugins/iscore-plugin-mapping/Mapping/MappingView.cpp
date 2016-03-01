#include <Process/Style/ProcessFonts.hpp>
#include <QFlags>
#include <QFont>
#include <QGraphicsItem>
#include <qnamespace.h>
#include <QPainter>
#include <QRect>

#include "MappingView.hpp"
#include <Process/LayerView.hpp>
#include <Process/Style/Skin.hpp>

namespace Mapping
{
MappingView::MappingView(QGraphicsItem* parent) :
    LayerView {parent}
{
    setZValue(1);
    this->setFlags(ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable);
}

void MappingView::paint_impl(QPainter* painter) const
{
    static const int fontSize = 8;
    QRectF processNameRect{10, fontSize, 0.95 * this->width(), fontSize * 6 };

    if(m_showName)
    {
        auto f = Skin::instance().SansFont;
        f.setPointSize(fontSize);
        painter->setFont(f);
        painter->setPen(Qt::lightGray);
        painter->drawText(processNameRect, Qt::AlignLeft, m_displayedName);
    }
}
}
