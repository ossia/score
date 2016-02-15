#include "CSPCoherencyCheckerList.hpp"

namespace Scenario
{
CoherencyCheckerFactoryInterface* CSPCoherencyCheckerList::get() const
{
    if(begin() != end())
        return map.begin()->get();
    else
        return nullptr;
}
}
