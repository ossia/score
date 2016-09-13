#include "LeftBrace.hpp"

#include <QGraphicsSceneMouseEvent>

using namespace Scenario;

LeftBraceView::LeftBraceView(const TemporalConstraintView& parentCstr, QGraphicsItem* parent):
    ConstraintBrace{parentCstr, parent}
{
}
