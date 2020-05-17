// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "MetronomeView.hpp"

#include <Process/LayerView.hpp>

#include <score/model/Skin.hpp>

#include <QGraphicsItem>
#include <QPainter>
#include <qnamespace.h>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Metronome::LayerView)
namespace Metronome
{
LayerView::LayerView(QGraphicsItem* parent) : Process::LayerView{parent}
{
  setZValue(1);
  setFlags(ItemClipsToShape | ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable);
  setAcceptDrops(true);
}

LayerView::~LayerView() { }

void LayerView::paint_impl(QPainter* painter) const { }

void LayerView::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  if (event->mimeData())
    dropReceived(event->pos(), *event->mimeData());
}

QPixmap LayerView::pixmap() noexcept
{
  if (m_curveView)
    return m_curveView->pixmap();
  else
    return QPixmap();
}
}
