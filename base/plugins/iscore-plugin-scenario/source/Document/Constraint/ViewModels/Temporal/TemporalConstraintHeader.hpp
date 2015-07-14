#pragma once
#include "Document/Constraint/ViewModels/ConstraintHeader.hpp"

class TemporalConstraintHeader : public ConstraintHeader
{
    public:
        using ConstraintHeader::ConstraintHeader;
        QRectF boundingRect() const;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
};
