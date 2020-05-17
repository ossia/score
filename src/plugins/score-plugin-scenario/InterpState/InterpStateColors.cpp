// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <score/model/Skin.hpp>

#include <InterpState/InterpStateColors.hpp>

namespace InterpState
{
Colors::Colors(const score::Skin& s) : m_style{s.Smooth3, s.Tender3, s.Smooth3, s.Tender3, s.Gray}
{
  m_style.init(s);
}
}
