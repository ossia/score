#include <Interpolation/Colors.hpp>
#include <Process/Style/Skin.hpp>

namespace Interpolation
{
Colors::Colors():
    m_style{
        Skin::instance().Smooth2,
        Skin::instance().Emphasis2,
        Skin::instance().Smooth1,
        Skin::instance().Tender3,
        Skin::instance().Gray}
{
}
}
