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
namespace Command
{
void InterpolateStates(const QList<const ConstraintModel*>&,
                       iscore::CommandStackFacade&);
}
}
