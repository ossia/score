#include "Layout.hpp"

#include <Process/Dataflow/PortItem.hpp>

#include <score/graphics/GraphicsLayout.hpp>
#include <score/graphics/RectItem.hpp>
#include <score/graphics/TextItem.hpp>

namespace Process
{
QGraphicsItem* LayoutBuilderBase::makePort(Process::ControlInlet& port)
{
  if(auto* f = portFactory.get(port.concreteKey()))
  {
    return f->makeFullItem(port, this->doc, this->layout, &this->context);
  }
  return nullptr;
}

QGraphicsItem* LayoutBuilderBase::makePort(Process::ControlOutlet& port)
{
  if(auto* f = portFactory.get(port.concreteKey()))
  {
    return f->makeFullItem(port, this->doc, this->layout, &this->context);
  }
  return nullptr;
}

std::pair<Process::ControlInlet*, QGraphicsItem*>
LayoutBuilderBase::makeInlet(Process::Inlet* p)
{
  if(auto* port = qobject_cast<Process::ControlInlet*>(p))
  {
    return {port, makePort(*port)};
  }
  return {};
}

std::pair<Process::ControlOutlet*, QGraphicsItem*>
LayoutBuilderBase::makeOutlet(Process::Outlet* p)
{
  if(auto* port = qobject_cast<Process::ControlOutlet*>(p))
  {
    return {port, makePort(*port)};
  }
  return {};
}

std::vector<std::pair<Process::ControlInlet*, QGraphicsItem*>>
LayoutBuilderBase::makeInlets(std::span<Process::Inlet*> index)
{
  std::vector<std::pair<Process::ControlInlet*, QGraphicsItem*>> ret;
  for(auto p : index)
  {
    if(auto* port = qobject_cast<Process::ControlInlet*>(p))
    {
      ret.push_back({port, makePort(*port)});
    }
  }
  return ret;
}

std::vector<std::pair<Process::ControlOutlet*, QGraphicsItem*>>
LayoutBuilderBase::makeOutlets(std::span<Process::Outlet*> index)
{
  std::vector<std::pair<Process::ControlOutlet*, QGraphicsItem*>> ret;
  for(auto p : index)
  {
    if(auto* port = qobject_cast<Process::ControlOutlet*>(p))
    {
      ret.push_back({port, makePort(*port)});
    }
  }
  return ret;
}

QGraphicsItem* LayoutBuilderBase::makeLabel(std::string_view item)
{
  const auto& brush = Process::labelBrush().main;
  auto lab = new score::SimpleTextItem{brush, nullptr};
  lab->setText(QString::fromUtf8(item.data()));
  return lab;
}

void LayoutBuilderBase::finalizeLayout(QGraphicsItem* rootItem)
{
  using namespace score;
  if(createdLayouts.empty())
    return;

  auto w = createdLayouts.front();
  w->setParentItem(rootItem);

  for(auto it = createdLayouts.rbegin(); it != createdLayouts.rend(); ++it)
  {
    score::GraphicsLayout& lay = **it;
    lay.layout();

    // Update the size of the layout if it was not set manually
    if(lay.boundingRect().size() == QSize{0, 0})
    {
      //lay.setRect(lay.childrenBoundingRect().adjusted(-default_margin, -default_margin, default_margin, default_margin));

      const auto& cld = lay.childrenBoundingRect();
      lay.setRect(
          QRectF{0., 0., cld.right() + default_margin, cld.bottom() + default_margin});
    }
  }

  for(auto it = createdLayouts.rbegin(); it != createdLayouts.rend(); ++it)
  {
    score::GraphicsLayout& lay = **it;
    // Center the widgets if necessary
    lay.centerContent();
  }

  createdLayouts.front()->setPos(0, 0);
}

}
