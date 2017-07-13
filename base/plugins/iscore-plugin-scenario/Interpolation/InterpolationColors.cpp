// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Interpolation/InterpolationColors.hpp>
#include <iscore/model/Skin.hpp>

namespace Interpolation
{
Colors::Colors(const iscore::Skin& s)
    : m_style{s.Tender3,
              s.Smooth3,
              s.Tender3,
              s.Smooth3, s.Gray}
{
  m_style.init(s);
}
}
