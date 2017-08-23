#include "NodeItem.hpp"

#include <Dataflow/Commands/EditConnection.hpp>

#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

namespace Dataflow
{
const constexpr double portRadius = 3.;
const constexpr double portUIRadius = portRadius + 1.;
const constexpr double portSize = 4.;
PortItem* hoveredPort{};
CableItem* createdCable{};
PortItem::PortItem(const iscore::DocumentContext& ctx, Type t, std::size_t idx,  NodeItem& node)
  : QQuickPaintedItem{&node}
  , m_ctx{ctx}
  , m_node{node}
  , m_type{t}
  , m_index{idx}
{
  this->setFlag(QQuickItem::ItemHasContents, true);
  this->setFlag(QQuickItem::ItemAcceptsDrops, true);
  this->setAntialiasing(true);
  //this->setAcceptHoverEvents(true);
  this->setAcceptedMouseButtons(Qt::LeftButton);
  this->setWidth(8);
  this->setHeight(8);
}

void PortItem::paint(QPainter* painter)
{
  if(m_glow)
  {
    painter->setPen(QPen(QColor::fromHsl(100, 150, 210), 2.5));
    painter->setBrush(QBrush(Qt::yellow));
  }
  else
  {
    painter->setPen(QPen(QColor::fromHsl(0, 0, 60), 2.));
    painter->setBrush(QBrush(Qt::gray));
  }
  painter->drawEllipse(boundingRect().center(), portRadius, portRadius);
}

void PortItem::mousePressEvent(QMouseEvent* ev)
{
  auto cable = new Process::Cable{Id<Process::Cable>{}};

  if(m_type == Type::Inlet)
  {
    cable->setInlet(m_index);
    cable->setSink(&m_node.node);
  }
  else if(m_type == Type::Outlet)
  {
    cable->setOutlet(m_index);
    cable->setSource(&m_node.node);
  }
  else if(m_type == Type::DependencyInlet)
  {
    cable->setSink(&m_node.node);
  }
  else if(m_type == Type::DependencyOutlet)
  {
    cable->setSource(&m_node.node);
  }

  createdCable = new CableItem{*cable};
  createdCable->setParentItem(this->parentItem()->parentItem());
  if(m_type == Type::Inlet)
  {
    createdCable->sink = &m_node;
    createdCable->setPosition(
          parentItem()->
          parentItem()->
          mapFromItem(parentItem(), m_node.inletPosition(m_index)));
  }
  else
  {
    createdCable->source = &m_node;
    createdCable->setPosition(
          parentItem()->
          parentItem()->
          mapFromItem(parentItem(), m_node.outletPosition(m_index)));
  }

  createdCable->press(ev->globalPos());
  ev->accept();
}

void PortItem::mouseMoveEvent(QMouseEvent* ev)
{
  createdCable->move(ev->globalPos());
  ev->accept();
}

void PortItem::mouseReleaseEvent(QMouseEvent* ev)
{
  auto item = createdCable->release(ev->globalPos());
  ev->accept();

  if(item && item != this)
  {
    ISCORE_ASSERT(&item->m_node.node != &this->m_node.node);
    auto cd = createdCable->m_cable.toCableData();
    switch(item->m_type)
    {
      case Type::Inlet:
        cd.inlet = item->m_index;
        cd.sink = item->m_node.node;
        break;
      case Type::Outlet:
        cd.outlet = item->m_index;
        cd.source = item->m_node.node;
        break;
      case Type::DependencyInlet:
        cd.sink = item->m_node.node;
        break;
      case Type::DependencyOutlet:
        cd.source = item->m_node.node;
        break;
    }
    if(cd.sink.valid() && cd.source.valid())
    {
      ISCORE_ASSERT(cd.sink != cd.source);

      auto& plug = m_ctx.model<Scenario::ScenarioDocumentModel>();
      CommandDispatcher<> disp{m_ctx.commandStack};
      disp.submitCommand<CreateCable>(
            plug,
            getStrongId(plug.cables),
            cd);
    }
  }

  delete createdCable;
  createdCable = nullptr;
}

void PortItem::hoverEnterEvent(QHoverEvent* ev)
{
  QQuickPaintedItem::hoverEnterEvent(ev);
  ev->accept();
}

void PortItem::hoverMoveEvent(QHoverEvent* ev)
{
  QQuickPaintedItem::hoverMoveEvent(ev);
  ev->accept();
}

void PortItem::hoverLeaveEvent(QHoverEvent* ev)
{
  QQuickPaintedItem::hoverLeaveEvent(ev);
  ev->accept();
}

void PortItem::dragEnterEvent(QDragEnterEvent* ev)
{
  QQuickPaintedItem::dragEnterEvent(ev);
  ev->accept();
}

void PortItem::dragLeaveEvent(QDragLeaveEvent* ev)
{
  QQuickPaintedItem::dragLeaveEvent(ev);
  ev->accept();
}

void PortItem::dragMoveEvent(QDragMoveEvent* ev)
{
  QQuickPaintedItem::dragMoveEvent(ev);
  ev->accept();
}

void PortItem::mouseDoubleClickEvent(QMouseEvent* ev)
{
  QQuickPaintedItem::mouseDoubleClickEvent(ev);
  ev->accept();
}

void PortItem::setGlow(bool b)
{
  m_glow = b;
  update();
}

NodeItem::NodeItem(const iscore::DocumentContext& ctx, Process::Node& p)
  : node{p}
  , m_ctx{ctx}
  , m_depIn{ctx, PortItem::DependencyInlet, 0, *this}
  , m_depOut{ctx, PortItem::DependencyOutlet, 0, *this}
{
  this->setFlag(QQuickItem::ItemHasContents, true);
  QObject::connect(&node, &Process::Node::inletsChanged,
                   this, &NodeItem::recreate);
  QObject::connect(&node, &Process::Node::outletsChanged,
                   this, &NodeItem::recreate);
  QObject::connect(&node.metadata(), &iscore::ModelMetadata::NameChanged,
                   this, &NodeItem::recreate);

  this->setAntialiasing(true);
  this->setAcceptedMouseButtons(Qt::LeftButton);
  recreate();
}

NodeItem::~NodeItem()
{
  aboutToDelete();
}

void NodeItem::recreate()
{
  const auto& procName = node.getText();
  std::size_t textwidth = std::max(30, QFontMetrics(QFont()).boundingRect(procName).width() + 24);
  std::size_t maxport = std::max(node.inlets().size(), node.outlets().size());
  setWidth(std::max(textwidth, 20 * maxport));
  setHeight(30);

  qDeleteAll(m_inlets);
  qDeleteAll(m_outlets);
  m_inlets.clear();
  m_outlets.clear();
  m_depIn.setPosition(depInlet());
  m_depOut.setPosition(depOutlet());
  for(std::size_t i = 0; i < node.inlets().size(); i++)
  {
    auto item = new PortItem{m_ctx, PortItem::Inlet, i, *this};
    item->setPosition(inletPosition(i));
    m_inlets.push_back(item);
  }
  for(std::size_t i = 0; i < node.outlets().size(); i++)
  {
    auto item = new PortItem{m_ctx, PortItem::Outlet, i, *this};
    item->setPosition(outletPosition(i));
    m_outlets.push_back(item);
  }
  update();
}

void NodeItem::paint(QPainter* painter)
{
  painter->setPen(QPen(Qt::darkGray, 2));
  painter->setBrush(QBrush(QColor::fromHsl(0, 0, 240)));
  auto rect = QRectF{xMv(), yMv(), objectW(), objectH()};
  painter->drawRoundedRect(rect, portSize, portSize);
  auto txt = node.getText();

  painter->drawText(rect, txt, QTextOption(Qt::AlignCenter));
}

QPointF NodeItem::depInlet() const
{ return QPointF{xMv() - portUIRadius, height()/2. - portUIRadius}; }

QPointF NodeItem::depOutlet() const
{ return QPointF{xMv() + objectW() - portUIRadius, height()/2. - portUIRadius}; }

QPointF NodeItem::inletPosition(int i) const
{
  const double spc = objectW() / double(node.inlets().size());
  return QPointF{portUIRadius + xMv() + i * spc, yMv() - portUIRadius};
}

QPointF NodeItem::outletPosition(int i) const
{
  const double spc = objectW() / double(node.outlets().size());
  return QPointF{portUIRadius + xMv() + i * spc, yMv() + objectH() - portUIRadius};
}

void NodeItem::mousePressEvent(QMouseEvent* ev)
{
  this->grabMouse();
  m_clickPos = ev->screenPos();
  m_origPos = this->position();

  ev->accept();
}

void NodeItem::mouseMoveEvent(QMouseEvent* ev)
{
  setPosition(m_origPos + (ev->screenPos() - m_clickPos));
  ev->accept();
}

void NodeItem::mouseReleaseEvent(QMouseEvent* ev)
{
  QQuickPaintedItem::mouseReleaseEvent(ev);
  // TODO submit command
  ev->accept();
  this->ungrabMouse();
}

void NodeItem::hoverEnterEvent(QHoverEvent* ev)
{
  QQuickPaintedItem::hoverEnterEvent(ev);
  ev->accept();
}

void NodeItem::hoverMoveEvent(QHoverEvent* ev)
{
  QQuickPaintedItem::hoverMoveEvent(ev);
  ev->accept();
}

void NodeItem::hoverLeaveEvent(QHoverEvent* ev)
{
  QQuickPaintedItem::hoverLeaveEvent(ev);
  ev->accept();
}

void NodeItem::dragEnterEvent(QDragEnterEvent* ev)
{
  QQuickPaintedItem::dragEnterEvent(ev);
  ev->accept();
}

void NodeItem::dragLeaveEvent(QDragLeaveEvent* ev)
{
  QQuickPaintedItem::dragLeaveEvent(ev);
  ev->accept();
}

void NodeItem::dragMoveEvent(QDragMoveEvent* ev)
{
  QQuickPaintedItem::dragMoveEvent(ev);
  ev->accept();
}

void NodeItem::mouseDoubleClickEvent(QMouseEvent* ev)
{
  // Dialog to set addresses
  QQuickPaintedItem::mouseDoubleClickEvent(ev);

  // Also allow to drag addresses on ports
  ev->accept();
}




CableItem::CableItem(Process::Cable& p, NodeItem* src, NodeItem* snk):
  m_cable{p}
, source{src}
, sink{snk}
{
  this->setFlag(QQuickItem::ItemHasContents, true);
  connect(&m_cable, &Process::Cable::inletChanged,
          this, [=] { recreate(); });
  connect(&m_cable, &Process::Cable::outletChanged,
          this, [=] { recreate(); });
  connect(&m_cable, &Process::Cable::sourceChanged,
          this, [=] { recreate(); });
  connect(&m_cable, &Process::Cable::sinkChanged,
          this, [=] { recreate(); });

  this->setAntialiasing(true);
  recreate();
}

void CableItem::recreate()
{
  for(auto& con : cons)
    disconnect(con);
  cons.clear();

  if(source)
  {
    cons.push_back(connect(source, &NodeItem::xChanged, this, [=] { updateRect(); update(); }));
    cons.push_back(connect(source, &NodeItem::yChanged, this, [=] { updateRect(); update(); }));
    cons.push_back(connect(source, &NodeItem::widthChanged, this, [=] { updateRect(); update(); }));
    cons.push_back(connect(source, &NodeItem::heightChanged, this, [=] { updateRect(); update(); }));
    cons.push_back(connect(source, &NodeItem::aboutToDelete, this, [=] { source = nullptr; recreate(); }));
  }
  if(sink)
  {
    cons.push_back(connect(sink, &NodeItem::xChanged, this, [=] { updateRect(); update(); }));
    cons.push_back(connect(sink, &NodeItem::yChanged, this, [=] { updateRect(); update(); }));
    cons.push_back(connect(sink, &NodeItem::widthChanged, this, [=] { updateRect(); update(); }));
    cons.push_back(connect(sink, &NodeItem::heightChanged, this, [=] { updateRect(); update(); }));
    cons.push_back(connect(sink, &NodeItem::aboutToDelete, this, [=] { sink = nullptr; recreate(); }));
  }

  updateRect();
  update();
}

void CableItem::updateRect()
{
  QRectF rect;
  if(source && sink)
  {
    if(m_cable.inlet() && m_cable.outlet())
    {
      rect = QRectF{
          source->position() + source->outletPosition(*m_cable.outlet()),
          sink->position() + sink->inletPosition(*m_cable.inlet())};
    }
    else
    {
      rect = QRectF{
          source->position() + source->depOutlet(),
          sink->position() + sink->depInlet()};
    }
  }
  else if(source)
  {
    if(m_cable.outlet())
    {
      rect = QRectF{
          source->position() + source->outletPosition(*m_cable.outlet()),
          m_curPos};
    }
    else
    {
      rect = QRectF{
          source->position() + source->depOutlet(),
          m_curPos};
    }
  }
  else if(sink)
  {
    if(m_cable.inlet())
    {
      rect = QRectF{
          sink->position() + sink->inletPosition(*m_cable.inlet()),
          m_curPos};
    }
    else
    {
      rect = QRectF{
          sink->position() + sink->depInlet(),
          m_curPos};
    }
  }

  const double grow = 4.;
  auto norm = rect.normalized();
  auto line = QLineF{
              mapFromItem(parentItem(), rect.topLeft() + QPointF{grow, grow}),
              mapFromItem(parentItem(), rect.bottomRight() + QPointF{grow, grow})};
  if(line != m_line)
  {
    m_line = line;
    this->setPosition(norm.topLeft() - QPointF{grow, grow});
    this->setWidth(norm.width() + 3. * grow);
    this->setHeight(norm.height() + 3. * grow);
  }
}

void CableItem::paint(QPainter* painter)
{
  painter->setPen(QPen(Qt::darkGray, 2.));
  painter->drawLine(m_line);
  /*
  QPainterPath p;
  p.moveTo(m_line.x1(), m_line.y1());

  if(!m_cable.inlet() && !m_cable.outlet())
    p.cubicTo(m_line.x2(), m_line.y1(), m_line.x1(), m_line.y2(), m_line.x2(), m_line.y2());
  else
    p.cubicTo(m_line.x1(), m_line.y2(), m_line.x2(), m_line.y1(), m_line.x2(), m_line.y2());
  painter->drawPath(p);
  */
}

void CableItem::press(QPointF pos)
{
  m_clickPos = pos;
}

void CableItem::move(QPointF pos)
{
  PortItem* currentHovered{};
  if(QLineF(m_clickPos, pos).length() > 5)
  {
    m_curPos = parentItem()->mapFromGlobal(pos);

    this->setVisible(false);
    auto item = dynamic_cast<NodeItem*>(parentItem()->childAt(m_curPos.x(), m_curPos.y()));
    this->setVisible(true);
    if(item)
    {
      auto item_pos = item->mapFromGlobal(pos);
      auto port_item = item->childAt(item_pos.x(), item_pos.y());
      currentHovered = dynamic_cast<PortItem*>(port_item);
    }
  }
  else
  {
    m_curPos = parentItem()->mapFromGlobal(m_clickPos);
  }

  if(hoveredPort)
  {
    hoveredPort->setGlow(false);
    hoveredPort = nullptr;
  }
  if(currentHovered)
  {
    currentHovered->setGlow(true);
    hoveredPort = currentHovered;
  }

  updateRect();
}

PortItem* CableItem::release(QPointF pos)
{
  if(QLineF(m_clickPos, pos).length() > 5)
  {
    m_curPos = parentItem()->mapFromGlobal(pos);

    this->setVisible(false);
    auto item = dynamic_cast<NodeItem*>(parentItem()->childAt(m_curPos.x(), m_curPos.y()));
    this->setVisible(true);
    if(item)
    {
      auto item_pos = item->mapFromGlobal(pos);
      auto port_item = item->childAt(item_pos.x(), item_pos.y());
      hoveredPort = dynamic_cast<PortItem*>(port_item);
    }
  }
  else
  {
    m_curPos = parentItem()->mapFromGlobal(m_clickPos);
  }

  updateRect();

  if(hoveredPort)
  {
    auto p = hoveredPort;
    hoveredPort->setGlow(false);
    hoveredPort = nullptr;
    return p;
  }
  return nullptr;
}

void CableItem::hoverEnterEvent(QHoverEvent* ev)
{
  QQuickPaintedItem::hoverEnterEvent(ev);
  ev->accept();
}

void CableItem::hoverMoveEvent(QHoverEvent* ev)
{
  QQuickPaintedItem::hoverMoveEvent(ev);
  ev->accept();
}

void CableItem::hoverLeaveEvent(QHoverEvent* ev)
{
  QQuickPaintedItem::hoverLeaveEvent(ev);
  ev->accept();
}

void CableItem::dragEnterEvent(QDragEnterEvent* ev)
{
  QQuickPaintedItem::dragEnterEvent(ev);
  ev->accept();
}

void CableItem::dragLeaveEvent(QDragLeaveEvent* ev)
{
  QQuickPaintedItem::dragLeaveEvent(ev);
  ev->accept();
}

void CableItem::dragMoveEvent(QDragMoveEvent* ev)
{
  QQuickPaintedItem::dragMoveEvent(ev);
  ev->accept();
}

void CableItem::mouseDoubleClickEvent(QMouseEvent* ev)
{
  QQuickPaintedItem::mouseDoubleClickEvent(ev);
  ev->accept();
}

}
