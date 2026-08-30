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
#include <score/graphics/widgets/QGraphicsPixmapButton.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/Pixmap.hpp>

#include <ossia/detail/ssize.hpp>

#include <ossia-qt/invoke.hpp>

#include <QGraphicsScene>
namespace Process
{
static const constexpr qreal PagerHeight = 13.;
static const constexpr qreal PagerSpacing = 3.;

//! Leading non-control inlets get a column of their own once there are enough
//! of them; below that they share the control grid.
static bool hasOwnInletColumn(int leadingInlets) noexcept
{
  return leadingInlets >= Process::ControlsPerColumn;
}

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

static void deletePortItems(QGraphicsItem* it)
{
  auto items = it->childItems();
  for(auto ptr : items)
  {
    if(auto r = qgraphicsitem_cast<Dataflow::PortItem*>(ptr))
      delete r;
    else
      deletePortItems(ptr);
  }
}
void DefaultEffectItem::reset()
{
  if(m_layout)
  {
    deletePortItems(m_layout);
    m_layout->setVisible(false);
    m_layout->deleteLater();
    m_layout = nullptr;
  }

  m_allLayouts.clear();

  delete m_pager;
  m_pager = nullptr;

  m_needRecreate = true;
  ossia::qt::run_async(this, &DefaultEffectItem::recreate);
}

DefaultEffectItem::PortsToDisplay DefaultEffectItem::computeDisplayedPorts() const
{
  PortsToDisplay res;

  // Folded, the node shows its routing: everything that is not a control, plus
  // the controls that take part in the patch.
  // Unfolded, the controls keep their declaration order and are walked through
  // one page at a time. A cable landing on a control that is not on the current
  // page is simply not drawn, as with the tabbed layouts.
  int controlCount = 0;
  int gridOffset = 0;
  if(!m_onlyUndisplayed)
  {
    for(Process::Inlet* e : m_effect.inlets())
      controlCount += Process::isControlPort(*e);
    for(Process::Outlet* e : m_effect.outlets())
      controlCount += Process::isControlPort(*e);

    // The leading non-control inlets are laid out in the same grid as the
    // controls unless there are enough of them for their own column: they take
    // up the first slots and shift where the columns end.
    int leading = 0;
    for(Process::Inlet* e : m_effect.inlets())
    {
      if(Process::isControlPort(*e))
        break;
      leading++;
    }
    if(!hasOwnInletColumn(leading))
      gridOffset = leading;
  }

  res.page = Process::nodeControlPage(controlCount, m_page, gridOffset);

  int controlIndex = 0;
  const auto take = [&](const Process::Port& p) {
    if(m_onlyUndisplayed)
      return Process::isVisibleWhenFolded(p);
    if(!Process::isControlPort(p))
      return true;
    const int idx = controlIndex++;
    return idx >= res.page.first && idx < res.page.last;
  };

  for(Process::Inlet* e : m_effect.inlets())
    if(take(*e))
      res.inlets.push_back(e);
  for(Process::Outlet* e : m_effect.outlets())
    if(take(*e))
      res.outlets.push_back(e);

  for(Process::Inlet* e : res.inlets)
  {
    if(Process::isControlPort(*e))
      break;
    res.leadingInlets++;
  }

  return res;
}

void DefaultEffectItem::updateDisplayedPorts()
{
  auto ports = computeDisplayedPorts();

  m_shownInlets = std::move(ports.inlets);
  m_shownOutlets = std::move(ports.outlets);
  m_leadingInlets = ports.leadingInlets;
  m_controlPage = ports.page;
  m_page = m_controlPage.page;

  if(m_onlyUndisplayed)
  {
    // Only the folded display depends on how the controls are wired up.
    const auto follow = [this](Process::Port& p) {
      if(!Process::isControlPort(p))
        return;
      QObject::connect(
          &p, &Process::Port::cablesChanged, this,
          &DefaultEffectItem::onPortWiringChanged, Qt::UniqueConnection);
      QObject::connect(
          &p, &Process::Port::addressChanged, this,
          &DefaultEffectItem::onPortWiringChanged, Qt::UniqueConnection);
    };
    for(Process::Inlet* e : m_effect.inlets())
      follow(*e);
    for(Process::Outlet* e : m_effect.outlets())
      follow(*e);
  }
}

void DefaultEffectItem::onPortWiringChanged()
{
  // Port::cablesChanged is emitted from inside the drop that creates the cable,
  // while the event is still being delivered to the port item: rebuilding here
  // would delete that very item under the caller's feet. Do it once the drop is
  // over, and only when the change actually shows.
  // One check per batch: loading a document wires up every port in a row.
  if(m_wiringCheckPending)
    return;
  m_wiringCheckPending = true;

  ossia::qt::run_async(this, [this] {
    m_wiringCheckPending = false;
    auto ports = computeDisplayedPorts();
    if(ports.inlets != m_shownInlets || ports.outlets != m_shownOutlets)
      reset();
  });
}

template <typename Port_T>
Process::ControlLayout
DefaultEffectItem::makeItem(Process::LayoutBuilderBase& b, Port_T& port)
{
  if(!m_onlyUndisplayed)
    return b.makePort(port);

  // Folded: the routing only, no control widget
  if(auto* f = b.portFactory.get(port.concreteKey()))
    return f->makeLabelItem(port, m_ctx, b.layout, this);
  return {};
}

void DefaultEffectItem::recreate()
{
  if(!m_needRecreate)
    return;
  m_needRecreate = false;

  updateDisplayedPorts();

  const bool has_i = !m_shownInlets.empty();
  const bool has_o = !m_shownOutlets.empty();

  if(has_i && has_o)
    recreate_both();
  else if(has_i)
  {
    // A long column of message inlets reads better next to the control grid
    // than merged into it.
    if(m_leadingInlets > 5)
      recreate_both();
    else
      recreate_onlyInlets();
  }
  else if(has_o)
    recreate_onlyOutlets();

  if(m_controlPage.paginated())
    createPager();

  updateRect();
}

void DefaultEffectItem::recreate_onlyInlets()
{
  auto& portFactory = m_ctx.app.interfaces<Process::PortFactoryList>();

  m_layout = new score::GraphicsDefaultLayout{this};
  m_allLayouts.push_back(m_layout);
  LayoutBuilderBase b{*this,       m_effect,          m_ctx,
                      portFactory, m_effect.inlets(), m_effect.outlets(),
                      m_layout,    {m_layout}};

  for(Process::Inlet* e : m_shownInlets)
  {
    SCORE_ASSERT(e->parent());
    auto item = makeItem(b, *e);
    SCORE_SOFT_ASSERT(item.container);
    if(item.container)
      item.container->setParentItem(m_layout);
    if(auto inlet = qobject_cast<Process::ControlInlet*>(e))
      con(*inlet, &Process::ControlInlet::domainChanged, this,
          &DefaultEffectItem::reset, Qt::UniqueConnection);
  }

  auto& lay = *m_layout;
  lay.layout();
  lay.fitChildrenRect();
}

void DefaultEffectItem::recreate_onlyOutlets()
{
  auto& portFactory = m_ctx.app.interfaces<Process::PortFactoryList>();

  m_layout = new score::GraphicsDefaultOutletLayout{this};
  m_allLayouts.push_back(m_layout);
  LayoutBuilderBase b{*this,       m_effect,          m_ctx,
                      portFactory, m_effect.inlets(), m_effect.outlets(),
                      m_layout,    {m_layout}};

  for(Process::Outlet* e : m_shownOutlets)
  {
    SCORE_ASSERT(e->parent());
    auto item = makeItem(b, *e);
    SCORE_SOFT_ASSERT(item.container);
    if(item.container)
      item.container->setParentItem(m_layout);

    if(auto outlet = qobject_cast<Process::ControlOutlet*>(e))
      con(*outlet, &Process::ControlOutlet::domainChanged, this,
          &DefaultEffectItem::reset, Qt::UniqueConnection);
  }

  auto& lay = *m_layout;
  lay.layout();
  lay.fitChildrenRect();
}

void DefaultEffectItem::recreate_both()
{
  auto& portFactory = m_ctx.app.interfaces<Process::PortFactoryList>();

  auto layout = new score::GraphicsIORootLayout{this};
  m_allLayouts.push_back(layout);
  layout->setMinimumWidth(m_minimumWidth);
  m_layout = layout;

  LayoutBuilderBase b{*this,       m_effect,          m_ctx,
                      portFactory, m_effect.inlets(), m_effect.outlets(),
                      m_layout,    {m_layout}};

  const int split = hasOwnInletColumn(m_leadingInlets) ? m_leadingInlets : 0;

  if(split > 0)
  {
    auto inlet_layout = new score::GraphicsDefaultInletLayout{m_layout};
    m_allLayouts.push_back(inlet_layout);
    for(int i = 0; i < split; i++)
    {
      auto item = makeItem(b, *m_shownInlets[i]);
      SCORE_SOFT_ASSERT(item.container);
      if(item.container)
        item.container->setParentItem(inlet_layout);
    }
    inlet_layout->layout();
    inlet_layout->fitChildrenRect();
  }

  // Folding can leave nothing but the split-off column: no empty layout then,
  // GraphicsIORootLayout lays out by child count.
  if(std::ssize(m_shownInlets) > split)
  {
    auto control_layout = new score::GraphicsDefaultLayout{m_layout};
    m_allLayouts.push_back(control_layout);
    for(int i = split, n = std::ssize(m_shownInlets); i < n; i++)
    {
      auto* e = m_shownInlets[i];
      SCORE_ASSERT(e->parent());
      auto item = makeItem(b, *e);
      SCORE_SOFT_ASSERT(item.container);
      if(item.container)
        item.container->setParentItem(control_layout);
      if(auto inlet = qobject_cast<Process::ControlInlet*>(e))
        con(*inlet, &Process::ControlInlet::domainChanged, this,
            &DefaultEffectItem::reset, Qt::UniqueConnection);
    }
    control_layout->layout();
    control_layout->fitChildrenRect();
  }

  if(!m_shownOutlets.empty())
  {
    auto outlet_layout = new score::GraphicsDefaultOutletLayout{m_layout};
    m_allLayouts.push_back(outlet_layout);
    for(Process::Outlet* e : m_shownOutlets)
    {
      SCORE_ASSERT(e->parent());
      auto item = makeItem(b, *e);
      SCORE_SOFT_ASSERT(item.container);
      if(item.container)
        item.container->setParentItem(outlet_layout);

      if(auto outlet = qobject_cast<Process::ControlOutlet*>(e))
        con(*outlet, &Process::ControlOutlet::domainChanged, this,
            &DefaultEffectItem::reset, Qt::UniqueConnection);
    }
    outlet_layout->layout();
    outlet_layout->fitChildrenRect();
  }

  auto& lay = *m_layout;
  lay.layout();

  lay.setRect(lay.childrenBoundingRect());
  lay.fitChildrenRect();
}

void DefaultEffectItem::createPager()
{
  // Dimmed at rest, accented while pressed: the pager is chrome, the controls
  // above it are what the node is about.
  static const auto prev_on = score::get_pixmap(":/icons/arrow_left_on.png");
  static const auto prev_off = score::get_pixmap(":/icons/arrow_left_disabled.png");
  static const auto next_on = score::get_pixmap(":/icons/arrow_right_on.png");
  static const auto next_off = score::get_pixmap(":/icons/arrow_right_disabled.png");

  m_pager = new score::EmptyRectItem{this};

  auto prev = new score::QGraphicsPixmapButton{prev_on, prev_off, m_pager};
  auto next = new score::QGraphicsPixmapButton{next_on, next_off, m_pager};

  // Same type as the port and control labels around it
  auto label = new score::SimpleTextItem{Process::labelBrush().main, m_pager};
  label->setText(
      QStringLiteral("%1/%2").arg(m_controlPage.page + 1).arg(m_controlPage.pageCount));

  const auto lab_w = label->boundingRect().width();
  const auto arrow_w = prev->boundingRect().width();

  const auto center = [](qreal h) { return std::round((PagerHeight - h) / 2.); };
  prev->setPos(0., center(prev->boundingRect().height()));
  label->setPos(arrow_w + PagerSpacing, center(label->boundingRect().height()));
  next->setPos(
      arrow_w + 2. * PagerSpacing + lab_w, center(next->boundingRect().height()));

  m_pager->setRect(
      {0., 0., 2. * arrow_w + 2. * PagerSpacing + lab_w + 2., PagerHeight});

  // The click is delivered from the button's own event handler: changing page
  // deletes it, so it has to happen once the event is done being dispatched.
  connect(prev, &score::QGraphicsPixmapButton::clicked, this, [this] {
    ossia::qt::run_async(this, [this] { setPage(m_page - 1); });
  });
  connect(next, &score::QGraphicsPixmapButton::clicked, this, [this] {
    ossia::qt::run_async(this, [this] { setPage(m_page + 1); });
  });
}

void DefaultEffectItem::setPage(int page)
{
  page = std::clamp(page, 0, m_controlPage.pageCount - 1);
  if(page == m_page)
    return;

  m_page = page;
  reset();
}

void DefaultEffectItem::updateRect()
{
  QRectF r = m_layout ? m_layout->rect() : QRectF{};
  if(m_pager)
  {
    const auto pager_r = m_pager->rect();
    m_pager->setPos(0., r.height());
    r.setHeight(r.height() + pager_r.height());
    r.setWidth(std::max(r.width(), pager_r.width()));
  }
  this->setRect(r);
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

    updateRect();
  }
}
}
