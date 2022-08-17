#include "DefaultEffectItem.hpp"

#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <Control/Layout.hpp>
#include <Control/Widgets.hpp>
#include <Effect/EffectLayout.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/graphics/TextItem.hpp>
#include <score/graphics/layouts/GraphicsGridLayout.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/ssize.hpp>

#include <ossia-qt/invoke.hpp>

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
    const Process::ProcessModel& effect, const Process::Context& doc,
    QGraphicsItem* root)
    : score::EmptyRectItem{root}
    , m_effect{effect}
    , m_ctx{doc}
{
  QObject::connect(
      &effect, &Process::ProcessModel::controlAdded, this, &DefaultEffectItem::reset);

  QObject::connect(
      &effect, &Process::ProcessModel::controlRemoved, this, &DefaultEffectItem::reset);

  QObject::connect(
      &effect, &Process::ProcessModel::controlOutletAdded, this,
      &DefaultEffectItem::reset);

  QObject::connect(
      &effect, &Process::ProcessModel::controlOutletRemoved, this,
      &DefaultEffectItem::reset);

  reset();

  QObject::connect(
      &effect, &Process::ProcessModel::inletsChanged, this, &DefaultEffectItem::reset);
  QObject::connect(
      &effect, &Process::ProcessModel::outletsChanged, this, &DefaultEffectItem::reset);

  connect(this, &score::ResizeableItem::childrenSizeChanged, this, [this] {
    if(m_layout)
    {
      auto& lay = *m_layout;
      lay.layout();
      lay.fitChildrenRect();

      this->setRect(m_layout->rect());
    }
  });
}

DefaultEffectItem::~DefaultEffectItem() { }

void DefaultEffectItem::reset()
{
  delete m_layout;
  m_layout = nullptr;
  m_ports.clear();

  m_needRecreate = true;
  ossia::qt::run_async(this, &DefaultEffectItem::recreate);
}

void DefaultEffectItem::recreate()
{
  if(!m_needRecreate)
    return;
  m_needRecreate = false;

  auto& portFactory = m_ctx.app.interfaces<Process::PortFactoryList>();

  m_layout = new score::GraphicsDefaultLayout{this};
  LayoutBuilderBase b{
      *this,    m_ctx,     portFactory, m_effect.inlets(), m_effect.outlets(),
      m_layout, {m_layout}};
  for(auto& e : m_effect.inlets())
  {
    SCORE_ASSERT(e->parent());
    if(auto inlet = qobject_cast<Process::ControlInlet*>(e))
    {
      auto item = b.makePort(*inlet);
      item->setParentItem(m_layout);

      con(*inlet, &Process::ControlInlet::domainChanged, this, &DefaultEffectItem::reset,
          Qt::UniqueConnection);
    }
  }

  for(auto& e : m_effect.outlets())
  {
    SCORE_ASSERT(e->parent());
    if(auto outlet = qobject_cast<Process::ControlOutlet*>(e))
    {
      auto item = b.makePort(*outlet);
      item->setParentItem(m_layout);

      con(*outlet, &Process::ControlOutlet::domainChanged, this,
          &DefaultEffectItem::reset, Qt::UniqueConnection);
    }
  }

  auto& lay = *m_layout;
  lay.layout();
  lay.fitChildrenRect();

  this->setRect(m_layout->rect());
}

}
