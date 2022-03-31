#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectLayer.hpp>
#include <Effect/EffectPainting.hpp>
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

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/graphics/GraphicWidgets.hpp>
#include <score/graphics/TextItem.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/model/Skin.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/tools/Bind.hpp>

#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QTimer>

#include <wobjectimpl.h>

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
    const Process::ProcessModel& process,
    const Process::Context& ctx,
    QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_model{process}
    , m_context{ctx}
{
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
  setFlag(ItemIsFocusable, true);
  setFlag(ItemClipsChildrenToShape, true);

  if(process.flags() & Process::ProcessFlags::FullyCustomItem)
  {
    createWithoutDecorations();
  }
  else
  {
    createWithDecorations();
  }

  ::bind(
      process, Process::ProcessModel::p_position{}, this, [this](QPointF p) {
        if (p != pos())
          setPos(p);
      });
}

void NodeItem::createWithDecorations()
{
  auto& process = m_model;
  auto& ctx = m_context;

  const bool startFolded = (process.inlets().size() > 12);

  // Title
  const auto& pixmaps = Process::Pixmaps::instance();
  m_fold = new score::QGraphicsPixmapToggle{
      pixmaps.unroll_small, pixmaps.roll_small, this};
  m_fold->setState(!startFolded);

  m_uiButton = Process::makeExternalUIButton(process, ctx, this, this);
  m_presetButton = Process::makePresetButton(process, ctx, this, this);

  auto& skin = score::Skin::instance();
  m_label = new score::SimpleTextItem{skin.Light.main, this};

  if (const auto& label = process.metadata().getLabel(); !label.isEmpty())
    m_label->setText(label);
  else  if (const auto& name = process.metadata().getName(); !name.isEmpty())
    m_label->setText(name);
  else
    m_label->setText(process.prettyShortName());

  con(process.metadata(),
      &score::ModelMetadata::NameChanged,
      this,
      [&](const QString& label) {
        if (!label.isEmpty())
          m_label->setText(label);
        else
          m_label->setText(process.prettyShortName());
      });

  con(process.metadata(),
      &score::ModelMetadata::LabelChanged,
      this,
      [&](const QString& label) {
        if (!label.isEmpty())
          m_label->setText(label);
        else
          m_label->setText(process.prettyShortName());
      });
  con(process, &Process::ProcessModel::loopsChanged, this, [&] {
    updateZoomRatio();
  });
  con(process, &Process::ProcessModel::loopDurationChanged, this, [&] {
    updateZoomRatio();
  });
  con(process, &Process::ProcessModel::startOffsetChanged, this, [&] {
    updateZoomRatio();
  });

  m_label->setFont(skin.Bold10Pt);

  // Selection
  con(process.selection, &Selectable::changed, this, &NodeItem::setSelected);
  m_selected = process.selection.get();

  if(!startFolded)
    createContentItem();
  else
    m_contentSize = QSizeF{minimalContentWidth(), minimalContentHeight()};

  resetInlets();
  resetOutlets();
  connect(
      &process,
      &Process::ProcessModel::inletsChanged,
      this,
      &NodeItem::resetInlets);
  connect(
      &process,
      &Process::ProcessModel::outletsChanged,
      this,
      &NodeItem::resetOutlets);

  connect(m_fold, &score::QGraphicsPixmapToggle::toggled, this, [=](bool b) {
    if (b)
    {
      createContentItem();
    }
    else
    {
      if (m_presenter)
      {
        QPointer<Process::LayerView> oldView
            = static_cast<Process::LayerView*>(m_fx);
        delete m_presenter;
        m_presenter = nullptr;
        if (oldView)
          delete oldView;
        m_fx = nullptr;
        QObject::disconnect(&m_model, &Process::ProcessModel::setSize,
                            this, nullptr);
      }
      else
      {
        delete m_fx;
        m_fx = nullptr;
      }

      m_contentSize = QSizeF{minimalContentWidth(), minimalContentHeight()};
    }
    QTimer::singleShot(1, this, [this] { updateSize(); });
  });

  updateSize();
}

