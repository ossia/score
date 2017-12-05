// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <QFlags>
#include <QFont>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QRect>
#include <score/model/Skin.hpp>
#include <qnamespace.h>

#include "MetronomeView.hpp"
#include <Process/LayerView.hpp>

namespace Metronome
{
LayerView::LayerView(QGraphicsItem* parent) : Process::LayerView{parent}
{
  setZValue(1);
  setFlags(ItemClipsToShape | ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable);
  setAcceptDrops(true);
}

LayerView::~LayerView()
{
}

void LayerView::paint_impl(QPainter* painter) const
{
}

void LayerView::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  if (event->mimeData())
    emit dropReceived(*event->mimeData());
}

QPixmap LayerView::pixmap()
{
  if (m_curveView)
    return m_curveView->pixmap();
  else
    return QPixmap();
}
}
