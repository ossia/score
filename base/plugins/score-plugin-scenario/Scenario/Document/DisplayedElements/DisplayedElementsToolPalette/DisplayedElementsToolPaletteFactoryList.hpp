#pragma once
#include <ossia/detail/algorithms.hpp>

#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactory.hpp>
#include <score/plugins/customfactory/FactoryFamily.hpp>

namespace Scenario
{
class DisplayedElementsToolPaletteFactoryList final
    : public score::MatchingFactory<DisplayedElementsToolPaletteFactory>
{
};
}
