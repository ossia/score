#include "Document/Constraint/ViewModels/AbstractConstraintHeader.hpp"

class FullViewConstraintHeader : public ConstraintHeader
{
    public:
        using ConstraintHeader::ConstraintHeader;
        QRectF boundingRect() const;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
};
