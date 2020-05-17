#pragma once
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactory.hpp>

#include <score/plugins/InterfaceList.hpp>

#include <ossia/detail/algorithms.hpp>

namespace Scenario
{
namespace Command
{
class TriggerCommandFactoryList final : public score::MatchingFactory<TriggerCommandFactory>
{
};
}
}
