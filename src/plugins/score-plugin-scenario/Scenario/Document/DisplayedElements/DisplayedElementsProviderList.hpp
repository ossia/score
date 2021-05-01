#pragma once
#include <score/plugins/InterfaceList.hpp>

#include <ossia/detail/algorithms.hpp>

#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>

namespace Scenario
{
class DisplayedElementsProviderList final
    : public score::MatchingFactory<DisplayedElementsProvider>
{
};
}
