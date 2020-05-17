// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LayerContextMenu.hpp"

namespace Process
{

LayerContextMenu::LayerContextMenu(StringKey<LayerContextMenu> k) : m_key{std::move(k)} { }

void LayerContextMenu::build(QMenu& m, QPoint pt, QPointF ptf, const LayerContext& proc) const
{
  for (auto& fun : functions)
  {
    SCORE_ASSERT(bool(fun));
    fun(m, pt, ptf, proc);
  }
}
}
