#include "TemporalConstraintView.hpp"

#include "TemporalConstraintViewModel.hpp"
#include "TemporalConstraintPresenter.hpp"
#include "Document/Constraint/Box/BoxPresenter.hpp"
#include "Document/Constraint/Box/BoxView.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QApplication>

#include <thread>
#include <chrono>
TemporalConstraintView::TemporalConstraintView(TemporalConstraintPresenter &presenter,
                                               QGraphicsObject* parent) :
    AbstractConstraintView {presenter, parent}
{
    this->setParentItem(parent);

    this->setZValue(parent->zValue() + 1);
    this->setCursor(Qt::CrossCursor);
}

QRectF TemporalConstraintView::boundingRect() const
{
    return {0, -18, qreal(maxWidth()), qreal(constraintHeight()) };
}

void TemporalConstraintView::paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget)
{
    // Draw the box bg
    if(auto box = presenter().box())
    {
        auto boxRect = box->view().boundingRect();
        painter->fillRect(boxRect, QColor::fromRgba(qRgba(0, 127, 229, 76)));

        auto color = qApp->palette("ScenarioPalette").base().color();
        QPen pen{color, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin};
        painter->setPen(pen);
        painter->drawLine(boxRect.topLeft(), boxRect.bottomLeft());
        painter->drawLine(boxRect.topRight(), boxRect.bottomRight());
    }


    QPainterPath solidPath, dashedPath, leftBrace, rightBrace;

    // Paths
    if(minWidth() == maxWidth())
    {
        solidPath.lineTo(defaultWidth(), 0);
    }
    else if(infinite())
    {
        if(minWidth() != 0)
            solidPath.lineTo(minWidth(), 0);

        dashedPath.moveTo(minWidth(), 0);
        dashedPath.lineTo(defaultWidth(), 0);
    }
    else
    {
        if(minWidth() != 0)
            solidPath.lineTo(minWidth(), 0);

        dashedPath.moveTo(minWidth(), 0);
        dashedPath.lineTo(maxWidth(), 0);

        leftBrace.moveTo(minWidth(), -10);
        leftBrace.arcTo(minWidth() - 10, -10, 20, 20, 90, 180);

        rightBrace.moveTo(maxWidth(), 10);
        rightBrace.arcTo(maxWidth() - 10, -10, 20, 20, 270, 180);
    }

    QPainterPath playedPath;
    if(playWidth() != 0)
    {
        playedPath.lineTo(playWidth(), 0);
    }

    // Colors
    QColor constraintColor;
    if(isSelected())
    {
        constraintColor = QColor::fromRgbF(0.188235, 0.54902, 0.776471);
    }
    else
    {
        constraintColor = qApp->palette("ScenarioPalette").base().color();
    }
    if(warning())
    {
        constraintColor = QColor{200,150,0};
    }
    if(! isValid())
    {
        constraintColor = Qt::red;
    }

    m_solidPen.setColor(constraintColor);
    m_dashPen.setColor(constraintColor);

    // Drawing
    painter->setPen(m_solidPen);
    if(!solidPath.isEmpty())
        painter->drawPath(solidPath);
    if(!leftBrace.isEmpty())
        painter->drawPath(leftBrace);
    if(!rightBrace.isEmpty())
        painter->drawPath(rightBrace);

    painter->setPen(m_dashPen);
    if(!dashedPath.isEmpty())
        painter->drawPath(dashedPath);

    leftBrace.closeSubpath();
    rightBrace.closeSubpath();

    QPen anotherPen(Qt::transparent, 4);
    painter->setPen(anotherPen);
    QColor blueish = m_solidPen.color().lighter();
    blueish.setAlphaF(0.3);
    painter->setBrush(blueish);
    painter->drawPath(leftBrace);
    painter->drawPath(rightBrace);

    static const QPen playedPen{
        QBrush{Qt::green},
        4,
        Qt::SolidLine,
                Qt::RoundCap,
                Qt::RoundJoin
    };
    static const QPen dashedPlayedPen{
        QBrush{Qt::green},
        4,
        Qt::DashLine,
                Qt::RoundCap,
                Qt::RoundJoin
    };

    painter->setPen(playedPen);
    if(!playedPath.isEmpty())
        painter->drawPath(playedPath);


    QRectF labelRect{0,0, minWidth(), -height()/2};
    QFont f("Ubuntu");
    f.setPixelSize(12);
    painter->setFont(f);
    painter->setPen(m_labelColor);
    painter->drawText(labelRect, Qt::AlignCenter, m_label);
}

void TemporalConstraintView::hoverEnterEvent(QGraphicsSceneHoverEvent *h)
{
    QGraphicsObject::hoverEnterEvent(h);
    emit constraintHoverEnter();
}

void TemporalConstraintView::hoverLeaveEvent(QGraphicsSceneHoverEvent *h)
{
    QGraphicsObject::hoverLeaveEvent(h);
    emit constraintHoverLeave();
}
void TemporalConstraintView::setLabel(const QString &label)
{
    m_label = label;
}

void TemporalConstraintView::setLabelColor(const QColor &labelColor)
{
    m_labelColor = labelColor;
}

bool TemporalConstraintView::shadow() const
{
    return m_shadow;
}

void TemporalConstraintView::setShadow(bool shadow)
{
    m_shadow = shadow;
    update();
}

