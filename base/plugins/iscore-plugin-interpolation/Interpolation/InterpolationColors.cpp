#include <Interpolation/InterpolationColors.hpp>
#include <iscore/model/Skin.hpp>

namespace Interpolation
{
Colors::Colors():
    m_style{
        iscore::Skin::instance().Emphasis4,
        iscore::Skin::instance().Smooth3,
        iscore::Skin::instance().Tender3,
        iscore::Skin::instance().Smooth3,
        iscore::Skin::instance().Gray}
{
}
}
