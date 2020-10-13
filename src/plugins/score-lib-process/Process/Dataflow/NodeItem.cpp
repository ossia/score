#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/NodeItem.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Process/Process.hpp>
#include <Process/Style/Pixmaps.hpp>

#include <QPainter>

#include <Effect/EffectLayer.hpp>



#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/graphics/TextItem.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/tools/Bind.hpp>
#include <score/graphics/GraphicWidgets.hpp>
#include <score/graphics/TextItem.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Bind.hpp>

#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectLayer.hpp>
#include <Effect/EffectPainting.hpp>
#include <wobjectimpl.h>

namespace Process
{

void NodeItem::resetInlets()
{
  qDeleteAll(m_inlets);
  m_inlets.clear();
  qreal x = InletX0;
  auto& portFactory = score::GUIAppContext().interfaces<Process::PortFactoryList>();
  for (Process::Inlet* port : m_model.inlets())
  {
    if (port->hidden)
      continue;
    Process::PortFactory* fact = portFactory.get(port->concreteKey());
    Dataflow::PortItem* item = fact->makeItem(*port, m_context, this, this);
    item->setPos(x, InletY0);
    m_inlets.push_back(item);

    x += PortSpacing;
  }

  m_label->setPos(QPointF{x + 2., 0.});
}

void NodeItem::resetOutlets()
{
  qDeleteAll(m_outlets);
  m_outlets.clear();
  qreal x = OutletX0;
  const qreal h = boundingRect().height() + OutletY0;
  auto& portFactory = score::AppContext().interfaces<Process::PortFactoryList>();
  for (Process::Outlet* port : m_model.outlets())
  {
    if (port->hidden)
      continue;
    Process::PortFactory* fact = portFactory.get(port->concreteKey());
    auto item = fact->makeItem(*port, m_context, this, this);
    item->setPos(x, h);
    m_outlets.push_back(item);

    x += PortSpacing;
  }
}

QSizeF NodeItem::size() const noexcept { return m_contentSize; }

void NodeItem::setSelected(bool s)
{
  if (m_selected != s)
  {
    m_selected = s;
    if (s)
      setFocus();

    update();
  }
}

QRectF NodeItem::boundingRect() const
{
  return {0., 0., m_contentSize.width(), TitleHeight + m_contentSize.height() + FooterHeight};
}

void NodeItem::paintNode(QPainter* painter, bool selected, bool hovered, QRectF rect)
{
  const auto& skin = score::Skin::instance();

  const auto& bset = skin.Emphasis5;
  const auto& fillbrush = skin.Emphasis5;
  const auto& brush = selected ? skin.Base2.darker : hovered ? bset.lighter : bset.main;
  const auto& pen = brush.pen2_solid_round_round;

  painter->setRenderHint(QPainter::Antialiasing, true);

  // Body
  painter->setPen(pen);
  painter->setBrush(fillbrush);

  painter->drawRoundedRect(rect, Corner, Corner);

  painter->setRenderHint(QPainter::Antialiasing, false);

  const auto h = rect.height();
  const auto w = rect.width();
  // Header
  painter->setBrush(brush.brush);
  painter->fillRect(QRectF{1., 0., w - 2., TitleHeight}, brush.brush);

  // Footer
  painter->fillRect(QRectF{1., h - FooterHeight, w - 2., FooterHeight}, brush.brush);
  // painter->setPen(Qt::green);
  // painter->setBrush(Qt::transparent);
  // painter->drawRect(rect);
}

void NodeItem::createContentItem()
{
  if(m_fx)
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
      connect(fx, &score::ResizeableItem::sizeChanged, this, &NodeItem::updateSize);
    }
    else if (auto fx = factory->makeLayerView(model, ctx, this))
    {
      m_fx = fx;
      m_presenter = factory->makeLayerPresenter(model, fx, ctx, this);
      m_contentSize = m_model.size();
      m_presenter->setWidth(m_contentSize.width(), m_contentSize.width());
      m_presenter->setHeight(m_contentSize.height());
      m_presenter->on_zoomRatioChanged(1.);
      m_presenter->parentGeometryChanged();
    }
  }

  if (!m_fx)
  {
    m_fx = new Process::DefaultEffectItem{model, ctx, this};
    m_contentSize = m_fx->boundingRect().size();
  }

  // Positions / size
  m_fx->setPos({0, TitleHeight});

  m_contentSize
      = QSizeF{std::max(100., m_contentSize.width()), std::max(10., m_contentSize.height())};

  if (m_ui)
  {
    m_ui->setPos({m_contentSize.width() + TopButtonX0, TopButtonY0});
  }
}

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

  // Title
  m_ui = Process::makeExternalUIButton(process, ctx, this, this);

  auto& skin = score::Skin::instance();
  m_label = new score::SimpleTextItem{skin.Light.main, this};

  if (const auto& label = process.metadata().getLabel(); !label.isEmpty())
    m_label->setText(label);
  else
    m_label->setText(process.prettyShortName());

  con(process.metadata(), &score::ModelMetadata::LabelChanged, this, [&](const QString& label) {
    if (!label.isEmpty())
      m_label->setText(label);
    else
      m_label->setText(process.prettyShortName());
  });

  m_label->setFont(skin.Bold10Pt);
  m_label->setPos({30., 0});

  // Selection
  con(process.selection, &Selectable::changed, this, &NodeItem::setSelected);

  createContentItem();

  resetInlets();
  resetOutlets();
  connect(&process, &Process::ProcessModel::inletsChanged, this, &NodeItem::resetInlets);
  connect(&process, &Process::ProcessModel::outletsChanged, this, &NodeItem::resetOutlets);

  const auto& pixmaps = Process::Pixmaps::instance();
  m_fold = new score::QGraphicsPixmapToggle{
      pixmaps.unroll_small, pixmaps.roll_small,  this};
  m_fold->setPos({0, 0});
  m_fold->setState(true);

  connect(m_fold, &score::QGraphicsPixmapToggle::toggled, this, [=] (bool b) {
    if(b)
    {
      createContentItem();
    }
    else
    {
      delete m_fx;
      m_fx = nullptr;
      double port_w = std::max(m_inlets.size() * 12., m_outlets.size() * 12.);
      m_contentSize = QSizeF{30 + m_label->boundingRect().width() + port_w, 0};
    }
    updateSize();
  });

  updateSize();

  ::bind(process, Process::ProcessModel::p_position{}, this, [this](QPointF p) {
    if (p != pos())
      setPos(p);
  });

  // TODO review the resizing heuristic...
  if (m_presenter)
  {
    ::bind(process, Process::ProcessModel::p_size{}, this, [this](QSizeF s) {
      if (s != m_contentSize)
        setSize(s);
    });
  }
}

