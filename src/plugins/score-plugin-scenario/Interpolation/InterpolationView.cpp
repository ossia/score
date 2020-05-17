// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "InterpolationView.hpp"

#include <Process/Style/ScenarioStyle.hpp>

#include <score/model/Skin.hpp>

#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Interpolation::View)
namespace Interpolation
{
View::View(QGraphicsItem* parent) : Process::LayerView{parent}
{
  setZValue(1);
  setFlags(ItemClipsToShape | ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable);
  setAcceptDrops(true);
}

View::~View() { }

void View::paint_impl(QPainter* painter) const { }

void View::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  dropReceived(event->pos(), *event->mimeData());
}

QPixmap View::pixmap() noexcept
{
  if (m_curveView)
    return m_curveView->pixmap();
  else
    return QPixmap();
}
}
