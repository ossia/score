#include "FullViewConstraintHeader.hpp"
#include "AddressBarItem.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsView>
FullViewConstraintHeader::FullViewConstraintHeader(QGraphicsItem * parent):
    ConstraintHeader{parent},
    m_bar{new AddressBarItem(this)}
{
    m_bar->setPos(10, 5);
}

AddressBarItem *FullViewConstraintHeader::bar() const
{
    return m_bar;
}

QRectF FullViewConstraintHeader::boundingRect() const
{
    return {0, 0, m_width, ConstraintHeader::headerHeight()};
}


void FullViewConstraintHeader::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    double textWidth = m_bar->width();

    // If the centered text is hidden, we put it at the left so that it's on the view.
    // We have to compute the visible part of the header
    auto view = scene()->views().first();

    // Note: if the constraint always has its pos() in (0; 0), we can
    // safely remove the call to mapToScene.
    double text_left = view->mapFromScene(mapToScene({m_width / 2. - textWidth / 2., 0})).x();
    double text_right = view->mapFromScene(mapToScene({m_width / 2. + textWidth / 2., 0})).x();
    double x = 0;
    double min_x = 10;
    double max_x = view->width() - 30;

    if(text_left > min_x && text_right < max_x)
    {
        x = m_width / 2. - textWidth / 2.;
    }
    else if(text_left < min_x)
    {
        // Compute the pixels needed to add to have top-left at 0
        x = (m_width - textWidth) / 2. - text_left + min_x;
    }
    else if(text_right > max_x)
    {
        // Compute the pixels needed to add to have top-right at max
        x = (m_width - textWidth) / 2. - text_right + max_x;
    }
    else
    {
        return;
    }

    if(std::abs(m_bar->pos().x() - x) > 1)
        m_bar->setPos(x, 5);
}