void NodeItem::updateSize()
{
  auto sz = m_fx ? m_fx->boundingRect().size() : QSizeF{100, 0};
  if (sz != m_contentSize || !m_fx)
  {
    prepareGeometryChange();
    if(m_fx)
    {
      m_contentSize = QSizeF{std::max(100., sz.width()), std::max(10., sz.height())};
    }

    if (m_ui)
    {
      m_ui->setParentItem(nullptr);
    }

    const auto r = boundingRect();

    for (auto& outlet : m_outlets)
    {
      outlet->setPos(outlet->pos().x(), r.height() + OutletY0);
    }

    if (m_ui)
    {
      m_ui->setParentItem(this);
      m_ui->setPos({m_contentSize.width() + TopButtonX0, TopButtonY0});
    }
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

    if (m_ui)
    {
      m_ui->setParentItem(nullptr);
    }

    m_presenter->setWidth(sz.width(), sz.width());
    m_presenter->setHeight(sz.height());
    m_presenter->on_zoomRatioChanged(m_ratio / sz.width());
    m_presenter->parentGeometryChanged();

    resetInlets();
    resetOutlets();
    if (m_ui)
    {
      m_ui->setParentItem(this);
      m_ui->setPos({m_contentSize.width() + TopButtonX0, TopButtonY0});
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

void NodeItem::setZoomRatio(ZoomRatio r)
{
  if (m_presenter)
  {
    if (r != m_ratio)
    {
      m_ratio = r;
      m_presenter->on_zoomRatioChanged(m_ratio / m_contentSize.width());
      // TODO investigate why this is necessary for scenario:
      m_presenter->parentGeometryChanged();
    }
  }
}

void NodeItem::setPlayPercentage(float f)
{
  m_playPercentage = f;
  update({0., 14., m_contentSize.width() * f, 14.});
}

qreal NodeItem::width() const noexcept { return m_contentSize.width(); }

qreal NodeItem::height() const { return TitleHeight + m_contentSize.height() + FooterHeight; }

const ProcessModel& NodeItem::model() const noexcept { return m_model; }

bool NodeItem::isInSelectionCorner(QPointF p, QRectF r) const
{
  return p.x() > r.width() - 10. && p.y() > r.height() - 10.;
}

void NodeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  paintNode(painter, m_selected, m_hover, boundingRect());

  if (m_presenter)
  {
    auto& style = Process::Style::instance();
    const auto h = height();
    const auto w = width();
    painter->setPen(style.IntervalWarning().main.pen0_solid_round);
    painter->drawLine(w - 8., h - 3., w - 3., h - 3.);
    painter->drawLine(w - 3., h - 8., w - 3., h - 3.);
  }

  // Exec
  if (m_playPercentage != 0.)
  {
    auto& style = Process::Style::instance();
    painter->setPen(style.IntervalPlayFill().main.pen1_solid_flat_miter);
    painter->drawLine(QPointF{0., 14.}, QPointF{width() * m_playPercentage, 14.});
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
}

void NodeItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
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

  score::SelectionDispatcher{m_context.selectionStack}.setAndCommit({&m_model});
  event->accept();
}

void NodeItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  auto origp = mapToItem(parentItem(), event->buttonDownPos(Qt::LeftButton));
  auto p = mapToItem(parentItem(), event->pos());
  switch (nodeItemInteraction)
  {
    case Interaction::Resize:
    {
      const auto sz = origNodeSize + QSizeF{p.x() - origp.x(), p.y() - origp.y()};
      m_context.dispatcher.submit<Process::ResizeNode>(m_model, sz.expandedTo({10, 10}));
      break;
    }
    case Interaction::Move:
    {
      m_context.dispatcher.submit<Process::MoveNode>(m_model, m_model.position() + (p - origp));
      break;
    }
  }
  event->accept();
}

void NodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  mouseMoveEvent(event);
  m_context.dispatcher.commit();
  event->accept();
}

void NodeItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  if (isInSelectionCorner(event->pos(), boundingRect()))
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
  if (isInSelectionCorner(event->pos(), boundingRect()))
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
