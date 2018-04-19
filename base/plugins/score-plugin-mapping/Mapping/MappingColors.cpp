// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MappingColors.hpp"

#include <score/model/Skin.hpp>

namespace Mapping
{
Colors::Colors(const score::Skin& s)
    : m_style{s.Emphasis3, s.Tender2, s.Emphasis3, s.Tender2, s.Gray}
{
  m_style.init(s);
}
}
