// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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
