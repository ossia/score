#include "ChainItem.hpp"

#include <Media/AudioChain/AudioChainLayer.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Scenario/Application/Menus/ScenarioCopy.hpp>

#include <score/model/path/PathSerialization.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/tools/std/Optional.hpp>

#include <QApplication>
#include <QDrag>

#include <Control/DefaultEffectItem.hpp>
namespace Media
{
EffectItem::EffectItem(
    const View& view,
    const Process::ProcessModel& effect,
    const Process::LayerContext& ctx,
    const Process::LayerFactoryList& fact,
    QGraphicsItem* parent)
    : ::Effect::ItemBase{effect, ctx.context, parent}
    , m_view{view}
    , m_model{effect}
    , m_context{ctx}
{
  // Main item
  if (auto factory = fact.findDefaultFactory(effect))
  {
    m_fx = factory->makeItem(m_model, m_context.context, this);
  }

  if (!m_fx)
  {
    m_fx = new Process::DefaultEffectItem{m_model, m_context.context, this};
  }

  m_fx->setParentItem(this);
  m_fx->setPos({0, TitleHeight});

  /// Rects
  // TODO bind
  connect(m_fx, &score::ResizeableItem::sizeChanged, this, &EffectItem::updateSize);

  // In & out ports
  resetInlets();
  resetOutlets();
  con(effect, &Process::ProcessModel::inletsChanged, this, &EffectItem::resetInlets);
  con(effect, &Process::ProcessModel::outletsChanged, this, &EffectItem::resetOutlets);

  updateSize();
}

void EffectItem::updateSize()
{
  setSize(m_fx->boundingRect().size());
}

void EffectItem::setSize(QSizeF sz)
{
  if (sz != m_contentSize)
  {
    prepareGeometryChange();
    m_contentSize = QSizeF{std::max(100., sz.width()), std::max(10., sz.height())};
    if (m_ui)
    {
      m_ui->setParentItem(nullptr);
    }

    const auto r = boundingRect();

    for (auto& outlet : m_outlets)
    {
      outlet->setPos(outlet->pos().x(), r.height() + OutletY0);
    }
    m_view.recomputeItemPositions();

    if (m_ui)
    {
      m_ui->setParentItem(this);
      m_ui->setPos({m_contentSize.width() + TopButtonX0, TopButtonY0});
    }
    update();
  }
}

void EffectItem::resetInlets()
{
  qDeleteAll(m_inlets);
  m_inlets.clear();
  qreal x = InletX0;
  auto& portFactory = score::AppContext().interfaces<Process::PortFactoryList>();
  for (Process::Inlet* port : m_model.inlets())
  {
    if (port->hidden)
      continue;
    Process::PortFactory* fact = portFactory.get(port->concreteKey());
    auto item = fact->makeItem(*port, m_context.context, this, this);
    item->setPos(x, InletY0);
    m_inlets.push_back(item);

    x += PortSpacing;
  }

  m_label->setPos(QPointF{x, 0.});
  updateSize();
}

void EffectItem::resetOutlets()
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
    auto item = fact->makeItem(*port, m_context.context, this, this);
    item->setPos(x, h);
    m_outlets.push_back(item);

    x += PortSpacing;
  }
  updateSize();
}

void EffectItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  paintNode(painter, m_selected, m_hover, boundingRect());
}

void EffectItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_context.context.focusDispatcher.focus(&m_context.presenter);
  score::SelectionDispatcher{m_context.context.selectionStack}.setAndCommit({&m_model});
  event->accept();
}

void EffectItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  auto min_dist
      = (event->screenPos() - event->buttonDownScreenPos(Qt::LeftButton)).manhattanLength()
        >= QApplication::startDragDistance();
  if (min_dist)
  {
    auto drag = new QDrag{this};
    QMimeData* mime = new QMimeData;

    JSONReader r;
    r.stream.StartObject();
    Scenario::copyProcess(r, m_model);
    r.obj["Path"] = score::IDocument::path(m_model);
    r.stream.EndObject();
    litHeight = this->boundingRect().height();
    mime->setData(score::mime::effect(), r.toByteArray());
    drag->setMimeData(mime);

    drag->exec();

    drag->deleteLater();
  }

  event->accept();
}

void EffectItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

}
