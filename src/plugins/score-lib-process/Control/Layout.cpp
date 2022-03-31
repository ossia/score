#include "Layout.hpp"
#include <score/graphics/GraphicsLayout.hpp>

#include <Process/Dataflow/PortItem.hpp>

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

QGraphicsItem* LayoutBuilderBase::makeInlet(int index)
{
  if(auto* port = qobject_cast<Process::ControlInlet*>(inlets[index]))
  {
    return makePort(*port);
  }
  return nullptr;
}

QGraphicsItem* LayoutBuilderBase::makeOutlet(int index)
{
  if(auto* port = qobject_cast<Process::ControlOutlet*>(outlets[index]))
  {
    return makePort(*port);
  }
  return nullptr;
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
      lay.setRect(QRectF{0., 0., cld.right() + default_margin, cld.bottom() + default_margin});
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
