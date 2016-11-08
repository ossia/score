#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactory.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <ossia/detail/algorithms.hpp>

namespace Scenario
{
class DisplayedElementsToolPaletteFactoryList final :
        public iscore::MatchingFactory<DisplayedElementsToolPaletteFactory>
{
};
}
