#include "View.hpp"

#include <Process/Style/ScenarioStyle.hpp>

#include <QPainter>

namespace Gfx::Mesh
{

View::View(QGraphicsItem* parent) : LayerView{parent} {}

View::~View() {}

void View::paint_impl(QPainter* painter) const {}
}
