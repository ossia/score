#pragma once
#include <QList>

namespace iscore
{
class CommandStackFacade;
struct DocumentContext;
}

namespace Scenario
{
class StateModel;
namespace Command
{
void RefreshStates(const iscore::DocumentContext& doc);
void RefreshStates(const QList<const StateModel*>&,
                   const iscore::CommandStackFacade&);
}
}
