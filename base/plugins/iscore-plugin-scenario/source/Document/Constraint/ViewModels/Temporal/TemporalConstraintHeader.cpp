#include "TemporalConstraintHeader.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <QFont>
#include <QPainter>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QApplication>
#include <cmath>
QRectF TemporalConstraintHeader::boundingRect() const
{
    return {0, 0, m_width, ConstraintHeader::headerHeight()};
}

void TemporalConstraintHeader::paint(
        QPainter *painter,
        const QStyleOptionGraphicsItem *option,
        QWidget *widget)
{
    if(m_state == State::RackHidden)
    {
        auto rect = boundingRect();
        painter->fillRect(rect, QColor::fromRgba(qRgba(0, 127, 229, 76)));

        // Fake timenode continuation
        auto color = qApp->palette("ScenarioPalette").base().color();
        QPen pen{color, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin};
        painter->setPen(pen);
        painter->drawLine(rect.topLeft(), rect.bottomLeft());
        painter->drawLine(rect.topRight(), rect.bottomRight());
        painter->drawLine(rect.bottomLeft(), rect.bottomRight());
    }
    // Header
    painter->setFont(font);
    painter->setPen(Qt::white);

    QFontMetrics fm(font);
    int textWidth = fm.width(m_text);

    // If the centered text is hidden, we put it at the left so that it's on the view.
    // We have to compute the visible part of the header
    auto view = scene()->views().first();
    int text_left = view->mapFromScene(mapToScene({m_width / 2. - textWidth / 2., 0})).x();
    int text_right = view->mapFromScene(mapToScene({m_width / 2. + textWidth / 2., 0})).x();
    double x = (m_width - textWidth) / 2.;
    double min_x = 10;
    double max_x = view->width() - 30;

    if(text_left <= min_x)
    {
        // Compute the pixels needed to add to have top-left at 0
        x = x - text_left + min_x;
    }
    else if(text_right >= max_x)
    {
        // Compute the pixels needed to add to have top-right at max
        x = x - text_right + max_x;
    }

    x = std::max(x, 10.);
    double y = 2.5;
    double w = m_width - x;
    double h = ConstraintHeader::headerHeight();


    if(std::abs(m_previous_x - x) > 1)
    {
        m_previous_x = x;
    }
    painter->drawText(m_previous_x,y,w,h, Qt::AlignLeft, m_text);

    if(m_width > 20)
    {
        painter->setPen(qRgba(0, 127, 229, 76));
        painter->drawLine(
                    boundingRect().bottomLeft(),
                    boundingRect().bottomRight());
    }
}

void TemporalConstraintHeader::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    emit doubleClicked();
}
