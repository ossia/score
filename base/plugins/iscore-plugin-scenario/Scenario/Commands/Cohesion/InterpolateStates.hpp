#pragma once
#include <QList>

namespace iscore
{
class CommandStackFacade;
struct DocumentContext;
}
class ConstraintModel;

void InterpolateStates(const QList<const ConstraintModel*>&,
                       iscore::CommandStackFacade&);
