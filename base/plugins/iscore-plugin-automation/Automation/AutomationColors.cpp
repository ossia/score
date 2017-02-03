#include "AutomationColors.hpp"
#include <iscore/model/Skin.hpp>

namespace Automation
{
Colors::Colors(const iscore::Skin& s)
  : m_style{
      s.Tender3,
      s.Emphasis2,
      s.Tender1,
      s.Tender2,
      s.Gray}
{
  m_style.init(s);
}
}
