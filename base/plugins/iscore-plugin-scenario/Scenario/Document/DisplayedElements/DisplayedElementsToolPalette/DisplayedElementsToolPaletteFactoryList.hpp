#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactory.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/tools/std/Algorithms.hpp>


namespace Scenario
{
class DisplayedElementsToolPaletteFactoryList final : public iscore::FactoryListInterface
{
    public:

      static const iscore::AbstractFactoryKey& static_abstractFactoryKey() {
          return DisplayedElementsToolPaletteFactory::static_abstractFactoryKey();
      }

      iscore::AbstractFactoryKey name() const final override {
          return DisplayedElementsToolPaletteFactory::static_abstractFactoryKey();
      }

      void insert(std::unique_ptr<iscore::FactoryInterfaceBase> e) final override
      {
          if(auto pf = dynamic_unique_ptr_cast<DisplayedElementsToolPaletteFactory>(std::move(e)))
              m_list.emplace_back(std::move(pf));
      }

      const auto& list() const
      {
          return m_list;
      }

      auto make(
              ScenarioDocumentPresenter& pres,
              const ConstraintModel& constraint) const
      {
          auto it = find_if(m_list, [&] (const auto& elt) { return elt->matches(constraint); });
          return (it != m_list.end())
                  ? (*it)->make(pres, constraint)
                  : nullptr;
      }

    private:
      std::vector<std::unique_ptr<DisplayedElementsToolPaletteFactory>> m_list;
};
}
