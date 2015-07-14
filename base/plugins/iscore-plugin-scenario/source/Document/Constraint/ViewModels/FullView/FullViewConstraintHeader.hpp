#include "Document/Constraint/ViewModels/AbstractConstraintHeader.hpp"

class FullViewConstraintHeader : public ConstraintHeader
{
    public:
        QRectF boundingRect() const;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
};
