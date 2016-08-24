#include <Interpolation/InterpolationColors.hpp>
#include <Process/Style/Skin.hpp>

namespace Interpolation
{
Colors::Colors():
    m_style{
        Skin::instance().Emphasis4,
        Skin::instance().Smooth3,
        Skin::instance().Tender3,
        Skin::instance().Smooth3,
        Skin::instance().Gray}
{
}
}
