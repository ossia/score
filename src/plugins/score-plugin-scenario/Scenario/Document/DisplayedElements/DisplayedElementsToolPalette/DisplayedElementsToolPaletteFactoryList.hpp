#pragma once
#include <score/plugins/InterfaceList.hpp>

#include <ossia/detail/algorithms.hpp>

#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactory.hpp>

namespace Scenario
{
class DisplayedElementsToolPaletteFactoryList final
    : public score::MatchingFactory<DisplayedElementsToolPaletteFactory>
{
};
}
