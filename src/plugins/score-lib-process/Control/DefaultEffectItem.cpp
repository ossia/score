#include "DefaultEffectItem.hpp"

#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <score/graphics/TextItem.hpp>

#include <QGraphicsScene>

#include <Control/Widgets.hpp>

namespace Media::Effect
{
struct DefaultEffectItem::Port
{
  score::EmptyRectItem* root;
  Dataflow::PortItem* port;
  QRectF rect;
};
DefaultEffectItem::DefaultEffectItem(
    const Process::ProcessModel& effect,
    const score::DocumentContext& doc,
    QGraphicsItem* root)
    : score::EmptyRectItem{root}, m_effect{effect}, m_ctx{doc}
{
  QObject::connect(
      &effect,
      &Process::ProcessModel::controlAdded,
      this,
      &DefaultEffectItem::on_controlAdded);

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

  for (auto& e : effect.inlets())
  {
    auto inlet = qobject_cast<Process::ControlInlet*>(e);
    if (!inlet)
      continue;

    setupInlet(*inlet, doc);
  }
  for (auto& e : effect.outlets())
  {
    auto outlet = qobject_cast<Process::ControlOutlet*>(e);
    if (!outlet)
      continue;

    setupOutlet(*outlet, doc);
  }
  updateRect();

  QObject::connect(
      &effect,
      &Process::ProcessModel::inletsChanged,
      this,
      &DefaultEffectItem::reset);
  QObject::connect(
        &effect,
        &Process::ProcessModel::outletsChanged,
        this,
        &DefaultEffectItem::reset);
}

DefaultEffectItem::~DefaultEffectItem()
{

}

static const constexpr int MaxRows = 4;
double DefaultEffectItem::currentColumnX() const
{
  int N = MaxRows * (m_ports.size() / MaxRows);
  qreal x = 0;
  for(int i = 0; i < N; )
  {
    qreal w = 0;
    for(int j = i; j < i + MaxRows && j < N; j++)
    {
      w = std::max(w, m_ports[j].rect.width());
    }
    x += w;
    i += MaxRows;
  }
  return x;
}


template<typename T>
void DefaultEffectItem::setupPort(
    T& port,
    const score::DocumentContext& doc)
{
  int i = m_ports.size();
  int row = i % MaxRows;
  // TODO put them in the correct order
  auto item = new score::EmptyRectItem{this};

  auto& portFactory
      = score::AppContext().interfaces<Process::PortFactoryList>();
  Process::PortFactory* fact = portFactory.get(port.concreteKey());
  auto portItem = fact->makeItem(port, doc, item, this);

  auto lab = new score::SimpleTextItem{Process::portBrush(port.type).main, item};
  if (port.customData().isEmpty())
    lab->setText(tr("Control"));
  else
    lab->setText(port.customData());
  connect(
      &port,
      &Process::ControlInlet::customDataChanged,
      item,
      [=](const QString& txt) { lab->setText(txt); });

  QGraphicsItem* widg = fact->makeControlItem(port, doc, item, this);
  widg->setParentItem(item);
  const qreal labelHeight = 10;
  const qreal labelWidth = lab->boundingRect().width();
  const auto wrect = widg->boundingRect();
  const qreal widgetHeight = wrect.height();
  const qreal widgetWidth = wrect.width();

  auto h = std::max(
      20.,
      (qreal)(widgetHeight + labelHeight + 7.));
  auto w = std::max(90., std::max(25. + labelWidth, widgetWidth));

  portItem->setPos(8., 4.);
  lab->setPos(20., 2);
  widg->setPos(18., labelHeight + 5.);

  const auto itemRect = QRectF{0., 0, w, h};
  item->setPos(currentColumnX(), row * h);
  item->setRect(itemRect);

  m_ports.push_back(Port{item, portItem, itemRect});
  updateRect();
}

void DefaultEffectItem::setupInlet(
    Process::ControlInlet& inlet,
    const score::DocumentContext& doc)
{
  setupPort(inlet, doc);
  con(inlet, &Process::ControlInlet::domainChanged, this, [this, &inlet] {
    on_controlRemoved(inlet);
    on_controlAdded(inlet.id());
  });
}

void DefaultEffectItem::setupOutlet(
    Process::ControlOutlet& outlet,
    const score::DocumentContext& doc)
{
  setupPort(outlet, doc);
  con(outlet, &Process::ControlOutlet::domainChanged, this, [this, &outlet] {
    on_controlOutletRemoved(outlet);
    on_controlOutletAdded(outlet.id());
  });
}

void DefaultEffectItem::on_controlAdded(const Id<Process::Port>& id)
{
  auto inlet = safe_cast<Process::ControlInlet*>(m_effect.inlet(id));
  setupInlet(*inlet, m_ctx);
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
  auto outlet = safe_cast<Process::ControlOutlet*>(m_effect.outlet(id));
  setupOutlet(*outlet, m_ctx);
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

  for (auto& e : m_effect.inlets())
  {
    if (auto inlet = qobject_cast<Process::ControlInlet*>(e))
      setupInlet(*inlet, m_ctx);
  }
  for (auto& e : m_effect.outlets())
  {
    if (auto outlet = qobject_cast<Process::ControlOutlet*>(e))
      setupOutlet(*outlet, m_ctx);
  }
  updateRect();
}

void DefaultEffectItem::updateRect()
{
  const auto r = this->childrenBoundingRect();
  if(r.height() != 0.) {
    this->setRect(r);
  }
  else {
    this->setRect({0., 0., 140, 0});
  }
}
}
