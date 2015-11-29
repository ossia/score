#include <Process/Style/ProcessFonts.hpp>
#include <qflags.h>
#include <qfont.h>
#include <qgraphicsitem.h>
#include <qnamespace.h>
#include <qpainter.h>
#include <qrect.h>

#include "AutomationView.hpp"
#include "Process/LayerView.hpp"

AutomationView::AutomationView(QGraphicsItem* parent) :
    LayerView {parent}
{
    setZValue(1);
    this->setFlags(ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable);
}

void AutomationView::paint_impl(QPainter* painter) const
{
    static const int fontSize = 10;
    QRectF processNameRect{0, this->height() - 2*fontSize, 0.95 * this->width(), fontSize + 2 };

    if(m_showName)
    {
        auto f = ProcessFonts::Sans();
        f.setPointSize(fontSize);
        painter->setFont(f);
        painter->setPen(Qt::lightGray);
        painter->drawText(processNameRect, Qt::AlignCenter, m_displayedName);
    }
}
