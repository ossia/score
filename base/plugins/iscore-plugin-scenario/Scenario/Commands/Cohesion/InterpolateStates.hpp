#pragma once
#include <QList>

namespace iscore
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
    const QList<const IntervalModel*>&, const iscore::CommandStackFacade&);
}
}
