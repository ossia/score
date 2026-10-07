#include "NodalIntervalView.hpp"

#include <Scenario/Application/Drops/DropOnCable.hpp>
#include <Scenario/Application/Drops/DropProcessInInterval.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessCreation.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>

#include <Scenario/Commands/Cohesion/InterpolateMacro.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/graphics/FirstUserInput.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/graphics/ZoomItem.hpp>
#include <score/model/Skin.hpp>
#include <score/selection/Selection.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsRectItem>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsView>
#include <QPainter>
#include <QScopedValueRollback>
#include <QTimer>

#include <wobjectimpl.h>

namespace Scenario
{
struct NodalContainer : public score::EmptyRectItem
{
  W_OBJECT(NodalContainer)
public:
  using score::EmptyRectItem::EmptyRectItem;

  int type() const noexcept { return QGraphicsItem::UserType + 5555; }
};
}
W_OBJECT_IMPL(Scenario::NodalIntervalView)
W_OBJECT_IMPL(Scenario::NodalContainer)
namespace Scenario
{
constexpr double g_zoom_base = 1.2;
// g_zoom_base is used as a logarithm base (std::log(g_zoom_base) as a
// denominator); it must be strictly greater than 1, otherwise the division
// would yield NaN / a division by zero.
static_assert(g_zoom_base > 1.0);
NodalIntervalView::NodalIntervalView(
    NodalIntervalView::ItemsToShow sh, const IntervalModel& model,
    const Process::Context& ctx, QGraphicsItem* parent)
    : score::EmptyRectItem{parent}
    , m_model{model}
    , m_context{ctx}
    , m_itemsToShow{sh}
    , m_container{new NodalContainer{this}}
{
  setAcceptDrops(true);
  setAcceptedMouseButtons(Qt::AllButtons);
  // setFlag(ItemHasNoContents, true);
  // setRect(QRectF{0, 0, 1000, 1000});
  const TimeVal r = m_model.duration.defaultDuration();
  for(auto& proc : m_model.processes)
  {
    if(m_itemsToShow == ItemsToShow::OnlyEffects
       && !(proc.flags() & Process::ProcessFlags::TimeIndependent))
      continue;
    setupNode(new Process::NodeItem{proc, m_context, r, m_container});
  }
  // No viewport stored (a new canvas): it follows its nodes, see
  // recenterRelativeToView.
  m_autoCenter = !m_model.nodalCenter() && m_model.legacyNodalOffset().isNull();
  m_model.processes.added.connect<&NodalIntervalView::on_processAdded>(*this);
  m_model.processes.removing.connect<&NodalIntervalView::on_processRemoving>(*this);

  con(
      model, &IntervalModel::executionEvent, this,
      [this](IntervalExecutionEvent ev) {
    if(ev == IntervalExecutionEvent::Finished)
      on_playPercentageChanged(0., TimeVal{});
      },
      Qt::QueuedConnection);

  {
    // Zoom handling
    auto item = new score::ZoomItem{this};
    item->setPos(10, 10);

    connect(item, &score::ZoomItem::zoom, this, &NodalIntervalView::zoomPlus);
    connect(item, &score::ZoomItem::dezoom, this, &NodalIntervalView::zoomMinus);

    connect(item, &score::ZoomItem::recenter, this, [this] {
      m_autoCenter = false;
  m_nodeToShow = nullptr;
      recenter();
    });
    connect(item, &score::ZoomItem::rescale, this, &NodalIntervalView::rescale);
    connect(
        this, &score::EmptyRectItem::sizeChanged, this,
        &NodalIntervalView::recenterRelativeToView);

    if(parent)
    {
      if(auto v = getView(*parent))
      {
        auto gv = static_cast<Scenario::ProcessGraphicsView*>(v);
        connect(gv, &ProcessGraphicsView::visibleRectChanged, this, [this] {
          recenterRelativeToView();
        }, Qt::DirectConnection);
        if(m_autoCenter)
          new score::FirstUserInput{gv->viewport(), this, [this] {
            // Pinned where it is now: from here on it moves only when asked.
            if(!std::exchange(m_autoCenter, false))
              return;
            if(!m_nodeItems.empty() && !boundingRect().isEmpty())
              storeCenterFromContainer();
          }};
      }
    }
  }
  if(const double savedScale = m_model.nodalScale();
     savedScale != 1.0 && std::isfinite(savedScale) && savedScale > 0.)
  {
    m_container->setScale(savedScale);
    m_zoomLevel = std::log(savedScale) / std::log(g_zoom_base);
  }
  else if(savedScale != 1.0)
  {
    // A corrupt document: do not divide by it.
    const_cast<IntervalModel&>(m_model).setNodalScale(1.0);
  }

  QTimer::singleShot(1, this, &NodalIntervalView::recenterRelativeToView);

  {
    m_selectionRect = new QGraphicsRectItem(this);
    m_selectionRect->setZValue(1000.);
    m_selectionRect->setAcceptedMouseButtons(Qt::NoButton);
    m_selectionRect->setVisible(false);

    auto& skin = score::Skin::instance();
    con(skin, &score::Skin::changed, this,
        &NodalIntervalView::updateSelectionRectStyle);
    updateSelectionRectStyle();
  }
}

void NodalIntervalView::updateSelectionRectStyle()
{
  const QColor selColor = score::Skin::instance().Light.color();
  QColor selFill = selColor;
  selFill.setAlpha(40);

  m_selectionRect->setPen(
      QPen{selColor, 1, Qt::DashLine, Qt::SquareCap, Qt::BevelJoin});
  m_selectionRect->setBrush(selFill);
}

void NodalIntervalView::zoomPlus()
{
  auto newLevel = m_zoomLevel + 1.0;
  if(newLevel <= 0.5 && newLevel >= -0.5)
    newLevel = 0.;
  zoomTo(newLevel);
}

void NodalIntervalView::zoomMinus()
{
  auto newLevel = m_zoomLevel - 1.0;
  if(newLevel <= 0.5 && newLevel >= -0.5)
    newLevel = 0.;
  zoomTo(newLevel);
}

QRectF NodalIntervalView::visibleRect() const
{
  const auto parentRect = boundingRect();
  auto v = getView(*this);
  if(!v)
    return parentRect;

  const auto viewTopLeft = mapFromScene(v->mapToScene(0, 0));
  const auto viewBottomRight = mapFromScene(v->mapToScene(v->width(), v->height()));
  return QRectF{viewTopLeft, viewBottomRight}.intersected(parentRect);
}

QPointF NodalIntervalView::viewportCenter() const
{
  const auto visible = visibleRect();
  return visible.isEmpty() ? boundingRect().center() : visible.center();
}

QPointF NodalIntervalView::pastePosition(QPointF scenePos) const
{
  const QPointF p = mapFromScene(scenePos);
  return m_container->mapFromParent(visibleRect().contains(p) ? p : viewportCenter());
}

void NodalIntervalView::pickInitialViewport()
{
  // First time this canvas is shown. Documents saved before the center was
  // stored have a pan offset relative to the centered nodes, in parent
  // coordinates, and a scale: convert them. Those with neither (new documents,
  // and older ones which never managed to save their zoom) get the nodes fitted
  // in the view. Either way the viewport is pinned from now on: it no longer
  // moves when nodes do.
  const QPointF offset = m_model.legacyNodalOffset();
  if(offset.isNull() && m_model.nodalScale() == 1.0 && m_itemsToShow == AllItems)
  {
    recenter();
    return;
  }
  const QPointF center = enclosingRect().center() - offset / m_container->scale();
  const_cast<IntervalModel&>(m_model).setNodalCenter(center);
  placeContainer(center);
}

void NodalIntervalView::placeContainer(QPointF center)
{
  // No rotation: mapToParent(p) == pos() + scale() * p
  m_container->setPos(viewportCenter() - center * m_container->scale());
}

void NodalIntervalView::storeCenterFromContainer()
{
  const QPointF center = (viewportCenter() - m_container->pos()) / m_container->scale();
  const_cast<IntervalModel&>(m_model).setNodalCenter(center);
}

void NodalIntervalView::recenterRelativeToView()
{
  if(m_autoCenter)
  {
    // A new canvas follows its nodes until it is panned or zoomed: a slot is
    // laid out as soon as it is created, before the process it was created
    // for gets its node, and a node computes its size a moment after it is
    // created.
    if(!boundingRect().isEmpty() && !m_nodeItems.empty())
      pickInitialViewport();
    return;
  }
  if(auto center = m_model.nodalCenter())
    placeContainer(*center);
  else if(!boundingRect().isEmpty())
    pickInitialViewport();
  // else: not laid out yet, sizeChanged brings us back here.
}

void NodalIntervalView::recenter()
{
  auto parentRect = boundingRect();
  auto childRect = enclosingRect();

  double w_ratio = parentRect.width() / childRect.width();
  double h_ratio = parentRect.height() / childRect.height();
  double z = std::clamp(std::min(w_ratio, h_ratio), 0.01, 1.0);
  m_container->setScale(z);
  m_zoomLevel = std::log(z) / std::log(g_zoom_base);
  const_cast<IntervalModel&>(m_model).setNodalScale(z);

  const QPointF center = childRect.center();
  const_cast<IntervalModel&>(m_model).setNodalCenter(center);
  placeContainer(center);
}

void NodalIntervalView::rescale()
{
  m_autoCenter = false;
  m_nodeToShow = nullptr;
  // Back to 1:1, around what is currently at the center of the view
  recenterRelativeToView();
  const QPointF center = m_model.nodalCenter().value_or(enclosingRect().center());
  m_container->setScale(1.0);
  m_zoomLevel = 0.;
  const_cast<IntervalModel&>(m_model).setNodalScale(1.0);
  placeContainer(center);
}

NodalIntervalView::~NodalIntervalView()
{
  qDeleteAll(m_nodeItems);
}

void NodalIntervalView::on_drop(QPointF pos, const QMimeData* data)
{
  QScopedValueRollback dropping{m_dropping, true};
  const bool ok = m_context.app.interfaces<Scenario::IntervalDropHandlerList>().drop(
      m_context, m_model, m_container->mapFromParent(pos), *data);
  if(ok)
    return;

  Scenario::DropProcessInInterval d;
  d.drop(m_context, m_model, pos, *data);
}

void NodalIntervalView::on_playPercentageChanged(double t, TimeVal parent_dur)
{
  t = ossia::max(t, 0.);
  for(Process::NodeItem* node : m_nodeItems)
  {
    node->setPlayPercentage(t, parent_dur);
  }
}

void NodalIntervalView::on_processAdded(const Process::ProcessModel& proc)
{
  if(m_itemsToShow == ItemsToShow::OnlyEffects
     && !(proc.flags() & Process::ProcessFlags::TimeIndependent))
    return;

  // The reason for this loop sucks a bit.
  // The "nodal" small view is created in IntervalModel::on_addProcess when slotAdded is sent,
  // which is called as a response of the nano signal processes.mutable_added.connect<>...
  // But NodalIntervalView adds itself to the callback list: this means that
  // after creation which already creates a node for the process, we get the item duplicated here.
  for(auto it = m_nodeItems.begin(); it != m_nodeItems.end(); ++it)
  {
    if(&(*it)->model() == &proc)
    {
      return;
    }
  }

  auto item = new Process::NodeItem{
      proc, m_context, m_model.duration.defaultDuration(), m_container};
  setupNode(item);

  // A canvas still following its nodes (m_autoCenter) recenters by itself
  if(!m_autoCenter && !m_dropping)
  {
    m_nodeToShow = item;
    showNode(*item);
  }
}

void NodalIntervalView::showNode(const Process::NodeItem& item)
{
  // As the zoom's center button does: all the nodes fit
  const QRectF visible = visibleRect();
  if(!visible.isEmpty() && !visible.contains(mapRectFromScene(item.sceneBoundingRect())))
    recenter();
}

void NodalIntervalView::on_processRemoving(const Process::ProcessModel& model)
{
  for(auto it = m_nodeItems.begin(); it != m_nodeItems.end(); ++it)
  {
    if(&(*it)->model() == &model)
    {
      delete(*it);
      m_nodeItems.erase(it);
      return;
    }
  }
}

void NodalIntervalView::on_zoomRatioChanged(ZoomRatio ratio)
{
  // TODO should be "on model duration changed"
  const TimeVal r = m_model.duration.defaultDuration();
  for(Process::NodeItem* node : m_nodeItems)
  {
    node->setParentDuration(r);
  }
}

QRectF NodalIntervalView::enclosingRect() const noexcept
{
  if(m_nodeItems.empty())
    return QRectF{-100., -100., 200., 200.};
  double x0{std::numeric_limits<double>::max()}, y0{x0},
      x1{std::numeric_limits<double>::lowest()}, y1{x1};

  for(QGraphicsItem* item : m_nodeItems)
  {
    // What the node draws, in canvas coordinates: its bounding rect does not
    // start at its position (the title bar is above it, the inlets left of it).
    const auto r = item->mapRectToParent(item->boundingRect());
    x0 = std::min(x0, r.left());
    y0 = std::min(y0, r.top());
    x1 = std::max(x1, r.right());
    y1 = std::max(y1, r.bottom());
  }

  // 5 % of margin on each side, so that the center is the nodes' center.
  const double w = x1 - x0;
  const double h = y1 - y0;
  return {x0 - 0.05 * w, y0 - 0.05 * h, 1.1 * w, 1.1 * h};
}

void NodalIntervalView::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
}

