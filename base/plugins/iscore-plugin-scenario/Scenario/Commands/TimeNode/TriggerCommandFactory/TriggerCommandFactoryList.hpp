#pragma once
#include <Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactory.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/tools/std/StdlibWrapper.hpp>

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

      void insert(iscore::FactoryInterfaceBase* e) final override
      {
          if(auto pf = dynamic_cast<TriggerCommandFactory*>(e))
              m_list.push_back(pf);
      }

      const auto& list() const
      {
          return m_list;
      }

      auto make_addTriggerCommand(const TimeNodeModel& tn) const
      {
          auto it = find_if(m_list, [&] (auto elt) { return elt->matches(tn); });
          return (it != m_list.end())
                  ? (*it)->make_addTriggerCommand(tn)
                  : nullptr;
      }
      auto make_removeTriggerCommand(const TimeNodeModel& tn) const
      {
          auto it = find_if(m_list, [&] (auto elt) { return elt->matches(tn); });
          return (it != m_list.end())
                  ? (*it)->make_removeTriggerCommand(tn)
                  : nullptr;
      }

    private:
      std::vector<TriggerCommandFactory*> m_list;
};
