#include "AutomationView.hpp"

#include <QPainter>
#include <QKeyEvent>
#include <QDebug>
AutomationView::AutomationView(QGraphicsItem* parent) :
    LayerView {parent}
{
    setZValue(parent->zValue() + 1);
    this->setFlags(ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable);
}

void AutomationView::paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget)
{
    static const int fontSize = 10;

    QRectF processNameRect{0, this->height() - 2*fontSize, 0.95 * this->width(), fontSize + 2 };

    static const QFont f{[] () { static QFont _f_("Ubuntu"); _f_.setPixelSize(fontSize); return _f_;}()};

    if(m_showName)
    {
        painter->setFont(f);
        QColor c = Qt::lightGray;
        painter->setPen(c);
        painter->drawText(processNameRect, Qt::AlignRight, m_displayedName);
    }
}