void NodalIntervalView::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
}

void NodalIntervalView::dragMoveEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
}

void NodalIntervalView::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  on_drop(event->pos(), event->mimeData());
  event->accept();
}

void NodalIntervalView::mousePressEvent(QGraphicsSceneMouseEvent* e)
{
  if(e->button() == Qt::LeftButton)
  {
    if(e->modifiers() & Qt::ControlModifier)
    {
      m_rubberBanding = true;
      m_rubberBandOrigin = e->pos();
      m_rubberBandRect = QRectF{m_rubberBandOrigin, m_rubberBandOrigin};
      m_selectionRect->setVisible(true);
      m_selectionRect->setRect(QRectF{});
    }
    else
    {
      // Selecting the slot's interval makes the next paste go in it. The
      // central canvas needs no selection: it pastes in the displayed interval.
      if(m_itemsToShow == OnlyEffects)
        score::SelectionDispatcher{this->m_context.selectionStack}.select(m_model);
      else
        this->m_context.selectionStack.deselect();
      auto focus = Process::ProcessFocusManager::get(this->m_context);
      if(focus)
        focus->focusNothing();
      m_pressedPos = e->scenePos();
    }
  }
  else if(e->button() == Qt::MiddleButton)
  {
    m_pressedPos = e->scenePos();
  }
  e->accept();
}

