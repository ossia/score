#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactory.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/tools/std/Algorithms.hpp>


namespace Scenario
{
class DisplayedElementsToolPaletteFactoryList final :
        public iscore::ConcreteFactoryList<DisplayedElementsToolPaletteFactory>
{
    public:
      auto make(
              ScenarioDocumentPresenter& pres,
              const ConstraintModel& constraint) const
      {
          auto it = find_if(*this, [&] (const auto& elt) { return elt.matches(constraint); });
          return (it != end())
                  ? it->make(pres, constraint)
                  : nullptr;
      }
};
}
