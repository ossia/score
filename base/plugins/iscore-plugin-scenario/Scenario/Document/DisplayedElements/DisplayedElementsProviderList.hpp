#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <ossia/detail/algorithms.hpp>


namespace Scenario
{
class DisplayedElementsProviderList final :
        public iscore::MatchingFactory<DisplayedElementsProvider>
{
};
}
