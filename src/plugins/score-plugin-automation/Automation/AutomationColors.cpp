// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AutomationColors.hpp"

#include <score/model/Skin.hpp>

namespace Automation
{
Colors::Colors(const score::Skin& s) : m_style{s.Tender1, s.Tender2, s.Tender1, s.Tender2, s.Gray}
{
  m_style.init(s);
}
}
