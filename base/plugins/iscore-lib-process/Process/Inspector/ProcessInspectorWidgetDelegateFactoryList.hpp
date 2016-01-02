#pragma once
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <iscore/tools/std/Algorithms.hpp>
class ProcessInspectorWidgetDelegateFactoryList final : public iscore::FactoryListInterface
{
    public:
      static const iscore::FactoryBaseKey& staticFactoryKey() {
          return ProcessInspectorWidgetDelegateFactory::staticFactoryKey();
      }

      iscore::FactoryBaseKey name() const final override {
          return ProcessInspectorWidgetDelegateFactory::staticFactoryKey();
      }

      void insert(std::unique_ptr<iscore::FactoryInterfaceBase> e) final override
      {
          if(auto pf = dynamic_unique_ptr_cast<ProcessInspectorWidgetDelegateFactory>(std::move(e)))
              m_list.emplace_back(std::move(pf));
      }

      const auto& list() const
      {
          return m_list;
      }

      auto make(
              const Process::ProcessModel& proc,
              const iscore::DocumentContext& ctx,
              QWidget* parent) const
      {
          auto it = find_if(m_list, [&] (const auto& elt) { return elt->matches(proc); });
          return (it != m_list.end())
                  ? (*it)->make(proc, ctx, parent)
                  : nullptr;
      }

    private:
      std::vector<std::unique_ptr<ProcessInspectorWidgetDelegateFactory>> m_list;
};

