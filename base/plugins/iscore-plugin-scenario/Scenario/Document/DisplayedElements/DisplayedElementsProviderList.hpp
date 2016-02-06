#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/tools/std/Algorithms.hpp>


namespace Scenario
{
class DisplayedElementsProviderList final :
        public iscore::ConcreteFactoryList<DisplayedElementsProvider>
{
    public:
      auto make(const ConstraintModel& cst) const
      {
          auto it = find_if(*this, [&] (const auto& elt) { return elt.matches(cst); });
          return (it != end())
                  ? it->make(cst)
                  : DisplayedElementsContainer{};
      }
};
}
