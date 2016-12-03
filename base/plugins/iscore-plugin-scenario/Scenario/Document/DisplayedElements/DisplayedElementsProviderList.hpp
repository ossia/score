#pragma once
#include <ossia/detail/algorithms.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

namespace Scenario
{
class DisplayedElementsProviderList final
    : public iscore::MatchingFactory<DisplayedElementsProvider>
{
};
}
