#pragma once
#include <QList>

namespace score
{
class CommandStackFacade;
struct DocumentContext;
}

namespace Scenario
{
class IntervalModel;
namespace Command
{
void InterpolateStates(
    const QList<const IntervalModel*>&, const score::CommandStackFacade&);
}
}
