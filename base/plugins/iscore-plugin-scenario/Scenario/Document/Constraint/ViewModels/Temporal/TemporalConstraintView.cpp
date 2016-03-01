#include <Process/Style/ProcessFonts.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <QBrush>
#include <QFont>
#include <QGraphicsItem>
#include <qnamespace.h>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <Process/Style/Skin.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintPresenter.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintView.hpp>
#include "TemporalConstraintPresenter.hpp"
#include "TemporalConstraintView.hpp"
#include <Scenario/Document/Constraint/ViewModels/Temporal/Braces/LeftBrace.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/Braces/RightBrace.hpp>

class QGraphicsSceneHoverEvent;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
TemporalConstraintView::TemporalConstraintView(
        TemporalConstraintPresenter &presenter,
        QGraphicsObject* parent) :
    ConstraintView {presenter, parent},
    m_labelColor{ScenarioStyle::instance().ConstraintDefaultLabel},
    m_bgColor{ScenarioStyle::instance().ConstraintDefaultBackground}
{
    this->setParentItem(parent);

    this->setZValue(ZPos::Constraint);
    m_leftBrace = new LeftBraceView{*this, this};
    m_leftBrace->setX(minWidth());

    m_rightBrace = new RightBraceView{*this, this};
    m_rightBrace->setX(maxWidth());
}




void TemporalConstraintView::paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem*,
        QWidget*)
{
    qreal min_w = minWidth();
    qreal max_w = maxWidth();
    qreal def_w = defaultWidth();
    qreal play_w = playWidth();

    m_leftBrace->setX(min_w);
    m_rightBrace->setX(max_w);

    // Draw the stuff present if there is a rack *in the model* ?
    if(presenter().rack())
    {
        // Background
        auto rect = boundingRect();
        rect.adjust(0,4,0,-10);
        rect.setWidth(this->defaultWidth());
        painter->fillRect(rect, m_bgColor);

        // Fake timenode continuation
        auto color = ScenarioStyle::instance().RackSideBorder;
        QPen pen{color, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin};
        painter->setPen(pen);
        painter->drawLine(rect.topLeft(), rect.bottomLeft());
        painter->drawLine(rect.topRight(), rect.bottomRight());
    }


    QPainterPath solidPath, dashedPath, leftBrace, rightBrace;

    // Paths
    if(infinite())
    {
        if(min_w != 0.)
        {
            solidPath.lineTo(min_w, 0);

            m_leftBrace->show();
        }
        m_rightBrace->hide();

        // TODO end state should be hidden
        dashedPath.moveTo(min_w, 0);
        dashedPath.lineTo(def_w, 0);
    }
    else if(min_w == max_w) // TODO rigid()
    {
        solidPath.lineTo(def_w, 0);
        m_leftBrace->hide();
        m_rightBrace->hide();
    }
    else
    {
        if(min_w != 0.)
            solidPath.lineTo(min_w, 0);

        dashedPath.moveTo(min_w, 0);
        dashedPath.lineTo(max_w, 0);

        m_leftBrace->show();
        m_rightBrace->show();

    }

    QPainterPath playedPath;
    if(play_w != 0.)
    {
        playedPath.lineTo(play_w, 0);
    }

    // Colors
    QColor constraintColor;
    // TODO make a switch instead
    if(isSelected())
    {
        constraintColor = ScenarioStyle::instance().ConstraintSelected;
    }
    else if(warning())
    {
        constraintColor = ScenarioStyle::instance().ConstraintWarning;
    }
    else
    {
        constraintColor = ScenarioStyle::instance().ConstraintBase;
    }
    if(! isValid())
    {
        constraintColor = ScenarioStyle::instance().ConstraintInvalid;
    }


    m_solidPen.setColor(constraintColor);
    m_dashPen.setColor(constraintColor);

    // Drawing
    painter->setPen(m_solidPen);
    if(!solidPath.isEmpty())
        painter->drawPath(solidPath);

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


    static const QPen playedPen{
        QBrush{ScenarioStyle::instance().ConstraintPlayFill},
        4,
        Qt::SolidLine,
                Qt::RoundCap,
                Qt::RoundJoin
    };

    painter->setPen(playedPen);
    if(!playedPath.isEmpty())
        painter->drawPath(playedPath);


    static const int fontSize = 12;
    QRectF labelRect{0,0, defaultWidth(), (-fontSize - 2.)};
    auto f = Skin::instance().SansFont;
    f.setPointSize(fontSize);

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
}