void NodalIntervalView::mouseMoveEvent(QGraphicsSceneMouseEvent* e)
{
  if(m_rubberBanding && (e->buttons() & Qt::LeftButton))
  {
    m_rubberBandRect = QRectF{m_rubberBandOrigin, e->pos()}.normalized();
    m_selectionRect->setRect(m_rubberBandRect);
  }
  else if(e->buttons() & (Qt::LeftButton | Qt::MiddleButton))
  {
    const auto delta = e->scenePos() - m_pressedPos;
    panBy(delta);
    m_pressedPos = e->scenePos();
  }
  e->accept();
}

void NodalIntervalView::panBy(QPointF delta)
{
  m_autoCenter = false;
  m_nodeToShow = nullptr;
  m_container->setPos(m_container->pos() + delta);
  storeCenterFromContainer();
}

void NodalIntervalView::mouseReleaseEvent(QGraphicsSceneMouseEvent* e)
{
  if(e->button() == Qt::LeftButton && m_rubberBanding)
  {
    m_rubberBanding = false;
    m_selectionRect->setVisible(false);
    m_selectionRect->setRect(QRectF{});

    const bool cumulation = e->modifiers() & Qt::ControlModifier;
    Selection sel;
    for(auto* node : m_nodeItems)
    {
      const QRectF nodeRect = mapRectFromScene(node->sceneBoundingRect());
      if(m_rubberBandRect.intersects(nodeRect))
        sel.append(node->model());
    }
    sel = filterSelections(sel, m_context.selectionStack.currentSelection(), cumulation);
    score::SelectionDispatcher{m_context.selectionStack}.select(sel);
    m_rubberBandRect = {};
  }
  e->accept();
}


