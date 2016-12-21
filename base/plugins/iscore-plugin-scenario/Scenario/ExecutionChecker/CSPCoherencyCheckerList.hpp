#pragma once

#include "CoherencyCheckerFactoryInterface.hpp"

#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>

namespace Scenario
{
class ISCORE_PLUGIN_SCENARIO_EXPORT CSPCoherencyCheckerList final
    : public iscore::InterfaceList<CoherencyCheckerFactoryInterface>
{
public:
  CoherencyCheckerFactoryInterface* get() const;
};
}
