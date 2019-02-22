#pragma once

#include "CoherencyCheckerFactoryInterface.hpp"

#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/Interface.hpp>

namespace Scenario
{
class SCORE_PLUGIN_SCENARIO_EXPORT CSPCoherencyCheckerList final
    : public score::InterfaceList<CoherencyCheckerFactoryInterface>
{
public:
  CoherencyCheckerFactoryInterface* get() const;
};
}
