#include "Document/Constraint/ViewModels/AbstractConstraintHeader.hpp"

class TemporalConstraintHeader : public ConstraintHeader
{
    public:
        QRectF boundingRect() const;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
};
