#pragma once
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactory.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/tools/std/StdlibWrapper.hpp>
#include <iscore/tools/std/Algorithms.hpp>

namespace Scenario
{
namespace Command
{
class TriggerCommandFactoryList final : public iscore::FactoryListInterface
{
    public:
        // TODO refactor this with constraint inspector blablabli
      static const iscore::FactoryBaseKey& staticFactoryKey() {
          return TriggerCommandFactory::staticFactoryKey();
      }

      iscore::FactoryBaseKey name() const final override {
          return TriggerCommandFactory::staticFactoryKey();
      }

      void insert(std::unique_ptr<iscore::FactoryInterfaceBase> e) final override
      {
          if(auto pf = dynamic_unique_ptr_cast<TriggerCommandFactory>(std::move(e)))
              m_list.emplace_back(std::move(pf));
      }

      const auto& list() const
      {
          return m_list;
      }

      auto make_addTriggerCommand(const TimeNodeModel& tn) const
      {
          auto it = find_if(m_list, [&] (const auto& elt) { return elt->matches(tn); });
          return (it != m_list.end())
                  ? (*it)->make_addTriggerCommand(tn)
                  : nullptr;
      }
      auto make_removeTriggerCommand(const TimeNodeModel& tn) const
      {
          auto it = find_if(m_list, [&] (const auto& elt) { return elt->matches(tn); });
          return (it != m_list.end())
                  ? (*it)->make_removeTriggerCommand(tn)
                  : nullptr;
      }

    private:
      std::vector<std::unique_ptr<TriggerCommandFactory>> m_list;
};
}
}
