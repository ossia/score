#pragma once
#include <ossia/detail/algorithms.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactory.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

namespace Scenario
{
class DisplayedElementsToolPaletteFactoryList final
    : public iscore::MatchingFactory<DisplayedElementsToolPaletteFactory>
{
};
}
