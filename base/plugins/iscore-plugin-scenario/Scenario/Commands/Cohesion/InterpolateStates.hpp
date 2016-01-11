#pragma once
#include <QList>

namespace iscore
{
class CommandStackFacade;
struct DocumentContext;
}

namespace Scenario
{
class ConstraintModel;

void InterpolateStates(const QList<const ConstraintModel*>&,
                       iscore::CommandStackFacade&);
}
