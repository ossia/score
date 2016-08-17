#include "MappingColors.hpp"
#include <Process/Style/Skin.hpp>

namespace Mapping
{
MappingColors::MappingColors():
    m_style{
        Skin::instance().Tender3,
        Skin::instance().Emphasis2,
        Skin::instance().Emphasis3,
        Skin::instance().Tender2,
        Skin::instance().Gray}
{
}
}
