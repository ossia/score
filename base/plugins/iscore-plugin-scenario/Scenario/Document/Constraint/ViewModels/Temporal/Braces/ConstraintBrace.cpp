#include "ConstraintBrace.hpp"
#include <QPen>
#include <QPainter>
#include <QGraphicsSceneEvent>
#include <QCursor>

#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintPresenter.hpp>

using namespace Scenario;

ConstraintBrace::ConstraintBrace(const TemporalConstraintView& parentCstr, QGraphicsItem* parent):
    QGraphicsObject(parent),
    m_parent{parentCstr}
{
    this->setCursor(Qt::SizeHorCursor);
    this->setZValue(ZPos::Brace);

    m_path.moveTo(10, -10);
    m_path.arcTo(0, -10, 20, 20, 90, 180);
}

QRectF ConstraintBrace::boundingRect() const
{
    return {0, -10, 10, 20};
}

void ConstraintBrace::paint(QPainter* painter,
                            const QStyleOptionGraphicsItem* option,
                            QWidget* widget)
{
    QColor constraintColor;
    // TODO make a switch instead
    if(m_parent.isSelected())
    {
        constraintColor = ScenarioStyle::instance().ConstraintSelected.getColor();
    }
    else if(m_parent.warning())
    {
        constraintColor = ScenarioStyle::instance().ConstraintWarning.getColor();
    }
    else
    {
        constraintColor = ScenarioStyle::instance().ConstraintBase.getColor();
    }
    if(! m_parent.isValid())
    {
        constraintColor = ScenarioStyle::instance().ConstraintInvalid.getColor();
    }


    QPen pen{constraintColor, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin};
    painter->setPen(pen);

    painter->drawPath(m_path);

#if defined(ISCORE_SCENARIO_DEBUG_RECTS)
    painter->setPen(Qt::lightGray);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(boundingRect());
#endif

}

void ConstraintBrace::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if(event->button() == Qt::MouseButton::LeftButton)
        emit m_parent.presenter().pressed(event->scenePos());
}

void ConstraintBrace::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
        emit m_parent.presenter().moved(event->scenePos());

}

void ConstraintBrace::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
        emit m_parent.presenter().released(event->scenePos());
}
