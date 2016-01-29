#include <Process/Style/ProcessFonts.hpp>
#include <QFlags>
#include <QFont>
#include <QGraphicsItem>
#include <qnamespace.h>
#include <QPainter>
#include <QRect>

#include "AutomationView.hpp"
#include <Process/LayerView.hpp>

namespace Automation
{
LayerView::LayerView(QGraphicsItem* parent) :
    Process::LayerView {parent}
{
    setZValue(1);
    this->setFlags(ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable);
}

void LayerView::paint_impl(QPainter* painter) const
{
    static const int fontSize = 8;
    QRectF processNameRect{0, this->height() - 2*fontSize, 0.95 * this->width(), fontSize + 2 };

#if !defined(ISCORE_IEEE_SKIN)
    if(m_showName)
    {
        auto f = Process::Fonts::Sans();
        f.setPointSize(fontSize);
        painter->setFont(f);
        painter->setPen(Qt::lightGray);
        painter->drawText(processNameRect, Qt::AlignCenter, m_displayedName);
    }
#endif
}
}
