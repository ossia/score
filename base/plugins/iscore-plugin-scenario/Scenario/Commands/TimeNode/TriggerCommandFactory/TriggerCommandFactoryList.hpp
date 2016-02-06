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
        public iscore::ConcreteFactoryList<TriggerCommandFactory>
{
    public:
      auto make_addTriggerCommand(const TimeNodeModel& tn) const
      {
          auto it = find_if(*this, [&] (const auto& elt) { return elt.matches(tn); });
          return (it != end())
                  ? it->make_addTriggerCommand(tn)
                  : nullptr;
      }
      auto make_removeTriggerCommand(const TimeNodeModel& tn) const
      {
          auto it = find_if(*this, [&] (const auto& elt) { return elt.matches(tn); });
          return (it != end())
                  ? it->make_removeTriggerCommand(tn)
                  : nullptr;
      }
};
}
}
