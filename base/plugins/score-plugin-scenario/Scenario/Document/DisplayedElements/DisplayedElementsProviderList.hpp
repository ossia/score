#pragma once
#include <ossia/detail/algorithms.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>
#include <score/plugins/customfactory/FactoryFamily.hpp>

namespace Scenario
{
class DisplayedElementsProviderList final
    : public score::MatchingFactory<DisplayedElementsProvider>
{
};
}
