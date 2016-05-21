#include "CSPCoherencyCheckerList.hpp"

namespace Scenario
{
CoherencyCheckerFactoryInterface* CSPCoherencyCheckerList::get() const
{
    if(map.begin() != map.end())
        return map.begin()->second.get();
    else
        return nullptr;
}
}
