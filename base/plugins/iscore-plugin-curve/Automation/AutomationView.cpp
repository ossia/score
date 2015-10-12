#include "AutomationView.hpp"

#include <QPainter>
#include <QKeyEvent>
#include <QDebug>

static const int fontSize = 10;
static const QFont f{[] () { static QFont _f_("Ubuntu"); _f_.setPixelSize(fontSize); return _f_;}()};
static const QColor c = Qt::lightGray;

AutomationView::AutomationView(QGraphicsItem* parent) :
    LayerView {parent}
{
    setZValue(1);
    this->setFlags(ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable);
}

void AutomationView::paint_impl(QPainter* painter) const
{
    QRectF processNameRect{0, this->height() - 2*fontSize, 0.95 * this->width(), fontSize + 2 };

    if(m_showName)
    {
        painter->setFont(f);
        painter->setPen(c);
        painter->drawText(processNameRect, Qt::AlignRight, m_displayedName);
    }
}