void NodalIntervalView::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  event->accept();
}

void NodalIntervalView::wheelEvent(QGraphicsSceneWheelEvent* event)
{
  if(m_itemsToShow != NodalIntervalView::AllItems)
    return;

  static constexpr double sensitivity = 120.0;
  double numDegrees = event->delta();
  if(numDegrees == 0)
  {
    event->ignore();
    return;
  }

  QPointF anchor = event->pos();
  QPointF localAnchor = m_container->mapFromParent(anchor);

  double scrollStep = numDegrees / sensitivity;
  m_zoomLevel = std::clamp(m_zoomLevel + scrollStep, -10.0, 5.0);
  double newScale = std::pow(g_zoom_base, m_zoomLevel);
  m_container->setScale(newScale);

  QPointF newAnchorPos = m_container->mapToParent(localAnchor);
  m_container->setPos(m_container->pos() + (anchor - newAnchorPos));
  const_cast<IntervalModel&>(m_model).setNodalScale(newScale);
  storeCenterFromContainer();

  event->accept();
}

void NodalIntervalView::zoomTo(double newZoomLevel)
{
  m_autoCenter = false;
  m_nodeToShow = nullptr;
  newZoomLevel = std::clamp(newZoomLevel, -10.0, 5.0);
  if(newZoomLevel == m_zoomLevel)
    return;

  QPointF anchor;
  if(auto v = getView(*this))
  {
    const auto viewTopLeft = mapFromScene(v->mapToScene(0, 0));
    const auto viewBottomRight = mapFromScene(v->mapToScene(v->width(), v->height()));
    const auto visibleRect
        = QRectF{viewTopLeft, viewBottomRight}.intersected(boundingRect());
    anchor = visibleRect.center();
  }
  else
  {
    anchor = boundingRect().center();
  }

  const QPointF localAnchor = m_container->mapFromParent(anchor);

  m_zoomLevel = newZoomLevel;
  double newScale = std::pow(g_zoom_base, m_zoomLevel);
  m_container->setScale(newScale);

  const QPointF newAnchorPos = m_container->mapToParent(localAnchor);
  m_container->setPos(m_container->pos() + (anchor - newAnchorPos));
  const_cast<IntervalModel&>(m_model).setNodalScale(newScale);
  storeCenterFromContainer();
}

