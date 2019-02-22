#pragma once
#include <QList>

namespace score
{
class CommandStackFacade;
struct DocumentContext;
}

namespace Scenario
{
class StateModel;
namespace Command
{
void RefreshStates(const score::DocumentContext& doc);
void RefreshStates(
    const QList<const StateModel*>&, const score::DocumentContext&);
}
}
