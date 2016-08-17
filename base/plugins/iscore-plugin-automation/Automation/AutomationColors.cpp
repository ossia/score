#include "AutomationColors.hpp"
#include <Process/Style/Skin.hpp>

namespace Automation
{
Colors::Colors():
    m_style{
        Skin::instance().Tender3,
        Skin::instance().Emphasis2,
        Skin::instance().Tender1,
        Skin::instance().Tender2,
        Skin::instance().Gray}
{
}
}
