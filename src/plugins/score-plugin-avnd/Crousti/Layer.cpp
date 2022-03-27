#include "Layout.hpp"
#include <score/graphics/GraphicsLayout.hpp>

#include <Process/Dataflow/PortItem.hpp>

#include <score/graphics/RectItem.hpp>
#include <score/graphics/TextItem.hpp>

namespace oscr
{
#if 0
QGraphicsItem* LayoutBuilderBase::makePortImpl(
    Process::ControlInlet& portModel,
    QGraphicsItem& parent)
{
  Process::PortFactory* fact = portFactory.get(portModel.concreteKey());
  return fact->makeFullItem(portModel, this->doc, )

      /*

      using namespace score;
  auto item = new score::EmptyRectItem{&parent};

  // Port
  auto port = fact->makePortItem(portModel, doc, item, &context);
  port->setPos(0 + default_padding, 2. + default_padding);

  // Text
  const auto& brush = Process::portBrush(portModel.type()).main;
  auto lab = new score::SimpleTextItem{brush, item};
  lab->setText(portModel.visualName());
  lab->setPos(12 + default_padding, 0 + default_padding);

  // Control
  auto widg = fact->makeControlItem(portModel, doc, item, &context);
  //auto widg = ctrl.make_item(ctrl, inlet, doc, nullptr, &context);
  widg->setParentItem(item);
  widg->setPos(0 + default_padding, 12. + default_padding);

  item->setRect(item->childrenBoundingRect().adjusted(-default_margin, -default_margin, default_margin, default_margin));

  return item;
      */
}

QGraphicsItem* LayoutBuilderBase::makePortImpl(
    Process::ControlOutlet& portModel,
    QGraphicsItem& parent)
{
  using namespace score;
  auto item = new score::EmptyRectItem{&parent};

  // Port
  Process::PortFactory* fact = portFactory.get(portModel.concreteKey());
  auto port = fact->makePortItem(portModel, doc, item, &context);
  port->setPos(0 + default_padding, 2. + default_padding);

  // Text
  const auto& brush = Process::portBrush(portModel.type()).main;
  auto lab = new score::SimpleTextItem{brush, item};
  lab->setText(portModel.visualName());
  lab->setPos(12 + default_padding, 0 + default_padding);

  // Control
  auto widg = fact->makeControlItem(portModel, doc, item, &context);
  //auto widg = ctrl.make_item(ctrl, inlet, doc, nullptr, &context);
  widg->setParentItem(item);
  widg->setPos(0 + default_padding, 12. + default_padding);

  item->setRect(item->childrenBoundingRect().adjusted(-default_margin, -default_margin, default_margin, default_margin));

  return item;
}
#endif

QGraphicsItem* LayoutBuilderBase::makeInlet(int index)
{
  if(auto* port = qobject_cast<Process::ControlInlet*>(inlets[index]))
  {
    if(auto* f = portFactory.get(port->concreteKey()))
    {
      return f->makeFullItem(*port, this->doc, this->layout, &this->context);
    }
  }
  return nullptr;
}

QGraphicsItem* LayoutBuilderBase::makeOutlet(int index)
{
  if(auto* port = qobject_cast<Process::ControlOutlet*>(outlets[index]))
  {
    if(auto* f = portFactory.get(port->concreteKey()))
    {
      return f->makeFullItem(*port, this->doc, this->layout, &this->context);
    }
  }
  return nullptr;
}

QGraphicsItem* LayoutBuilderBase::makeLabel(std::string_view item)
{
  const auto& brush = Process::portBrush(Process::PortType::Message).main;
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

    // Update the size of the layout
    lay.setRect(lay.childrenBoundingRect().adjusted(-default_margin, -default_margin, default_margin, default_margin));
  }

  for(auto it = createdLayouts.rbegin(); it != createdLayouts.rend(); ++it)
  {
    score::GraphicsLayout& lay = **it;
    // Center the widgets if necessary
    lay.centerContent();
  }
}

}
