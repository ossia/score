#include "MappingColors.hpp"
#include <iscore/model/Skin.hpp>

namespace Mapping
{
Colors::Colors(const iscore::Skin& s)
    : m_style{s.Emphasis3,
              s.Tender2,
              s.Emphasis3,
              s.Tender2, s.Gray}
{
  m_style.init(s);
}
}
