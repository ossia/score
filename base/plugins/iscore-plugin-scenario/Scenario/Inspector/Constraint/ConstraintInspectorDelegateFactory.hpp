#pragma once
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegate.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/tools/std/StdlibWrapper.hpp>
#include <memory>
#include <vector>

#include "iscore/plugins/customfactory/FactoryInterface.hpp"

class ConstraintModel;

class ConstraintInspectorDelegateFactory : public iscore::FactoryInterfaceBase
{
         ISCORE_FACTORY_DECL("ConstraintInspectorDelegate")
    public:
        virtual ~ConstraintInspectorDelegateFactory();
        virtual std::unique_ptr<ConstraintInspectorDelegate> make(const ConstraintModel& constraint) = 0;
        virtual bool matches(const ConstraintModel& constraint) const = 0;
};

class ConstraintInspectorDelegateFactoryList final : public iscore::FactoryListInterface
{
    public:
      static const iscore::FactoryBaseKey& staticFactoryKey() {
          return ConstraintInspectorDelegateFactory::staticFactoryKey();
      }

      iscore::FactoryBaseKey name() const final override {
          return ConstraintInspectorDelegateFactory::staticFactoryKey();
      }

      void insert(iscore::FactoryInterfaceBase* e) final override
      {
          if(auto pf = dynamic_cast<ConstraintInspectorDelegateFactory*>(e))
              m_list.push_back(pf);
      }

      const auto& list() const
      {
          return m_list;
      }

      auto make(const ConstraintModel& constraint) const
      {
          auto it = find_if(m_list, [&] (auto elt) { return elt->matches(constraint); });
          return (it != m_list.end())
                  ? (*it)->make(constraint)
                  : nullptr;
      }

    private:
      std::vector<ConstraintInspectorDelegateFactory*> m_list;
};
