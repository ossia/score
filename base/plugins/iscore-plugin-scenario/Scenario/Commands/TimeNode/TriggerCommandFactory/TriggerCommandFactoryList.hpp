#pragma once
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactory.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <ossia/detail/algorithms.hpp>

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