void NodalIntervalView::setupNode(Process::NodeItem* item)
{
  m_nodeItems.push_back(item);
  // A canvas with no viewport yet centers on its nodes once they have a size.
  connect(&item->model(), &Process::ProcessModel::sizeChanged, this, [this] {
    if(m_autoCenter)
      recenterRelativeToView();
  });
  connect(item, &Process::NodeItem::geometryChanged, this, [this, item] {
    if(m_nodeToShow == item)
      showNode(*item);
  });
  connect(
      item, &Process::NodeItem::dropReceived, this, &NodalIntervalView::on_dropOnNode);
  item->dropOnCableHandler
      = [this](
            const Process::ProcessModel& proc, const Process::Cable& cbl,
            score::Dispatcher& disp) {
    QScopedValueRollback dropping{m_dropping, true};
    auto& doc = score::IDocument::modelDelegate<Scenario::ScenarioDocumentModel>(
        m_context.document);
    Scenario::insertProcessInCable(disp, m_context, doc, proc, cbl);
      };
  item->canDropOnCableHandler
      = [this](const Process::ProcessModel& proc, const Process::Cable& cbl) {
    return Scenario::canInsertProcessInCable(m_context, proc, cbl);
      };
}

void NodalIntervalView::on_dropOnNode(const QPointF& pos, const QMimeData& mime)
{
  auto item = static_cast<Process::NodeItem*>(QObject::sender());
  if(!item)
    return;

  QScopedValueRollback dropping{m_dropping, true};
  auto& doc = score::IDocument::modelDelegate<Scenario::ScenarioDocumentModel>(
      m_context.document);
  auto drop = new Scenario::DropOnNode{*item, doc, m_context};
  drop->drop(mime);
  drop->deleteLater();
}
}
