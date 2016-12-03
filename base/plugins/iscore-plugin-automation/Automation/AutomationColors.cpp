#include "AutomationColors.hpp"
#include <iscore/model/Skin.hpp>

namespace Automation
{
Colors::Colors()
    : m_style{iscore::Skin::instance().Tender3,
              iscore::Skin::instance().Emphasis2,
              iscore::Skin::instance().Tender1,
              iscore::Skin::instance().Tender2, iscore::Skin::instance().Gray}
{
}
}
