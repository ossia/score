#include "DefaultEffectItem.hpp"

#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <score/graphics/TextItem.hpp>

#include <QGraphicsScene>

#include <Control/Widgets.hpp>

namespace Media::Effect
{
DefaultEffectItem::DefaultEffectItem(
    const Process::ProcessModel& effect,
    const score::DocumentContext& doc,
    score::RectItem* root)
    : score::RectItem{root}, m_effect{effect}, m_ctx{doc}
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

  for (auto& e : effect.inlets())
  {
    auto inlet = qobject_cast<Process::ControlInlet*>(e);
    if (!inlet)
      continue;

    setupInlet(*inlet, doc);
  }

  QObject::connect(
      &effect,
      &Process::ProcessModel::inletsChanged,
      this,
      &DefaultEffectItem::reset);
  // TODO same for outlets if we have control outlets one day
}

void DefaultEffectItem::setupInlet(
    Process::ControlInlet& inlet,
    const score::DocumentContext& doc)
{
  con(inlet, &Process::ControlInlet::domainChanged, this, [this, &inlet] {
    on_controlRemoved(inlet);
    on_controlAdded(inlet.id());
  });
  // TODO put them in the correct order
  auto item = new score::EmptyRectItem{this};
  double pos_y = this->childrenBoundingRect().height();

  auto& portFactory
      = score::AppContext().interfaces<Process::PortFactoryList>();
  Process::PortFactory* fact = portFactory.get(inlet.concreteKey());
  auto port = fact->makeItem(inlet, doc, item, this);
  m_ports.push_back({item, port});

  auto lab = new score::SimpleTextItem{Process::Style::instance().EventDefault,
                                       item};
  if (inlet.customData().isEmpty())
    lab->setText(tr("Control"));
  else
    lab->setText(inlet.customData());
  connect(
      &inlet,
      &Process::ControlInlet::customDataChanged,
      item,
      [=](const QString& txt) { lab->setText(txt); });
  lab->setPos(15, 2);

  QGraphicsItem* widg{};
  auto& dom = inlet.domain().get();
  if (bool(dom))
  {
    auto min = dom.convert_min<float>();
    auto max = dom.convert_max<float>();
    struct
    {
      float min, max;
      float getMin() const { return min; }
      float getMax() const { return max; }
    } info{min, max};
    widg = Control::FloatSlider::make_item(info, inlet, doc, nullptr, this);
  }
  else
  {
    struct SliderInfo
    {
      static float getMin() { return 0.; }
      static float getMax() { return 1.; }
    };
    widg = Control::FloatSlider::make_item(
        SliderInfo{}, inlet, doc, nullptr, this);
  }

  widg->setParentItem(item);
  widg->setPos(15, lab->boundingRect().height());

  auto h = std::max(
      20.,
      (qreal)(
          widg->boundingRect().height() + lab->boundingRect().height() + 2.));

  port->setPos(7., h / 2.);

  item->setPos(0, pos_y);
  item->setRect(QRectF{0., 0, 170., h});
}

void DefaultEffectItem::on_controlAdded(const Id<Process::Port>& id)
{
  auto inlet = safe_cast<Process::ControlInlet*>(m_effect.inlet(id));
  setupInlet(*inlet, m_ctx);
  this->setRect(this->childrenBoundingRect());
}

void DefaultEffectItem::on_controlRemoved(const Process::Port& port)
{
  for (auto it = m_ports.begin(); it != m_ports.end(); ++it)
  {
    auto ptr = it->second;
    if (&ptr->port() == &port)
    {
      auto parent_item = it->first;
      auto h = parent_item->boundingRect().height();
      delete parent_item;
      m_ports.erase(it);

      for (; it != m_ports.end(); ++it)
      {
        auto item = it->first;
        item->moveBy(0., -h);
      }
      this->setRect(this->childrenBoundingRect());
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
  this->setRect(this->childrenBoundingRect());
  m_ports.clear();

  for (auto& e : m_effect.inlets())
  {
    if (auto inlet = qobject_cast<Process::ControlInlet*>(e))
      setupInlet(*inlet, m_ctx);
  }
  this->setRect(this->childrenBoundingRect());
}
}
