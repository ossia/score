#include "TemporalConstraintView.hpp"

#include "TemporalConstraintViewModel.hpp"
#include "TemporalConstraintPresenter.hpp"
#include "TemporalConstraintHeader.hpp"
#include "Document/Constraint/Rack/RackPresenter.hpp"
#include "Document/Constraint/Rack/RackView.hpp"
#include "Document/State/StateView.hpp"

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsSceneMouseEvent>
#include <QApplication>

#include <thread>
#include <chrono>
TemporalConstraintView::TemporalConstraintView(TemporalConstraintPresenter &presenter,
                                               QGraphicsObject* parent) :
    ConstraintView {presenter, parent}
{
    this->setParentItem(parent);

    this->setZValue(parent->zValue() + 3);
}


QRectF TemporalConstraintView::boundingRect() const
{
    return {0, -15, qreal(maxWidth()), qreal(constraintHeight()) };
}

void TemporalConstraintView::paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget)
{
    int min_w = static_cast<int>(minWidth());
    int max_w = static_cast<int>(maxWidth());
    int def_w = static_cast<int>(defaultWidth());

    // Draw the stuff present if there is a rack *in the model* ?
    if(presenter().rack())
    {
        // Background
        auto rect = boundingRect();
        rect.adjust(0,15,0,-10);
        rect.setWidth(this->defaultWidth());
        painter->fillRect(rect, QColor::fromRgba(qRgba(0, 127, 229, 76)));

        // Fake timenode continuation
        auto color = qApp->palette("ScenarioPalette").base().color();
        QPen pen{color, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin};
        painter->setPen(pen);
        painter->drawLine(rect.topLeft(), rect.bottomLeft());
        painter->drawLine(rect.topRight(), rect.bottomRight());
    }


    QPainterPath solidPath, dashedPath, leftBrace, rightBrace;

    // Paths
    if(infinite())
    {
        if(min_w != 0)
        {
            solidPath.lineTo(min_w, 0);

            leftBrace.moveTo(min_w, -10);
            leftBrace.arcTo(min_w - 10, -10, 20, 20, 90, 180);
        }

        // TODO end state should be hidden
        dashedPath.moveTo(min_w, 0);
        dashedPath.lineTo(def_w, 0);
    }
    else if(min_w == max_w) // TODO rigid()
    {
        solidPath.lineTo(def_w, 0);
    }
    else
    {
        if(min_w != 0)
            solidPath.lineTo(min_w, 0);

        dashedPath.moveTo(min_w, 0);
        dashedPath.lineTo(max_w, 0);

        leftBrace.moveTo(min_w, -10);
        leftBrace.arcTo(min_w - 10, -10, 20, 20, 90, 180);

        rightBrace.moveTo(max_w, 10);
        rightBrace.arcTo(max_w - 10, -10, 20, 20, 270, 180);
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
        this->setZValue(this->zValue()+ 1);
    }
    else
    {
        this->setZValue(parentObject()->zValue() + 3);
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
        QBrush{QColor::fromRgb(34, 224, 0)},
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


    static const int fontSize = 12;
    QRectF labelRect{0,0, defaultWidth(), (-fontSize - 2.)};
    static const QFont f{[] () { static QFont _f_("Ubuntu"); _f_.setPixelSize(fontSize); return _f_;}()};

    painter->setFont(f);
    painter->setPen(m_labelColor);
    painter->drawText(labelRect, Qt::AlignCenter, m_label);

#if defined(ISCORE_SCENARIO_DEBUG_RECTS)
    painter->setPen(Qt::darkRed);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(boundingRect());
#endif
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
    update();
}

void TemporalConstraintView::setLabelColor(const QColor &labelColor)
{
    m_labelColor = labelColor;
    update();
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

