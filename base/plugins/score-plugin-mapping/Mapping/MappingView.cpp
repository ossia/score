// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <QFlags>
#include <QFont>
#include <QGraphicsItem>
#include <QPainter>
#include <QRect>
#include <qnamespace.h>

#include "MappingView.hpp"
#include <Process/LayerView.hpp>
#include <Process/Style/ScenarioStyle.hpp>

namespace Mapping
{
LayerView::LayerView(QGraphicsItem* parent) : Process::LayerView{parent}
{
  setZValue(1);
  this->setFlags(ItemClipsToShape |
      ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable);
}

void LayerView::paint_impl(QPainter* painter) const
{
}

QPixmap LayerView::pixmap()
{
  if (m_curveView)
    return m_curveView->pixmap();
  else
    return QPixmap();
}
}
