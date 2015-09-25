#pragma once
#include "Document/Constraint/ViewModels/ConstraintHeader.hpp"

class TemporalConstraintHeader : public ConstraintHeader
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


        int type() const override
        { return QGraphicsItem::UserType + 6; }
    signals:
        void doubleClicked();

    protected:
        void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;

    private:
        int m_previous_x{};
};
