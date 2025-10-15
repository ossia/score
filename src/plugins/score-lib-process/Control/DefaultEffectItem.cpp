#include "DefaultEffectItem.hpp"

#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <Control/Layout.hpp>
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

DefaultEffectItem::DefaultEffectItem(
    bool onlyUndisplayed, const Process::ProcessModel& effect,
    const Process::Context& doc, QGraphicsItem* root)
    : score::EmptyRectItem{root}
    , m_effect{effect}
    , m_ctx{doc}
    , m_onlyUndisplayed{onlyUndisplayed}
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

  connect(
      this, &score::ResizeableItem::childrenSizeChanged, this,
      &DefaultEffectItem::relayout);
  connect(
      this, &score::ResizeableItem::minimumWidthChanged, this,
      &DefaultEffectItem::reset);
}

DefaultEffectItem::~DefaultEffectItem() { }

void DefaultEffectItem::reset()
{
  delete m_layout;
  m_layout = nullptr;
  m_allLayouts.clear();

  m_needRecreate = true;
  ossia::qt::run_async(this, &DefaultEffectItem::recreate);
}

void DefaultEffectItem::recreate()
{
  if(!m_needRecreate)
    return;
  m_needRecreate = false;

  int n_i = m_effect.inlets().size();
  int n_o = m_effect.outlets().size();
  int num_hidden = 0;
  for(auto& e : m_effect.inlets())
  {
    if(!e->displayHandledExplicitly)
      num_hidden++;
    else
      break;
  }

  if(!m_onlyUndisplayed)
  {
    if(n_i > 0 && n_o > 0)
      recreate_full_both(num_hidden);
    else if(n_i > 0 && n_o == 0)
    {
      if(num_hidden > 5)
        recreate_full_both(num_hidden);
      else
        recreate_full_onlyInlets();
    }
    else if(n_i == 0 && n_o > 0)
      recreate_full_onlyOutlets();
  }
  else
  {
    if(n_i > 0 && n_o > 0)
      recreate_fold_both();
    else if(n_i > 0 && n_o == 0)
      recreate_fold_onlyInlets();
    else if(n_i == 0 && n_o > 0)
      recreate_fold_onlyOutlets();
  }
}

void DefaultEffectItem::recreate_full_onlyInlets()
{
  auto& portFactory = m_ctx.app.interfaces<Process::PortFactoryList>();

  m_layout = new score::GraphicsDefaultLayout{this};
  m_allLayouts.push_back(m_layout);
  LayoutBuilderBase b{*this,       m_effect,          m_ctx,
                      portFactory, m_effect.inlets(), m_effect.outlets(),
                      m_layout,    {m_layout}};

  for(auto& e : m_effect.inlets())
  {
    SCORE_ASSERT(e->parent());
    {
      auto item = b.makePort(*e);
      SCORE_SOFT_ASSERT(item.container);
      item.container->setParentItem(m_layout);
      if(auto inlet = qobject_cast<Process::ControlInlet*>(e))
        con(*inlet, &Process::ControlInlet::domainChanged, this,
            &DefaultEffectItem::reset, Qt::UniqueConnection);
    }
  }

  auto& lay = *m_layout;
  lay.layout();
  lay.fitChildrenRect();

  this->setRect(m_layout->rect());
}

void DefaultEffectItem::recreate_full_onlyOutlets()
{
  auto& portFactory = m_ctx.app.interfaces<Process::PortFactoryList>();

  m_layout = new score::GraphicsDefaultOutletLayout{this};
  m_allLayouts.push_back(m_layout);
  LayoutBuilderBase b{*this,       m_effect,          m_ctx,
                      portFactory, m_effect.inlets(), m_effect.outlets(),
                      m_layout,    {m_layout}};

  for(auto& e : m_effect.outlets())
  {
    SCORE_ASSERT(e->parent());
    {
      auto item = b.makePort(*e);
      SCORE_SOFT_ASSERT(item.container);
      item.container->setParentItem(m_layout);

      if(auto outlet = qobject_cast<Process::ControlOutlet*>(e))
        con(*outlet, &Process::ControlOutlet::domainChanged, this,
            &DefaultEffectItem::reset, Qt::UniqueConnection);
    }
  }

  auto& lay = *m_layout;
  lay.layout();
  lay.fitChildrenRect();

  this->setRect(m_layout->rect());
}

