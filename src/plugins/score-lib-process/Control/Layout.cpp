#include "Layout.hpp"

#include <Process/Dataflow/PortItem.hpp>

#include <score/graphics/GraphicsLayout.hpp>
#include <score/graphics/RectItem.hpp>
#include <score/graphics/TextItem.hpp>

namespace Process
{
Process::ControlLayout LayoutBuilderBase::makePort(
    Process::Inlet& port, const Process::ControlPresentation& presentation)
{
  if(auto* f = portFactory.get(port.concreteKey()))
  {
    if(auto control = qobject_cast<Process::ControlInlet*>(&port))
      return f->makeFullItem(
          *control, this->doc, this->layout, &this->context, presentation);
    else
      return f->makeLabelItem(port, this->doc, this->layout, &this->context);
  }
  return {};
}

Process::ControlLayout LayoutBuilderBase::makePort(
    Process::Outlet& port, const Process::ControlPresentation& presentation)
{
  if(auto* f = portFactory.get(port.concreteKey()))
  {
    if(auto control = qobject_cast<Process::ControlOutlet*>(&port))
      return f->makeFullItem(
          *control, this->doc, this->layout, &this->context, presentation);
    else
      return f->makeLabelItem(port, this->doc, this->layout, &this->context);
  }
  return {};
}

std::pair<Process::ControlInlet*, Process::ControlLayout>
LayoutBuilderBase::makeInlet(
    Process::Inlet* p, const Process::ControlPresentation& presentation)
{
  if(auto* port = qobject_cast<Process::ControlInlet*>(p))
  {
    return {port, makePort(*port, presentation)};
  }
  return {};
}

std::pair<Process::ControlOutlet*, Process::ControlLayout>
LayoutBuilderBase::makeOutlet(
    Process::Outlet* p, const Process::ControlPresentation& presentation)
{
  if(auto* port = qobject_cast<Process::ControlOutlet*>(p))
  {
    return {port, makePort(*port, presentation)};
  }
  return {};
}

std::vector<std::pair<Process::ControlInlet*, Process::ControlLayout>>
LayoutBuilderBase::makeInlets(std::span<Process::Inlet*> index)
{
  std::vector<std::pair<Process::ControlInlet*, Process::ControlLayout>> ret;
  for(auto p : index)
  {
    if(auto* port = qobject_cast<Process::ControlInlet*>(p))
    {
      ret.push_back({port, makePort(*port)});
    }
  }
  return ret;
}

std::vector<std::pair<Process::ControlOutlet*, Process::ControlLayout>>
LayoutBuilderBase::makeOutlets(std::span<Process::Outlet*> index)
{
  std::vector<std::pair<Process::ControlOutlet*, Process::ControlLayout>> ret;
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

  // The avendish builder's outermost layout is the root item itself
  auto w = createdLayouts.front();
  if(w != rootItem)
    w->setParentItem(rootItem);

  for(auto it = createdLayouts.rbegin(); it != createdLayouts.rend(); ++it)
  {
    score::GraphicsLayout& lay = **it;
    lay.layout();

    // Update the size of the layout if it was not set manually
    if(lay.boundingRect().size() == QSize{0, 0})
    {
      //lay.setRect(lay.childrenBoundingRect().adjusted(-default_margin, -default_margin, default_margin, default_margin));

      const bool outermost = (&lay == createdLayouts.front());
      const bool margins = outermost || marginOnNestedLayouts;
      const qreal margin = margins ? default_margin : 0.;
      // A nested box drawn with its own background keeps as much room on its
      // right as on its left, so that what fills its width does not reach the
      // next box
      const qreal right_margin
          = (margins || lay.hasBackground()) ? default_margin : 0.;
      const auto& cld = lay.childrenBoundingRect();
      lay.setRect(QRectF{0., 0., cld.right() + right_margin, cld.bottom() + margin});
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
