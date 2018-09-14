#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>

#include <score/plugins/customfactory/FactoryFamily.hpp>

#include <ossia/detail/algorithms.hpp>

namespace Scenario
{
class DisplayedElementsProviderList final
    : public score::MatchingFactory<DisplayedElementsProvider>
{
};
}
