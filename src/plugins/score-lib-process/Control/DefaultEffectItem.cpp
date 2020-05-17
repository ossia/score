#include "DefaultEffectItem.hpp"

#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <score/graphics/TextItem.hpp>
#include <score/tools/Bind.hpp>

#include <QGraphicsScene>

#include <Control/Widgets.hpp>
#include <Effect/EffectLayout.hpp>
namespace Process
{
struct DefaultEffectItem::Port
{
  score::EmptyRectItem* root;
  Dataflow::PortItem* port;
  QRectF rect;
};
DefaultEffectItem::DefaultEffectItem(
    const Process::ProcessModel& effect,
    const Process::Context& doc,
    QGraphicsItem* root)
    : score::EmptyRectItem{root}, m_effect{effect}, m_ctx{doc}
{
  QObject::connect(
      &effect, &Process::ProcessModel::controlAdded, this, &DefaultEffectItem::on_controlAdded);

  QObject::connect(
      &effect,
      &Process::ProcessModel::controlRemoved,
      this,
      &DefaultEffectItem::on_controlRemoved);

  QObject::connect(
      &effect,
      &Process::ProcessModel::controlOutletAdded,
      this,
      &DefaultEffectItem::on_controlOutletAdded);

  QObject::connect(
      &effect,
      &Process::ProcessModel::controlOutletRemoved,
      this,
      &DefaultEffectItem::on_controlOutletRemoved);

  auto& portFactory = doc.app.interfaces<Process::PortFactoryList>();
  for (auto& e : effect.inlets())
  {
    auto inlet = qobject_cast<Process::ControlInlet*>(e);
    if (!inlet)
      continue;

    setupInlet(*inlet, portFactory);
  }
  for (auto& e : effect.outlets())
  {
    auto outlet = qobject_cast<Process::ControlOutlet*>(e);
    if (!outlet)
      continue;

    setupOutlet(*outlet, portFactory);
  }
  updateRect();

  QObject::connect(
      &effect, &Process::ProcessModel::inletsChanged, this, &DefaultEffectItem::reset);
  QObject::connect(
      &effect, &Process::ProcessModel::outletsChanged, this, &DefaultEffectItem::reset);
}

DefaultEffectItem::~DefaultEffectItem() { }

template <typename T>
void DefaultEffectItem::setupPort(T& port, const Process::PortFactoryList& portFactory)
{
  int i = m_ports.size();

  auto csetup = Process::controlSetup(
      [](auto& factory, auto& inlet, const auto& doc, auto item, auto parent) {
        return factory.makeItem(inlet, doc, item, parent);
      },
      [](auto& factory, auto& inlet, const auto& doc, auto item, auto parent) {
        return factory.makeControlItem(inlet, doc, item, parent);
      },
      [&](int j) { return m_ports[j].rect.size(); },
      [&] { return port.customData(); });
  auto [item, portItem, widg, lab, itemRect]
      = Process::createControl(i, csetup, port, portFactory, m_ctx, this, this);
  m_ports.push_back(Port{item, portItem, itemRect});
  updateRect();
}

void DefaultEffectItem::setupInlet(
    Process::ControlInlet& inlet,
    const Process::PortFactoryList& portFactory)
{
  setupPort(inlet, portFactory);
  con(inlet, &Process::ControlInlet::domainChanged, this, [this, &inlet] {
    on_controlRemoved(inlet);
    on_controlAdded(inlet.id());
  });
}

void DefaultEffectItem::setupOutlet(
    Process::ControlOutlet& outlet,
    const Process::PortFactoryList& portFactory)
{
  setupPort(outlet, portFactory);
  con(outlet, &Process::ControlOutlet::domainChanged, this, [this, &outlet] {
    on_controlOutletRemoved(outlet);
    on_controlOutletAdded(outlet.id());
  });
}

void DefaultEffectItem::on_controlAdded(const Id<Process::Port>& id)
{
  auto& portFactory = m_ctx.app.interfaces<Process::PortFactoryList>();
  auto inlet = safe_cast<Process::ControlInlet*>(m_effect.inlet(id));
  setupInlet(*inlet, portFactory);
}

void DefaultEffectItem::on_controlRemoved(const Process::Port& port)
{
  for (auto it = m_ports.begin(); it != m_ports.end(); ++it)
  {
    auto ptr = it->port;
    if (&ptr->port() == &port)
    {
      auto parent_item = it->root;
      auto h = parent_item->boundingRect().height();
      delete parent_item;
      m_ports.erase(it);

      for (; it != m_ports.end(); ++it)
      {
        auto item = it->root;
        item->moveBy(0., -h);
      }
      updateRect();
      return;
    }
  }
}

void DefaultEffectItem::on_controlOutletAdded(const Id<Process::Port>& id)
{
  auto& portFactory = m_ctx.app.interfaces<Process::PortFactoryList>();
  auto outlet = safe_cast<Process::ControlOutlet*>(m_effect.outlet(id));
  setupOutlet(*outlet, portFactory);
}

void DefaultEffectItem::on_controlOutletRemoved(const Process::Port& port)
{
  for (auto it = m_ports.begin(); it != m_ports.end(); ++it)
  {
    auto ptr = it->port;
    if (&ptr->port() == &port)
    {
      auto parent_item = it->root;
      auto h = parent_item->boundingRect().height();
      delete parent_item;
      m_ports.erase(it);

      for (; it != m_ports.end(); ++it)
      {
        auto item = it->root;
        item->moveBy(0., -h);
      }
      updateRect();
      return;
    }
  }
}

void DefaultEffectItem::reset()
{
  const auto& c = this->childItems();
  for (auto child : c)
  {
    this->scene()->removeItem(child);
    delete child;
  }
  updateRect();
  m_ports.clear();

  auto& portFactory = m_ctx.app.interfaces<Process::PortFactoryList>();
  for (auto& e : m_effect.inlets())
  {
    if (auto inlet = qobject_cast<Process::ControlInlet*>(e))
      setupInlet(*inlet, portFactory);
  }
  for (auto& e : m_effect.outlets())
  {
    if (auto outlet = qobject_cast<Process::ControlOutlet*>(e))
      setupOutlet(*outlet, portFactory);
  }
  updateRect();
}

void DefaultEffectItem::updateRect()
{
  const auto r = this->childrenBoundingRect();
  if (r.height() != 0.)
  {
    this->setRect(r);
  }
  else
  {
    this->setRect({0., 0., 100, 100});
  }
}
}
