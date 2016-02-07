#pragma once
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactory.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/tools/std/StdlibWrapper.hpp>
#include <iscore/tools/std/Algorithms.hpp>

namespace Scenario
{
namespace Command
{
class TriggerCommandFactoryList final :
        public iscore::MatchingFactory<TriggerCommandFactory>
{
};
}
}