void DefaultEffectItem::recreate_full_both(int num_hidden)
{
  auto& portFactory = m_ctx.app.interfaces<Process::PortFactoryList>();

  auto layout = new score::GraphicsIORootLayout{this};
  m_allLayouts.push_back(layout);
  layout->setMinimumWidth(m_minimumWidth);
  m_layout = layout;

  LayoutBuilderBase b{*this,       m_effect,          m_ctx,
                      portFactory, m_effect.inlets(), m_effect.outlets(),
                      m_layout,    {m_layout}};

  if(num_hidden >= 5)
  {
    auto inlet_layout = new score::GraphicsDefaultInletLayout{m_layout};
    m_allLayouts.push_back(inlet_layout);
    for(int i = 0; i < num_hidden; i++)
    {
      auto& e = *m_effect.inlets()[i];
      auto item = b.makePort(e);
      SCORE_SOFT_ASSERT(item.container);
      item.container->setParentItem(inlet_layout);
    }
    inlet_layout->layout();
    inlet_layout->fitChildrenRect();

    auto control_layout = new score::GraphicsDefaultLayout{m_layout};
    m_allLayouts.push_back(control_layout);
    for(int i = num_hidden; i < m_effect.inlets().size(); i++)
    {
      auto& e = *m_effect.inlets()[i];
      auto item = b.makePort(e);
      SCORE_SOFT_ASSERT(item.container);
      item.container->setParentItem(control_layout);
      if(auto inlet = qobject_cast<Process::ControlInlet*>(&e))
        con(*inlet, &Process::ControlInlet::domainChanged, this,
            &DefaultEffectItem::reset, Qt::UniqueConnection);
    }
    control_layout->layout();
    control_layout->fitChildrenRect();
  }
  else
  {
    auto inlet_layout = new score::GraphicsDefaultLayout{m_layout};
    m_allLayouts.push_back(inlet_layout);
    for(auto& e : m_effect.inlets())
    {
      SCORE_ASSERT(e->parent());
      {
        auto item = b.makePort(*e);
        SCORE_SOFT_ASSERT(item.container);
        item.container->setParentItem(inlet_layout);
        if(auto inlet = qobject_cast<Process::ControlInlet*>(e))
          con(*inlet, &Process::ControlInlet::domainChanged, this,
              &DefaultEffectItem::reset, Qt::UniqueConnection);
      }
    }
    inlet_layout->layout();
    inlet_layout->fitChildrenRect();
  }

  if(!m_effect.outlets().empty())
  {
    auto outlet_layout = new score::GraphicsDefaultOutletLayout{m_layout};
    m_allLayouts.push_back(outlet_layout);
    for(auto& e : m_effect.outlets())
    {
      SCORE_ASSERT(e->parent());
      {
        auto item = b.makePort(*e);
        SCORE_SOFT_ASSERT(item.container);
        item.container->setParentItem(outlet_layout);

        if(auto outlet = qobject_cast<Process::ControlOutlet*>(e))
          con(*outlet, &Process::ControlOutlet::domainChanged, this,
              &DefaultEffectItem::reset, Qt::UniqueConnection);
      }
    }
    outlet_layout->layout();
    outlet_layout->fitChildrenRect();
  }

  auto& lay = *m_layout;
  lay.layout();

  lay.setRect(lay.childrenBoundingRect());
  lay.fitChildrenRect();

  this->setRect(m_layout->rect());
}

