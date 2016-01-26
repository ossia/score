#include "RightBrace.hpp"

using namespace Scenario;

RightBraceView::RightBraceView(const TemporalConstraintView& parentCstr,
                       QGraphicsItem* parent):
    ConstraintBrace{parentCstr, parent}
{
    this->setRotation(180);
}
