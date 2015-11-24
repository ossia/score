#pragma once
#include <memory>

#include <Scenario/Document/BaseElement/FullViewStateMachines/FullViewStateMachine.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/tools/std/StdlibWrapper.hpp>

class ConstraintModel;
class ScenarioToolPaletteFactory : public iscore::FactoryInterfaceBase
{
         ISCORE_FACTORY_DECL("ScenarioToolPalette")
    public:
        virtual ~ScenarioToolPaletteFactory();

         virtual bool matches(
                 const ConstraintModel& constraint) const = 0;

        virtual std::unique_ptr<GraphicsSceneToolPalette> make(
                 BaseElementPresenter& pres,
                 const ConstraintModel& constraint) = 0;
};

class ScenarioToolPaletteFactoryList final : public iscore::FactoryListInterface
{
    public:
      static const iscore::FactoryBaseKey& staticFactoryKey() {
          return ScenarioToolPaletteFactory::staticFactoryKey();
      }

      iscore::FactoryBaseKey name() const final override {
          return ScenarioToolPaletteFactory::staticFactoryKey();
      }

      void insert(iscore::FactoryInterfaceBase* e) final override
      {
          if(auto pf = dynamic_cast<ScenarioToolPaletteFactory*>(e))
              m_list.push_back(pf);
      }

      const auto& list() const
      {
          return m_list;
      }

      auto make(
              BaseElementPresenter& pres,
              const ConstraintModel& constraint) const
      {
          auto it = find_if(m_list, [&] (auto elt) { return elt->matches(constraint); });
          return (it != m_list.end())
                  ? (*it)->make(pres, constraint)
                  : nullptr;
      }

    private:
      std::vector<ScenarioToolPaletteFactory*> m_list;
};