void DefaultEffectItem::recreate_fold_onlyInlets()
{
  auto& portFactory = m_ctx.app.interfaces<Process::PortFactoryList>();

  m_layout = new score::GraphicsDefaultInletLayout{this};
  m_allLayouts.push_back(m_layout);
  LayoutBuilderBase b{*this,       m_effect,          m_ctx,
                      portFactory, m_effect.inlets(), m_effect.outlets(),
                      m_layout,    {m_layout}};

  for(auto& e : m_effect.inlets())
  {
    SCORE_ASSERT(e->parent());
    //if(!e->displayHandledExplicitly)
    if(auto* f = portFactory.get(e->concreteKey()))
    {
      auto item = f->makeLabelItem(*e, m_ctx, m_layout, this);
      SCORE_SOFT_ASSERT(item.container);
      item.container->setParentItem(m_layout);
    }
  }

  auto& lay = *m_layout;
  lay.layout();
  lay.fitChildrenRect();

  this->setRect(m_layout->rect());
}

void DefaultEffectItem::recreate_fold_onlyOutlets()
{
  auto& portFactory = m_ctx.app.interfaces<Process::PortFactoryList>();

  m_layout = new score::GraphicsDefaultOutletLayout{this};
  m_allLayouts.push_back(m_layout);
  LayoutBuilderBase b{*this,       m_effect,          m_ctx,
                      portFactory, m_effect.inlets(), m_effect.outlets(),
                      m_layout,    {m_layout}};

  for(auto& e : m_effect.outlets())
  {
    SCORE_ASSERT(e->parent());
    if(auto* f = portFactory.get(e->concreteKey()))
    {
      auto item = f->makeLabelItem(*e, m_ctx, m_layout, this);
      SCORE_SOFT_ASSERT(item.container);
      item.container->setParentItem(m_layout);
    }
  }

  auto& lay = *m_layout;
  lay.layout();
  lay.fitChildrenRect();

  this->setRect(m_layout->rect());
}

void DefaultEffectItem::recreate_fold_both()
{
  auto& portFactory = m_ctx.app.interfaces<Process::PortFactoryList>();

  auto layout = new score::GraphicsIORootLayout{this};
  m_allLayouts.push_back(layout);
  layout->setMinimumWidth(m_minimumWidth);
  m_layout = layout;

  LayoutBuilderBase b{*this,       m_effect,          m_ctx,
                      portFactory, m_effect.inlets(), m_effect.outlets(),
                      m_layout,    {m_layout}};

  auto inlet_layout = new score::GraphicsDefaultInletLayout{m_layout};
  m_allLayouts.push_back(inlet_layout);
  for(auto& e : m_effect.inlets())
  {
    SCORE_ASSERT(e->parent());
    if(auto* f = portFactory.get(e->concreteKey()))
    {
      auto item = f->makeLabelItem(*e, m_ctx, inlet_layout, this);
      SCORE_SOFT_ASSERT(item.container);
      item.container->setParentItem(inlet_layout);
    }
  }
  inlet_layout->layout();
  inlet_layout->fitChildrenRect();

  auto outlet_layout = new score::GraphicsDefaultOutletLayout{m_layout};
  m_allLayouts.push_back(outlet_layout);
  for(auto& e : m_effect.outlets())
  {
    SCORE_ASSERT(e->parent());
    if(auto* f = portFactory.get(e->concreteKey()))
    {
      auto item = f->makeLabelItem(*e, m_ctx, outlet_layout, this);
      SCORE_SOFT_ASSERT(item.container);
      item.container->setParentItem(outlet_layout);
    }
  }
  outlet_layout->layout();
  outlet_layout->fitChildrenRect();

  auto& lay = *m_layout;
  lay.layout();

  lay.setRect(lay.childrenBoundingRect());
  lay.fitChildrenRect();

  this->setRect(m_layout->rect());
}

void DefaultEffectItem::relayout()
{
  if(m_layout)
  {
    for(auto it = m_allLayouts.rbegin(); it != m_allLayouts.rend(); ++it)
    {
      auto& lay = **it;
      lay.layout();
      lay.fitChildrenRect();
    }

    this->setRect(m_layout->rect());
  }
}
}
