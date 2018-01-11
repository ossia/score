// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "InterpolationView.hpp"
#include <QGraphicsSceneMouseEvent>
#include <score/model/Skin.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <QPainter>
namespace Interpolation
{
View::View(QGraphicsItem* parent) : Process::LayerView{parent}
{
  setZValue(1);
  setFlags(ItemClipsToShape |
           ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable);
  setAcceptDrops(true);
}

View::~View()
{
}

void View::paint_impl(QPainter* painter) const
{
}

void View::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  emit dropReceived(event->pos(), *event->mimeData());
}
}