void NodeItem::createWithoutDecorations()
{
  auto& process = m_model;

  con(process.selection, &Selectable::changed, this, &NodeItem::setSelected);
  m_selected = process.selection.get();

  createContentItem();
}

void NodeItem::resetInlets()
{
  qDeleteAll(m_inlets);
  m_inlets.clear();
  const qreal x = InletX0;
  qreal y = m_label ? InletY0 : InletY0 - 10.;
  auto& portFactory
      = score::GUIAppContext().interfaces<Process::PortFactoryList>();
  for (Process::Inlet* port : m_model.inlets())
  {
    if (port->hidden)
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

void NodeItem::updateTitlePos()
{
  if(!m_label)
    return;

  double x0 = FoldX0;
  if (!m_inlets.empty())
    x0 += InletX0;

  m_fold->setPos({x0, FoldY0});
  x0 += UiX0 - FoldX0 + 2.;

  if (m_uiButton) {
    m_uiButton->setPos({x0, UiY0});
    x0 += UiX0 + SpacingIcon;
  }

  if(m_presetButton) {
    m_presetButton->setPos({x0, UiY0});
    x0 += UiX0 + SpacingIcon*2.;
  }

  m_label->setPos({x0, TitleY0});
}

void NodeItem::resetOutlets()
{
  qDeleteAll(m_outlets);
  m_outlets.clear();
  const qreal x = m_contentSize.width() + OutletX0;
  qreal y = m_label ? OutletY0 : OutletY0 - 10.;

  auto& portFactory
      = score::AppContext().interfaces<Process::PortFactoryList>();
  for (Process::Outlet* port : m_model.outlets())
  {
    if (port->hidden)
      continue;
    Process::PortFactory* fact = portFactory.get(port->concreteKey());
    auto item = fact->makePortItem(*port, m_context, this, this);
    item->setPos(x, y);
    item->setZValue(10);
    m_outlets.push_back(item);

    y += PortSpacing;
  }
}

QSizeF NodeItem::size() const noexcept
{
  return m_contentSize;
}

void NodeItem::setSelected(bool s)
{
  m_selected = s;
  if (s)
    setFocus();
  else
    clearFocus();

  update();
}

QRectF NodeItem::boundingRect() const
{
  if(!m_label)
  {
    return {0, 0, m_contentSize.width() + RightSideWidth, m_contentSize.height() + FooterHeight};
  }
  else
  {
    double x = 0;
    double y = -TitleHeight;
    double w = m_contentSize.width();
    const double h = m_contentSize.height() + TitleHeight + FooterHeight;
    if (!m_inlets.empty())
    {
      x -= LeftSideWidth;
      w += LeftSideWidth;
    }
    if(!m_outlets.empty() || m_presenter) // FIXME make the redimension handle an item instead
    {
      w += RightSideWidth;
    }
    return {x, y, w, h};
  }
}

void NodeItem::createContentItem()
{
  if (m_fx)
    return;

  auto& ctx = m_context;
  auto& model = m_model;
  // Body
  auto& fact = ctx.app.interfaces<Process::LayerFactoryList>();
  if (auto factory = fact.findDefaultFactory(model))
  {
    if (auto fx = factory->makeItem(model, ctx, this))
    {
      m_fx = fx;
      m_contentSize = m_fx->boundingRect().size();
      connect(
          fx,
          &score::ResizeableItem::sizeChanged,
          this,
          &NodeItem::updateSize);
    }
    else if (auto fx = factory->makeLayerView(model, ctx, this))
    {
      m_fx = fx;
      m_presenter = factory->makeLayerPresenter(model, fx, ctx, this);
      m_contentSize = m_model.size();
      m_presenter->setWidth(m_contentSize.width(), m_contentSize.width());
      m_presenter->setHeight(m_contentSize.height());

      // TODO review the resizing heuristic...
      if (m_presenter)
      {
        ::bind(model, Process::ProcessModel::p_size{}, this, [this](QSizeF s) {
          if (s != m_contentSize)
            setSize(s);
        });
      }
    }
  }

  if (!m_fx)
  {
    m_fx = new Process::DefaultEffectItem{model, ctx, this};
    m_contentSize = m_fx->boundingRect().size();
  }

  if(m_fx->toolTip().isEmpty())
  {
    auto& p = this->m_context.app.interfaces<Process::ProcessFactoryList>();
    const auto& desc = p.get(m_model.concreteKey())->descriptor({});
    this->setToolTip(QString("%1\n%2").arg(desc.prettyName, desc.description));
  }
  else
  {
    this->setToolTip(m_fx->toolTip());
  }

  // Positions / size
  m_fx->setPos({0, 0});

  double w = std::max(minimalContentWidth(), m_contentSize.width());
  double h = std::max(minimalContentHeight(), m_contentSize.height());
  m_contentSize = QSizeF{w, h};

  updateSize();
  updateZoomRatio();
  updateTitlePos();

  if(m_model.size() != m_contentSize)
    const_cast<Process::ProcessModel&>(m_model).setSize(m_contentSize);
}

double NodeItem::minimalContentWidth() const noexcept
{
  if(Q_UNLIKELY(!m_label))
    return 30.;
  else
    return std::max(75.0, TitleWithUiX0 + m_label->boundingRect().width() + 6.);
}
double NodeItem::minimalContentHeight() const noexcept
{
  double h = 22.;
  if(!m_inlets.empty())
    h = std::max(h, m_inlets.back()->y() + 10.);
  if(!m_outlets.empty())
    h = std::max(h, m_outlets.back()->y() + 10.);
  return h;
}

void NodeItem::updateSize()
{
  if(!m_label)
    return;

  auto sz = m_fx ? m_fx->boundingRect().size() : QSizeF{minimalContentWidth(), minimalContentHeight()};

  //if (sz != m_contentSize || !m_fx)
  {
    prepareGeometryChange();
    if (m_fx)
    {
      double w = std::max(minimalContentWidth(), sz.width());
      double h = std::max(minimalContentHeight(), sz.height());
      m_contentSize = QSizeF{w, h};
    }

    if (m_uiButton)
    {
      m_uiButton->setParentItem(nullptr);
    }

    for (auto& outlet : m_outlets)
    {
      outlet->setPos(m_contentSize.width() + OutletX0, outlet->pos().y());
    }

    if (m_uiButton)
    {
      m_uiButton->setParentItem(this);
    }
    updateTitlePos();
    update();
  }
}

void NodeItem::setSize(QSizeF sz)
{
  if (m_presenter)
  {
    // TODO: find a way to indicate to a model what will be the size of the
    // item
    //      - maybe set it without a command in that case ?
    prepareGeometryChange();
    m_contentSize = sz;

    if (m_uiButton)
    {
      m_uiButton->setParentItem(nullptr);
    }

    m_presenter->setWidth(sz.width(), sz.width());
    m_presenter->setHeight(sz.height());
    updateZoomRatio();

    resetInlets();
    resetOutlets();
    if (m_uiButton)
    {
      m_uiButton->setParentItem(this);
      m_uiButton->setPos({m_contentSize.width() + TopButtonX0, TopButtonY0});
    }
  }
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
  if (r != m_parentDuration)
  {
    m_parentDuration = r;
    updateZoomRatio();
  }
}

void NodeItem::setPlayPercentage(float f, TimeVal parent_dur)
{
  // Comes from the interval -> if we are looping we make a small modulo of it
  if (m_model.loops())
  {
    auto loopDur = m_model.loopDuration().impl;
    double playdur = f * parent_dur.impl;
    f = std::fmod(playdur, loopDur) / loopDur;
  }
  m_playPercentage = f;
  update({0., 14., m_contentSize.width() * f, 14.});
}

qreal NodeItem::width() const noexcept
{
  return m_contentSize.width();
}

qreal NodeItem::height() const
{
  return TitleHeight + m_contentSize.height() + FooterHeight;
}

const ProcessModel& NodeItem::model() const noexcept
{
  return m_model;
}

void NodeItem::updateZoomRatio() const noexcept
{
  const auto dur = m_model.loops() ? m_model.loopDuration() : m_parentDuration;
  if (m_presenter)
  {
    m_presenter->on_zoomRatioChanged(dur.impl / m_contentSize.width());
    // TODO investigate why this is necessary for scenario:
    m_presenter->parentGeometryChanged();
  }
}

bool NodeItem::isInSelectionCorner(QPointF p, QRectF r) const
{
  return (p.x() - r.x()) > (r.width() - 10.)
         && (p.y() - r.y()) > (r.height() - 10.);
}

void NodeItem::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  auto& style = Process::Style::instance();
  const auto& skin = style.skin;
  const auto rect = boundingRect();
  //painter->fillRect(boundingRect(), Qt::red);
  // return;

  const auto& bset = skin.Emphasis5;
  const auto& fillbrush = skin.Emphasis5;
  const auto& brush = m_selected ? skin.Base2.darker
                      : m_hover  ? bset.lighter
                                 : bset.main;
  const auto& pen = brush.pen2_solid_round_round;

  painter->setRenderHint(QPainter::Antialiasing, true);

  // Body
  painter->setPen(pen);
  painter->setBrush(fillbrush);

  painter->drawRoundedRect(rect, Corner, Corner);

  painter->setRenderHint(QPainter::Antialiasing, false);

  // Exec
  if (m_playPercentage != 0.)
  {
    painter->setPen(style.IntervalPlayFill().main.pen1_solid_flat_miter);
    painter->drawLine(
        QPointF{0., 0.}, QPointF{width() * m_playPercentage, 0.});
  }

  // Resizing handle
  if (m_presenter)
  {
    const auto h = m_contentSize.height();
    const auto w = m_contentSize.width() + ((!m_outlets.empty() || m_presenter) ? RightSideWidth : 0);
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
  Move,
  Resize
} nodeItemInteraction{};
QSizeF origNodeSize{};
bool nodeDidMove{};
}

void NodeItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  nodeDidMove = false;
  if (m_presenter && isInSelectionCorner(event->pos(), boundingRect()))
  {
    nodeItemInteraction = Interaction::Resize;
    origNodeSize = m_model.size();
  }
  else
  {
    nodeItemInteraction = Interaction::Move;
  }

  if (m_presenter)
    m_context.focusDispatcher.focus(m_presenter);

  score::SelectionDispatcher{m_context.selectionStack}.select(m_model);
  event->accept();
}
namespace
{
static std::vector<const Process::ProcessModel*> selectedProcesses(const score::DocumentContext& ctx)
{
  std::vector<const Process::ProcessModel*> ps;
  auto sel = ctx.selectionStack.currentSelection();
  ps.reserve(sel.size());
  for(auto& obj : sel)
    if(auto p = qobject_cast<Process::ProcessModel*>(obj))
      ps.push_back(p);
  return ps;
}
}
void NodeItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  auto origp = event->buttonDownScenePos(Qt::LeftButton);
  auto p = event->scenePos();
  if(p != origp)
    nodeDidMove = true;

  if(nodeDidMove)
  {
    switch (nodeItemInteraction)
    {
      case Interaction::Resize:
      {
        const auto sz
            = origNodeSize + QSizeF{p.x() - origp.x(), p.y() - origp.y()};
        m_context.dispatcher.submit<Process::ResizeNode>(
            m_model, sz.expandedTo({minimalContentWidth(), minimalContentHeight()}));
        updateSize();
        break;
      }
      case Interaction::Move:
      {
        m_context.dispatcher.submit<Process::MoveNodes>(
            selectedProcesses(m_context), p - origp);
        break;
      }
    }
  }
  event->accept();
}

void NodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  mouseMoveEvent(event);
  if(nodeDidMove)
    m_context.dispatcher.commit();
  nodeDidMove = false;
  event->accept();
}

void NodeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  event->accept();
}

void NodeItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  if (m_presenter && isInSelectionCorner(event->pos(), boundingRect()))
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
  if (m_presenter && isInSelectionCorner(event->pos(), boundingRect()))
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
}
