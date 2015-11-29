#pragma once
#include <Scenario/Document/Constraint/ViewModels/ConstraintHeader.hpp>
#include <qgraphicsitem.h>
#include <qnamespace.h>
#include <qrect.h>

class QGraphicsSceneMouseEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

class TemporalConstraintHeader final : public ConstraintHeader
{
        Q_OBJECT
    public:
        TemporalConstraintHeader():
            ConstraintHeader{}
        {
            this->setAcceptedMouseButtons(Qt::LeftButton);  // needs to be enabled for dblclick
            this->setFlags(QGraphicsItem::ItemIsSelectable);// needs to be enabled for dblclick
        }

        QRectF boundingRect() const override;
        void paint(QPainter *painter,
                   const QStyleOptionGraphicsItem *option,
                   QWidget *widget) override;
    signals:
        void doubleClicked();

    protected:
        void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

    private:
        int m_previous_x{};
};
