#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/NodeItem.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <Process/Style/Pixmaps.hpp>
#include <Process/Style/ScenarioStyle.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectLayer.hpp>
#include <Effect/EffectPainting.hpp>

#include <Process/ApplicationPlugin.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/graphics/GraphicWidgets.hpp>
#include <score/graphics/GraphicsLayout.hpp>
#include <score/graphics/TextItem.hpp>
#include <score/model/Skin.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/tools/Bind.hpp>

#include <ossia-qt/invoke.hpp>

#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QTimer>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Process::NodeItem)
namespace Process
{

static const constexpr qreal TitleHeight = 20.;
//static const constexpr qreal TitleX0 = 15;
static const constexpr qreal TitleY0 = -TitleHeight + 2.;
static const constexpr qreal FoldX0 = 2;
static const constexpr qreal FoldY0 = TitleY0 + 1;
static const constexpr qreal UiX0 = 16;
static const constexpr qreal UiY0 = TitleY0 + 1.;
static const constexpr qreal TitleWithUiX0 = 27;
static const constexpr qreal SpacingIcon = 3;

static const constexpr qreal FooterHeight = 0.;
static const constexpr qreal Corner = 2.;
static const constexpr qreal PortSpacing = 10.;
static const constexpr qreal InletX0 = -10.;
static const constexpr qreal InletY0 = 0;
static const constexpr qreal OutletX0 = -2.;
static const constexpr qreal OutletY0 = InletY0; // Add to height
static const constexpr qreal TopButtonX0 = -12.;
static const constexpr qreal TopButtonY0 = TitleY0 + 2.;
static const constexpr qreal LeftSideWidth = 10.;
static const constexpr qreal RightSideWidth = LeftSideWidth;

NodeItem::NodeItem(
    const Process::ProcessModel& process, const Process::Context& ctx, TimeVal parentDur,
    QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_model{process}
    , m_context{ctx}
    , m_dispatcher{ctx.commandStack}
    , m_parentDuration{parentDur}
{
  setObjectName("NodeItem");
  setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
  setAcceptHoverEvents(true);
  setAcceptDrops(true);
  setFlag(ItemIsFocusable, true);
  setFlag(ItemClipsChildrenToShape, true);
  const auto& pf
      = ctx.app.interfaces<Process::ProcessFactoryList>().get(process.concreteKey());
  setData(0xF1, pf->descriptor(process).documentationLink);

  if(process.flags() & Process::ProcessFlags::FullyCustomItem)
  {
    createWithoutDecorations();
  }
  else
  {
    createWithDecorations();
  }

  initConnections();
}

void NodeItem::createWithDecorations()
{
  auto& process = m_model;
  auto& ctx = m_context;

  m_folded = (process.inlets().size() > 32);

  // Title
  const auto& pixmaps = Process::Pixmaps::instance();
  m_fold
      = new score::QGraphicsPixmapToggle{pixmaps.unroll_small, pixmaps.roll_small, this};
  m_fold->setState(!m_folded);

  m_scriptButton = Process::makeScriptButton(
      const_cast<Process::ProcessModel&>(process), ctx, this, this);
  m_uiButton = Process::makeExternalUIButton(
      const_cast<Process::ProcessModel&>(process), ctx, this, this);
  m_presetButton = Process::makePresetButton(process, ctx, this, this);

  auto& skin = score::Skin::instance();
  m_label = new score::SimpleTextItem{skin.Light.main, this};
  m_label->setFont(skin.Bold10Pt);

  updateLabel();

  con(process.metadata(), &score::ModelMetadata::NameChanged, this,
      &NodeItem::updateLabel);
  con(process.metadata(), &score::ModelMetadata::LabelChanged, this,
      &NodeItem::updateLabel);

  con(process, &Process::ProcessModel::loopsChanged, this, [&] { updateZoomRatio(); });
  con(process, &Process::ProcessModel::loopDurationChanged, this,
      [&] { updateZoomRatio(); });
  con(process, &Process::ProcessModel::startOffsetChanged, this,
      [&] { updateZoomRatio(); });

  // Selection
  con(process.selection, &Selectable::changed, this, &NodeItem::setSelected);
  m_selected = process.selection.get();

  if(!m_folded)
    createContentItem();
  else
    createFoldedItem();

  // This is because if it's a DefaultEffectItem the ports are created asynchronously
  // Thus they are added to the vertical lists instead of the actual items
  ossia::qt::run_async(this, [this] {
    resetInlets();
    resetOutlets();

    resizeAsync();
  });
  connect(&process, &Process::ProcessModel::controlAdded, this, &NodeItem::resetInlets);
  connect(
      &process, &Process::ProcessModel::controlRemoved, this, &NodeItem::resetInlets);
  connect(
      &process, &Process::ProcessModel::controlOutletAdded, this,
      &NodeItem::resetOutlets);
  connect(
      &process, &Process::ProcessModel::controlOutletRemoved, this,
      &NodeItem::resetOutlets);
  connect(&process, &Process::ProcessModel::inletsChanged, this, &NodeItem::resetInlets);
  connect(
      &process, &Process::ProcessModel::outletsChanged, this, &NodeItem::resetOutlets);

  connect(m_fold, &score::QGraphicsPixmapToggle::toggled, this, [this](bool b) {
    resetItem();

    m_folded = !b;
    if(!m_folded)
    {
      createContentItem();
    }
    else
    {
      createFoldedItem();
    }
    QTimer::singleShot(1, this, [this] {
      resetInlets();
      resetOutlets();

      resizeAsync();
    });
  });

  resizeAsync();
  updateTooltip();
}

void NodeItem::createWithoutDecorations()
{
  auto& process = m_model;

  con(process.selection, &Selectable::changed, this, &NodeItem::setSelected);
  m_selected = process.selection.get();

  createContentItem();

  updateTooltip();
}

void NodeItem::updateTooltip()
{
  if(!m_fx || m_fx->toolTip().isEmpty())
  {
    auto& p = this->m_context.app.interfaces<Process::ProcessFactoryList>();
    const auto& desc = p.get(m_model.concreteKey())->descriptor(m_model);

    bool has_name = !desc.prettyName.isEmpty();
    bool has_desc = !desc.description.isEmpty();
    bool has_link = !desc.documentationLink.isEmpty();
    QString str;
    if(has_name)
    {
      str += desc.prettyName;
      str += "\n";
    }
    if(has_desc)
    {
      str += desc.description;
      str += "\n";
    }
    if(has_link)
    {
      str += QStringLiteral("Press F1 to open the docs:\n%1")
                 .arg(desc.documentationLink.toString());
    }
    else
    {
      str += QStringLiteral("Press F1 to open the docs!");
    }

    this->setToolTip(str);
  }
  else if(m_fx)
  {
    this->setToolTip(m_fx->toolTip());
  }
}

void NodeItem::resetItem()
{
  qDeleteAll(m_inlets);
  m_inlets.clear();

  qDeleteAll(m_outlets);
  m_outlets.clear();

  if(m_presenter)
  {
    QPointer<Process::LayerView> oldView = static_cast<Process::LayerView*>(m_fx);
    delete m_presenter;
    m_presenter = nullptr;
    if(oldView)
      delete oldView;
    m_fx = nullptr;
    QObject::disconnect(&m_model, &Process::ProcessModel::setSize, this, nullptr);
  }
  else
  {
    delete m_fx;
    m_fx = nullptr;
  }

  m_contentSize = QSizeF{minimalContentWidth(), minimalContentHeight()};
}

void NodeItem::resetInlets()
{
  // if(!m_presenter)
  //   return;
  qDeleteAll(m_inlets);
  m_inlets.clear();

  // We don't display ports on the left & right side
  // for normal items which are supposed to handle it
  if(dynamic_cast<Process::DefaultEffectItem*>(m_fx) && !m_presenter)
    return updateTitlePos();

  const qreal x = InletX0;
  qreal y = m_label ? InletY0 : InletY0 - 10.;
  auto& portFactory = score::GUIAppContext().interfaces<Process::PortFactoryList>();
  for(Process::Inlet* port : m_model.inlets())
  {
    if(port->displayHandledExplicitly)
      continue;

    Process::PortFactory* fact = portFactory.get(port->concreteKey());
    Dataflow::PortItem* item = fact->makePortItem(*port, m_context, this, this);
    item->setPos(x, y);
    item->setZValue(10);
    m_inlets.push_back(item);

    y += PortSpacing;
  }

  updateTitlePos();
}

void NodeItem::resetOutlets()
{
  // if(!m_presenter)
  //   return;
  qDeleteAll(m_outlets);
  m_outlets.clear();

  // We don't display ports on the left & right side
  // for normal items which are supposed to handle it
  if(dynamic_cast<Process::DefaultEffectItem*>(m_fx) && !m_presenter)
    return updateTitlePos();

  const qreal x = m_contentSize.width() + OutletX0;
  qreal y = m_label ? OutletY0 : OutletY0 - 10.;

  auto& portFactory = score::AppContext().interfaces<Process::PortFactoryList>();
  for(Process::Outlet* port : m_model.outlets())
  {
    if(port->displayHandledExplicitly)
      continue;
    Process::PortFactory* fact = portFactory.get(port->concreteKey());
    auto item = fact->makePortItem(*port, m_context, this, this);
    item->setPos(x, y);
    item->setZValue(10);
    m_outlets.push_back(item);

    y += PortSpacing;
  }
}

void NodeItem::updateLabel()
{
  if(const auto& name = m_model.metadata().getName(); !name.isEmpty())
  {
    m_label->setText(name);
  }
  else if(const auto& label = m_model.metadata().getLabel(); !label.isEmpty())
  {
    m_label->setText(label);
  }
  else
  {
    m_label->setText(m_model.prettyShortName());
  }

  if(auto resizeable = dynamic_cast<score::ResizeableItem*>(m_fx))
  {
    resizeable->setMinimumWidth(minimalContentWidth());
  }
  QTimer::singleShot(2, this, &NodeItem::updateSize);
}

void NodeItem::updateTitlePos()
{
  if(!m_label)
    return;

  double x0 = FoldX0;
  if(!m_inlets.empty())
    x0 += InletX0;

  m_fold->setPos({x0, FoldY0});
  x0 += UiX0 - FoldX0 + 2.;

  if(m_scriptButton)
  {
    m_scriptButton->setPos({x0, UiY0});
    x0 += UiX0 + SpacingIcon;
  }

  if(m_uiButton)
  {
    m_uiButton->setPos({x0, UiY0});
    x0 += UiX0 + SpacingIcon;
  }

  if(m_presetButton)
  {
    m_presetButton->setPos({x0, UiY0});
    x0 += UiX0 + SpacingIcon * 2.;
  }

  m_label->setPos({x0, TitleY0});
}

QSizeF NodeItem::size() const noexcept
{
  return m_contentSize;
}

void NodeItem::setSelected(bool s)
{
  m_selected = s;
  if(s)
    setFocus();
  else
    clearFocus();

  update();
}

QRectF NodeItem::contentRect() const noexcept
{
  if(!m_label)
  {
    return {
        0, 0, m_contentSize.width() + RightSideWidth,
        m_contentSize.height() + FooterHeight};
  }
  else
  {
    double x = 0;
    double y = -TitleHeight;
    double w = m_contentSize.width();
    const double h = m_contentSize.height() + TitleHeight + FooterHeight;
    if(!m_inlets.empty())
    {
      x -= LeftSideWidth;
      w += LeftSideWidth;
    }
    if(!m_outlets.empty() || m_presenter)
    { // FIXME make the redimension handle an item instead
      w += RightSideWidth;
    }
    return {x, y, w, h};
  }
}

void NodeItem::updateContentRect()
{
  m_contentRect = contentRect().adjusted(-2., -2., 2., 2.);
}

QRectF NodeItem::boundingRect() const
{
  return m_contentRect.adjusted(-2., -2., 2., 2.);
}

void NodeItem::createFoldedItem()
{
  if(m_fx)
    return;

  auto& ctx = m_context;
  auto& model = m_model;
  // Body
  score::ResizeableItem* resizeable{};
  {
    auto fx = new Process::DefaultEffectItem{true, model, ctx, this};
    m_fx = fx;
    m_contentSize = m_fx->boundingRect().size(); // FIXME async, size is not set yet
    resizeable = fx;
  }
  setupItem(resizeable);
}

void NodeItem::createContentItem()
{
  if(m_fx)
    return;

  auto& ctx = m_context;
  auto& model = m_model;
  // Body
  auto& fact = ctx.app.interfaces<Process::LayerFactoryList>();
  score::ResizeableItem* resizeable{};
  if(auto factory = fact.findDefaultFactory(model))
  {
    if(auto fx = factory->makeItem(model, ctx, this))
    {
      m_fx = fx;
      m_contentSize = m_fx->boundingRect().size(); // FIXME async, size is not set yet
      resizeable = fx;
    }
    else if(auto fx = factory->makeLayerView(model, ctx, this))
    {
      m_fx = fx;
      m_presenter = factory->makeLayerPresenter(model, fx, ctx, this);
      m_contentSize = m_model.size();
      m_presenter->setWidth(m_contentSize.width(), m_contentSize.width());
      m_presenter->setHeight(m_contentSize.height());

      // TODO review the resizing heuristic...
      if(m_presenter)
      {
        ::bind(model, Process::ProcessModel::p_size{}, this, [this](QSizeF s) {
          if(s != m_contentSize)
            setSize(s);
        });
          connect(m_presenter, &Process::LayerPresenter::contextMenuRequested, this, [this] (QPoint pos, QPointF sp) {
              auto menu = new QMenu;
              auto& reg = score::GUIAppContext()
                              .applicationPlugin<Process::ApplicationPlugin>()
                              .layerContextMenuRegistrar();

              m_presenter->fillContextMenu(*menu, pos, sp, reg);
              if(menu->actions().size() > 0) {
                menu->exec(pos);
                menu->close();
              }

              menu->deleteLater();
          }, Qt::QueuedConnection);
      }
    }
  }

  if(!m_fx)
  {
    auto fx = new Process::DefaultEffectItem{false, model, ctx, this};
    m_fx = fx;
    m_contentSize = m_fx->boundingRect().size(); // FIXME async, size is not set yet
    resizeable = fx;
  }

  setupItem(resizeable);
}

void NodeItem::resizeAsync()
{
  updateTitlePos();
  if(auto resizeable = dynamic_cast<score::ResizeableItem*>(m_fx))
  {
    resizeable->setMinimumWidth(minimalContentWidth());
    // Needed to leave some time for defaulteffectitem
    QTimer::singleShot(2, this, [this] {
      updateSize();
      updateZoomRatio();
    });
  }
  else
  {
    QMetaObject::invokeMethod(this, [this] {
      updateSize();
      updateZoomRatio();
    });
  }
}

void NodeItem::recreate()
{
  resetItem();
  qDeleteAll(this->childItems());
  m_scriptButton = nullptr;
  m_uiButton = nullptr;
  m_presetButton = nullptr;
  m_label = nullptr;
  m_fold = nullptr;
  disconnect(&this->m_model, nullptr, this, nullptr);
  disconnect(&this->m_model.selection, nullptr, this, nullptr);
  disconnect(&this->m_model.metadata(), nullptr, this, nullptr);

  if(this->m_model.flags() & Process::ProcessFlags::FullyCustomItem)
  {
    createWithoutDecorations();
  }
  else
  {
    createWithDecorations();
  }

  initConnections();
}

void NodeItem::setupItem(score::ResizeableItem* resizeable)
{
  // Positions / size
  double w = std::max(minimalContentWidth(), m_contentSize.width());
  double h = std::max(minimalContentHeight(), m_contentSize.height());
  m_contentSize = QSizeF{w, h};

  resizeAsync();
  if(m_model.size() != m_contentSize && !m_folded)
    const_cast<Process::ProcessModel&>(m_model).setSize(m_contentSize);

  if(resizeable)
  {
    if(auto lay = dynamic_cast<score::GraphicsLayout*>(resizeable))
      connect(lay, &score::ResizeableItem::childrenSizeChanged, this, [this, lay] {
        lay->layout();
        lay->fitChildrenRect();
        m_contentSize = lay->rect().size();

        resizeAsync();
      });
    connect(resizeable, &score::ResizeableItem::sizeChanged, this, [this](QSizeF sz) {
      double w = std::max(minimalContentWidth(), sz.width());
      double h = std::max(minimalContentHeight(), sz.height());
      m_contentSize = QSizeF{w, h};

      resizeAsync();
    });
  }
}

double NodeItem::minimalContentWidth() const noexcept
{
  if(Q_UNLIKELY(!m_label))
  {
    if((m_inlets.size() + m_outlets.size()) <= 1)
      return 10.;
    else
      return 20.;
  }
  else
  {
    return std::max(75.0, m_label->pos().x() + m_label->boundingRect().width() + 6.);
  }
}
double NodeItem::minimalContentHeight() const noexcept
{
  if(Q_UNLIKELY(!m_label))
    if(std::max(m_inlets.size(), m_outlets.size()) <= 1)
      return 10.;

  double h = 22.;
  if(!m_inlets.empty())
    h = std::max(h, m_inlets.back()->y() + 10.);
  if(!m_outlets.empty())
    h = std::max(h, m_outlets.back()->y() + 10.);
  return h;
}

void NodeItem::updateSize()
{
  if(m_model.flags() & Process::ProcessFlags::FullyCustomItem)
  {
    if(m_presenter)
    {
      m_presenter->setWidth(m_contentSize.width(), m_contentSize.width());
      m_presenter->setHeight(m_contentSize.height());
    }
    QSizeF sz{10.0, 10.0};
    if(m_fx)
    {
      auto fx_sz = m_fx->boundingRect().size();
      sz.rwidth() = std::max(sz.width(), fx_sz.width());
      sz.rheight() = std::max(sz.height(), fx_sz.height());
    }
    updateContentRect();
    update();
    if(auto sc = this->scene())
      sc->update();
    return;
  }

  if(!m_label)
  {
    if(auto sc = this->scene())
      sc->update();
    return;
  }
  QSizeF sz{minimalContentWidth(), minimalContentHeight()};
  if(m_fx)
  {
    auto fx_sz = m_fx->boundingRect().size();
    sz.rwidth() = std::max(sz.width(), fx_sz.width());
    sz.rheight() = std::max(sz.height(), fx_sz.height());
  }
  //if (sz != m_contentSize || !m_fx)
  {
    prepareGeometryChange();
    if(m_fx)
    {
      m_contentSize = sz;
      if(!m_presenter)
      {
        if(m_model.outlets().size() > 0 && m_model.inlets().size() == 0)
        {
          // Align right for the case where we only have a series of outlets
          updateContentRect();
          m_fx->setPos(m_contentRect.width() - m_fx->boundingRect().width(), 0);
        }
        else
        {
          m_fx->setPos(0, 0);
        }
      }
    }

    if(m_presenter)
    {
      m_presenter->setWidth(m_contentSize.width(), m_contentSize.width());
      m_presenter->setHeight(m_contentSize.height());
    }

    if(m_scriptButton)
    {
      m_scriptButton->setParentItem(nullptr);
    }
    if(m_uiButton)
    {
      m_uiButton->setParentItem(nullptr);
    }

    for(auto& outlet : m_outlets)
    {
      outlet->setPos(m_contentSize.width() + OutletX0, outlet->pos().y());
    }

    if(m_scriptButton)
    {
      m_scriptButton->setParentItem(this);
    }
    if(m_uiButton)
    {
      m_uiButton->setParentItem(this);
    }
    updateTitlePos();
    updateContentRect();
    update();
    if(auto sc = this->scene())
      sc->update();
  }
}

void NodeItem::setSize(QSizeF sz)
{
  if(m_presenter)
  {
    // TODO: find a way to indicate to a model what will be the size of the
    // item
    //      - maybe set it without a command in that case ?
    prepareGeometryChange();
    m_contentSize = sz;

    if(m_scriptButton)
    {
      m_scriptButton->setParentItem(nullptr);
    }
    if(m_uiButton)
    {
      m_uiButton->setParentItem(nullptr);
    }

    m_presenter->setWidth(sz.width(), sz.width());
    m_presenter->setHeight(sz.height());
    updateZoomRatio();

    resetInlets();
    resetOutlets();
    auto cur_x = m_contentSize.width() + TopButtonX0;
    if(m_scriptButton)
    {
      m_scriptButton->setParentItem(this);
      m_scriptButton->setPos({cur_x, TopButtonY0});
      cur_x += UiX0 + SpacingIcon;
    }
    if(m_uiButton)
    {
      m_uiButton->setParentItem(this);
      m_uiButton->setPos({cur_x, TopButtonY0});
    }
  }
  if(auto resizeable = dynamic_cast<score::ResizeableItem*>(m_fx))
  {
    resizeable->setMinimumWidth(sz.width());
  }
  updateContentRect();
  update();
  if(auto sc = this->scene())
    sc->update();
}

const Id<Process::ProcessModel>& NodeItem::id() const noexcept
{
  return m_model.id();
}

NodeItem::~NodeItem()
{
  delete m_presenter;
}

void NodeItem::setParentDuration(TimeVal r)
{
  if(r != m_parentDuration)
  {
    m_parentDuration = r;
    updateZoomRatio();
  }
}

void NodeItem::setPlayPercentage(float f, TimeVal parent_dur)
{
  if(this->m_model.flags() & Process::ProcessFlags::FullyCustomItem)
    return;

  if(m_playPercentage != f)
  {
    // Comes from the interval -> if we are looping we make a small modulo of it
    if(m_model.loops())
    {
      auto loopDur = m_model.loopDuration().impl;
      double playdur = double(f) * parent_dur.impl;
      f = std::fmod(playdur, loopDur) / loopDur;
      if(m_playPercentage != f)
      {
        m_playPercentage = f;
        update({12., -1., (m_contentSize.width() - 24.), 3.});
      }
    }
    else
    {
      f = ossia::clamp(f, 0.f, 1.f);
      if(m_playPercentage != f)
      {
        m_playPercentage = f;
        update({12., -1., (m_contentSize.width() - 24.) * f + 1., 3.});
      }
    }
  }
}

qreal NodeItem::width() const noexcept
{
  return m_contentSize.width();
}

qreal NodeItem::height() const
{
  return TitleHeight + m_contentSize.height() + FooterHeight;
}

void NodeItem::initConnections()
{
  auto& process = this->model();
  ::bind(process, Process::ProcessModel::p_position{}, this, [this](QPointF p) {
    if(p != pos())
      setPos(p);
  });

  auto on_sizeChanged = [this] {
    m_needResize = true;
    QTimer::singleShot(1, this, [this] {
      if(!m_needResize)
        return;
      m_needResize = false;
      resizeAsync();
    });
  };
  connect(&process, &Process::ProcessModel::controlAdded, this, on_sizeChanged);
  connect(&process, &Process::ProcessModel::controlRemoved, this, on_sizeChanged);
  connect(&process, &Process::ProcessModel::controlOutletAdded, this, on_sizeChanged);
  connect(&process, &Process::ProcessModel::controlOutletRemoved, this, on_sizeChanged);
  connect(&process, &Process::ProcessModel::inletsChanged, this, on_sizeChanged);
  connect(&process, &Process::ProcessModel::outletsChanged, this, on_sizeChanged);
  connect(&process, &Process::ProcessModel::flagsChanged, this, &NodeItem::recreate);
}

const ProcessModel& NodeItem::model() const noexcept
{
  return m_model;
}

void NodeItem::updateZoomRatio() const noexcept
{
  const auto dur = m_model.loops() ? m_model.loopDuration() : m_parentDuration;
  if(m_presenter)
  {
    m_presenter->on_zoomRatioChanged(dur.impl / m_contentSize.width());
    // TODO investigate why this is necessary for scenario:
    m_presenter->parentGeometryChanged();
  }
}

bool NodeItem::isInSelectionCorner(QPointF p, QRectF r) const
{
  return (p.x() - r.x()) > (r.width() - 10.) && (p.y() - r.y()) > (r.height() - 10.);
}

void NodeItem::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& style = Process::Style::instance();
  const auto& skin = style.skin;
  const auto rect = m_contentRect;
  //painter->fillRect(boundingRect(), Qt::red);
  //return;
  const auto flags = this->m_model.flags();

  const auto& bset = skin.Emphasis5;
  const auto& fillbrush = !(flags & Process::ProcessFlags::NodeHasNoBackground)
                              ? skin.Emphasis5
                              : skin.TransparentBrush;
  const auto& brush = m_selected ? skin.Base2.darker
                      : m_hover  ? bset.lighter
                                 : bset.main;
  const auto& pen = m_dropping ? skin.Warn2.main.pen3_solid_round_round
                               : brush.pen2_solid_round_round;

  painter->setRenderHint(QPainter::Antialiasing, true);

  // Body
  painter->setPen(pen);

#if defined(SCORE_DEBUG_REDRAWS)
  {
    QColor c;
    c.setRedF(0.5 * double(rand()) / RAND_MAX);
    c.setGreenF(0.5 * double(rand()) / RAND_MAX);
    c.setBlueF(0.5 * double(rand()) / RAND_MAX);
    c.setAlphaF(1.);
    painter->setBrush(c);
  }
#else
  painter->setBrush(fillbrush);
#endif

  painter->drawRoundedRect(rect, Corner, Corner);

  painter->setRenderHint(QPainter::Antialiasing, false);

  // Exec
  if(!(flags & Process::ProcessFlags::FullyCustomItem))
    if(m_playPercentage > 0.)
    {
      painter->setPen(style.IntervalPlayFill().main.pen1_solid_flat_miter);
      painter->drawLine(
          QPointF{12., 0.}, QPointF{12. + (width() - 24.) * m_playPercentage, 0.});
    }

  // Resizing handle
  if(m_presenter)
  {
    const auto h = m_contentSize.height();
    const auto w = m_contentSize.width()
                   + ((!m_outlets.empty() || m_presenter) ? RightSideWidth : 0);
    painter->setPen(style.IntervalWarning().main.pen0_solid_round);
    double start_x = w - 6.;
    double start_y = h - 6.;
    double center_x = w - 2.;
    double center_y = h - 2.;
    painter->drawLine(start_x, center_y, center_x, center_y);
    painter->drawLine(center_x, start_y, center_x, center_y);
  }
}

namespace
{
enum Interaction
{
  None,
  Move,
  Resize
} nodeItemInteraction{};
QSizeF origNodeSize{};
bool nodeDidMove{};
}

void NodeItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  nodeItemInteraction = Interaction::None;
  if(event->button() == Qt::LeftButton)
  {
    nodeDidMove = false;
    if(m_presenter && isInSelectionCorner(event->pos(), m_contentRect))
    {
      nodeItemInteraction = Interaction::Resize;
      origNodeSize = m_model.size();
    }
    else
    {
      nodeItemInteraction = Interaction::Move;
    }

    if(m_presenter && m_model.flags() & Process::ProcessFlags::ItemRequiresUniqueFocus)
    {
      m_context.focusDispatcher.focus(m_presenter);
    }
    score::SelectionDispatcher{m_context.selectionStack}.select(m_model);
    event->accept();
  }
  else
  {
    return QGraphicsItem::mousePressEvent(event);
  }
}
namespace
{
static std::vector<const Process::ProcessModel*>
selectedProcesses(const score::DocumentContext& ctx)
{
  std::vector<const Process::ProcessModel*> ps;
  auto sel = ctx.selectionStack.currentSelection();
  ps.reserve(sel.size());
  for(auto& obj : sel)
    if(auto p = qobject_cast<Process::ProcessModel*>(obj))
    {
      ps.push_back(p);
    }
  return ps;
}
}
void NodeItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if(nodeItemInteraction == Interaction::None)
    return QGraphicsItem::mouseMoveEvent(event);

  auto parent = this->parentItem();
  auto origp = parent->mapFromScene(event->buttonDownScenePos(Qt::LeftButton));
  auto p = parent->mapFromScene(event->scenePos());
  if(p != origp)
    nodeDidMove = true;

  if(nodeDidMove)
  {
    switch(nodeItemInteraction)
    {
      case Interaction::Resize: {
        const auto sz = origNodeSize + QSizeF{p.x() - origp.x(), p.y() - origp.y()};
        m_dispatcher.submit<Process::ResizeNode>(
            m_model, sz.expandedTo({minimalContentWidth(), minimalContentHeight()}));
        updateSize();
        break;
      }
      case Interaction::Move: {
        m_dispatcher.submit<Process::MoveNodes>(selectedProcesses(m_context), p - origp);
        break;
      }
      default:
        break;
    }
  }
  event->accept();
}

void NodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if(nodeItemInteraction == Interaction::None)
    return QGraphicsItem::mouseReleaseEvent(event);

  mouseMoveEvent(event);

  if(nodeDidMove)
    m_dispatcher.commit<Process::MoveNodesMacro>();
  nodeDidMove = false;
  event->accept();
}

void NodeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  event->accept();
}

void NodeItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  if(m_presenter && isInSelectionCorner(event->pos(), m_contentRect))
  {
    auto& skin = score::Skin::instance();
    setCursor(skin.CursorScaleFDiag);
  }
  else
  {
    unsetCursor();
  }

  m_hover = true;
  update();
  event->accept();
}

void NodeItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
  if(m_presenter && isInSelectionCorner(event->pos(), m_contentRect))
  {
    auto& skin = score::Skin::instance();
    setCursor(skin.CursorScaleFDiag);
  }
  else
  {
    unsetCursor();
  }
  event->accept();
}

void NodeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  unsetCursor();

  m_hover = false;
  update();
  event->accept();
}

void NodeItem::keyPressEvent(QKeyEvent* event)
{
  if(event->key() == Qt::Key_Down)
  {
    auto ports = Dataflow::getProcessPorts(this->m_model);
    if(!ports.empty())
    {
      score::SelectionDispatcher disp{m_context.selectionStack};
      disp.select(*ports.front());
    }
  }
  event->accept();
}

void NodeItem::keyReleaseEvent(QKeyEvent* event)
{
  event->accept();
}
void NodeItem::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  m_dropping = true;
  event->accept();
  update();
}

void NodeItem::dragMoveEvent(QGraphicsSceneDragDropEvent* event)
{
  m_dropping = true;
  event->accept();
  update();
}

void NodeItem::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  m_dropping = false;
  event->accept();
  update();
}

void NodeItem::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  m_dropping = false;
  event->accept();
  dropReceived(event->pos(), *event->mimeData());
  update();
}
}
