// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MetronomeColors.hpp"

#include <score/model/Skin.hpp>

namespace Metronome
{
Colors::Colors(const score::Skin& s)
    : m_style{s.Emphasis4, s.Base2, s.Emphasis1, s.Base1, s.Emphasis4}
{
  m_style.init(s);
}
}
