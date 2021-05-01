#pragma once
#include <score/plugins/InterfaceList.hpp>

#include <ossia/detail/algorithms.hpp>

#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactory.hpp>

namespace Scenario
{
namespace Command
{
class TriggerCommandFactoryList final
    : public score::MatchingFactory<TriggerCommandFactory>
{
};
}
}
