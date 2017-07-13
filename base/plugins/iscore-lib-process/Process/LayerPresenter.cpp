// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LayerPresenter.hpp"
namespace Process
{
LayerPresenter::~LayerPresenter() = default;

bool LayerPresenter::focused() const
{
  return m_focus;
}

void LayerPresenter::setFocus(bool focus)
{
  m_focus = focus;
  on_focusChanged();
}

void LayerPresenter::on_focusChanged()
{
}

void LayerPresenter::setFullView()
{

}

void LayerPresenter::fillContextMenu(
    QMenu&, QPoint pos, QPointF scenepos, const LayerContextMenuManager&)
{
}
}
