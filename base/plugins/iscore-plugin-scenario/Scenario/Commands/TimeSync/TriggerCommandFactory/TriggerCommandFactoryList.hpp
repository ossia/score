#pragma once
#include <ossia/detail/algorithms.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactory.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

namespace Scenario
{
namespace Command
{
class TriggerCommandFactoryList final
    : public iscore::MatchingFactory<TriggerCommandFactory>
{
};
}
}
