#pragma once
#include <vector>
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
void RefreshStates(const std::vector<const StateModel*>&, const score::DocumentContext&);
}
}
