#pragma once
#include "Document/Constraint/ViewModels/ConstraintHeader.hpp"

class TemporalConstraintHeader : public ConstraintHeader
{
    public:
        TemporalConstraintHeader():
            ConstraintHeader{}
        {
            this->setAcceptedMouseButtons(0);
        }

        QRectF boundingRect() const override;
        void paint(QPainter *painter,
                   const QStyleOptionGraphicsItem *option,
                   QWidget *widget) override;

        int type() const override
        { return QGraphicsItem::UserType + 6; }

    private:
        int m_previous_x{};
};
