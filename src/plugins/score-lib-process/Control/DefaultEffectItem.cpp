#include "DefaultEffectItem.hpp"

#include <score/graphics/layouts/GraphicsGridLayout.hpp>
#include <Process/Process.hpp>
#include <Control/Layout.hpp>
#include <Control/Widgets.hpp>
#include <Effect/EffectLayout.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/graphics/TextItem.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/ssize.hpp>

#include <QGraphicsScene>
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
    : score::EmptyRectItem{root}
    , m_effect{effect}
    , m_ctx{doc}
{
  QObject::connect(
      &effect,
      &Process::ProcessModel::controlAdded,
      this,
      &DefaultEffectItem::reset);

  QObject::connect(
      &effect,
      &Process::ProcessModel::controlRemoved,
      this,
      &DefaultEffectItem::reset);

  QObject::connect(
      &effect,
      &Process::ProcessModel::controlOutletAdded,
      this,
      &DefaultEffectItem::reset);

  QObject::connect(
      &effect,
      &Process::ProcessModel::controlOutletRemoved,
      this,
      &DefaultEffectItem::reset);

  reset();

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

DefaultEffectItem::~DefaultEffectItem() { }

void DefaultEffectItem::reset()
{
  delete m_layout;
  m_layout = nullptr;
  m_ports.clear();

  m_layout = new score::GraphicsDefaultLayout{this};
  recreate();
}

void DefaultEffectItem::recreate()
{
  auto& portFactory = m_ctx.app.interfaces<Process::PortFactoryList>();

  LayoutBuilderBase b{*this, m_ctx, portFactory, m_effect.inlets(), m_effect.outlets(), m_layout, {m_layout}};
  for (auto& e : m_effect.inlets())
  {
    if(auto inlet = qobject_cast<Process::ControlInlet*>(e))
    {
      auto item = b.makePort(*inlet);
      item->setParentItem(m_layout);

      con(*inlet, &Process::ControlInlet::domainChanged,
          this, &DefaultEffectItem::reset, Qt::UniqueConnection);
    }
  }

  for (auto& e : m_effect.outlets())
  {
    if(auto outlet = qobject_cast<Process::ControlOutlet*>(e))
    {
      auto item = b.makePort(*outlet);
      item->setParentItem(m_layout);

      con(*outlet, &Process::ControlOutlet::domainChanged,
          this, &DefaultEffectItem::reset, Qt::UniqueConnection);
    }
  }

  auto& lay = *m_layout;
  lay.layout();
  lay.fitChildrenRect();

  this->setRect(m_layout->rect());
}

}
