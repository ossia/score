#pragma once
#include "Document/Constraint/ViewModels/ConstraintHeader.hpp"

class AddressBarItem;
class FullViewConstraintHeader : public ConstraintHeader
{
    public:
        FullViewConstraintHeader(QGraphicsItem*);

        AddressBarItem* bar() const;

        QRectF boundingRect() const;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    private:
        AddressBarItem* m_bar{};
};
