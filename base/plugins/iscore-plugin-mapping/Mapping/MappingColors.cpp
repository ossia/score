#include "MappingColors.hpp"
#include <iscore/model/Skin.hpp>

namespace Mapping
{
MappingColors::MappingColors():
    m_style{
        iscore::Skin::instance().Tender3,
        iscore::Skin::instance().Emphasis2,
        iscore::Skin::instance().Emphasis3,
        iscore::Skin::instance().Tender2,
        iscore::Skin::instance().Gray}
{
}
}
