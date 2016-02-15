#pragma once

#include "CoherencyCheckerFactoryInterface.hpp"

#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>

namespace Scenario
{
class CSPCoherencyCheckerList final :
         public iscore::ConcreteFactoryList<CoherencyCheckerFactoryInterface>
{
    public:
        CoherencyCheckerFactoryInterface* get();
};

}
