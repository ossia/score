#include "LayerContextMenu.hpp"

namespace Process
{

LayerContextMenu::LayerContextMenu(StringKey<LayerContextMenu> k)
    : m_key{std::move(k)}
{
}

void LayerContextMenu::build(
    QMenu& m, QPoint pt, QPointF ptf, const LayerContext& proc) const
{
  for (auto& fun : functions)
  {
    ISCORE_ASSERT(bool(fun));
    fun(m, pt, ptf, proc);
  }
}
}
