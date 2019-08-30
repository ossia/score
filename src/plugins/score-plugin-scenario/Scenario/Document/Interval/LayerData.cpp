#include <Process/Process.hpp>
#include <Scenario/Document/Interval/LayerData.hpp>
#include <score/graphics/RectItem.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <QPainter>

#include <score/tools/Debug.hpp>

namespace Scenario
{
LayerData::LayerData(const Process::ProcessModel* m) : m_model(m) {}

void LayerData::cleanup()
{
  for(const auto& layer : m_layers)
  {
    // QPointer<Process::LayerView> v{view.view[i]};
    delete layer.presenter;
    delete layer.container;
    // if(v)
    //{
    //  qDebug() << "A presenter does not delete its view" <<
    //  (QObject*)view.view[i]; delete view.view[i];
    //}
  }

  m_layers.clear();
}

void LayerData::addView(
    Process::LayerFactory& factory,
    ZoomRatio zoomRatio,
    const Process::ProcessPresenterContext& context,
    QGraphicsItem* parentItem,
    QObject* parent)
{
  auto container = new LayerRectItem{parentItem};
  container->setFlag(QGraphicsItem::ItemClipsChildrenToShape);
  container->setY(m_slotY);
  auto view = factory.makeLayerView(*m_model, container);
  view->setPos(-m_model->startOffset().toPixels(zoomRatio), 0.);

  auto presenter = factory.makeLayerPresenter(*m_model, view, context, parent);
  presenter->putToFront();
  presenter->on_zoomRatioChanged(zoomRatio);
  presenter->setFullView();

  m_layers.push_back({presenter, container, view});
}

void LayerData::removeView(int i)
{
  delete m_layers[i].presenter;
  delete m_layers[i].container;
  m_layers.erase(m_layers.begin() + i);
}

Process::LayerPresenter* LayerData::mainPresenter() const noexcept
{
  return m_layers.front().presenter;
}

Process::LayerView* LayerData::mainView() const noexcept
{
  return m_layers.front().view;
}

bool LayerData::focused() const
{
  // TODO is this correct
  return mainPresenter()->focused();
}

void LayerData::setFocus(bool focus) const
{
  for (const auto& p : m_layers)
    p.presenter->setFocus(focus);
}

void LayerData::on_focusChanged() const
{
  for (const auto& p : m_layers)
    p.presenter->on_focusChanged();
}

void LayerData::setFullView() const
{
  for (const auto& p : m_layers)
    p.presenter->setFullView();
}

void LayerData::setWidth(qreal width, qreal defaultWidth) const
{
  for (const auto& p : m_layers)
  {
    p.presenter->setWidth(width, defaultWidth);
  }
}

void LayerData::setHeight(qreal height) const
{
  for (const auto& p : m_layers)
    p.presenter->setHeight(height);
}

void LayerData::putToFront() const
{
  for (const auto& p : m_layers)
    p.presenter->putToFront();
}

void LayerData::putBehind() const
{
  for (const auto& p : m_layers)
    p.presenter->putBehind();
}

void LayerData::on_zoomRatioChanged(
    const Process::ProcessPresenterContext& ctx,
    ZoomRatio r,
    qreal parent_width,
    qreal parent_default_width,
    qreal slot_height,
    QGraphicsItem* parentItem,
    QObject* parent)
{
  updateLoops(ctx, r, parent_width, parent_default_width, slot_height, parentItem, parent);

  for (const auto& p : m_layers)
  {
    p.presenter->on_zoomRatioChanged(r);
    p.view->setPos(-m_model->startOffset().toPixels(r), 0.);
  }
}


void LayerData::setupView(
    LayerData::Layer& layer,
    int idx,
    qreal parent_width,
    qreal parent_default_width,
    qreal view_width,
    qreal view_height)
{
  layer.view->setX(m_layers.front().view->pos().x());

  layer.container->setX(view_width * idx);
  layer.container->setRect({0., 0., view_width, view_height});

  layer.presenter->setWidth(parent_width, parent_default_width);
  layer.presenter->setHeight(view_height);
  layer.presenter->parentGeometryChanged();
}

void LayerData::updateLoops(
    const Process::ProcessPresenterContext& ctx,
    ZoomRatio r,
    qreal parent_width,
    qreal parent_default_width,
    qreal slot_height,
    QGraphicsItem* parentItem,
    QObject* parent)
{
  if (m_model->loops())
  {
    const auto view_width = m_model->loopDuration().toPixels(r);
    const auto num_views = std::max((int)1, (int)std::ceil(parent_width / view_width));
    if ((int)m_layers.size() < num_views)
    {
      int missing = num_views - m_layers.size();

      auto f = ctx.processList.findDefaultFactory(m_model->concreteKey());
      SCORE_ASSERT(f);
      for(int i = 0; i < missing; i++)
      {
        addView(*f, r, ctx, parentItem, parent);
      }
    }
    else
    {
      for(int i = int(m_layers.size()) - 1; i > num_views; i--)
        removeView(i);
    }

    // Update sizes for everyone
    for(int i = 0; i <int(m_layers.size()); i++)
    {
      setupView(m_layers[i], i, parent_width, parent_default_width, view_width, slot_height);
    }
  }
  else
  {
    if(m_layers.empty())
    {
      // Initial case
      auto f = ctx.processList.findDefaultFactory(m_model->concreteKey());
      SCORE_ASSERT(f);

      addView(*f, r, ctx, parentItem, parent);
    }
    else
    {
      for(int i =  int(m_layers.size()) - 1; i > 0; i--)
        removeView(i);
    }

    // Update sizes for first layer
    setupView(m_layers.front(), 0, parent_width, parent_default_width, parent_width, slot_height);
  }
}

void LayerData::parentGeometryChanged() const
{
  for (const auto& p : m_layers)
    p.presenter->parentGeometryChanged();
}

void LayerData::fillContextMenu(
    QMenu& m,
    QPoint pos,
    QPointF scenepos,
    const Process::LayerContextMenuManager& mgr) const
{
  return mainPresenter()->fillContextMenu(m, pos, scenepos, mgr);
}

Process::GraphicsShapeItem* LayerData::makeSlotHeaderDelegate() const
{
  return mainPresenter()->makeSlotHeaderDelegate();
}

void LayerData::updatePositions(qreal y, qreal instancewidth)
{
  m_slotY = y;
  qreal x = 0;
  for (const auto& p : m_layers)
  {
    p.container->setRect({0, 0, instancewidth, p.container->rect().height()});
    p.container->setPos(x, y);
    x += instancewidth;
  }
}

void LayerData::updateXPositions(qreal instancewidth) const
{
  qreal x = 0;
  for (const auto& p : m_layers)
  {
    p.container->setRect({0, 0, instancewidth, p.container->rect().height()});
    p.container->setX(x);
    x += instancewidth;
  }
}

void LayerData::updateYPositions(qreal y)
{
  m_slotY = y;
  for (const auto& p : m_layers)
    p.container->setY(y);
}

void LayerData::updateContainerWidths(qreal w) const
{
  for (const auto& p : m_layers)
    p.container->setRect({0., 0., w, p.container->rect().height()});
}

void LayerData::updateContainerHeights(qreal h) const
{
  for (const auto& p : m_layers)
    p.container->setRect({0., 0., p.container->rect().width(), h});
}

void LayerData::updateStartOffset(double x) const
{
  for (const auto& p : m_layers)
    p.view->setPos(x, 0.);
}

void LayerData::update() const
{
  for (const auto& p : m_layers)
    p.view->update();
}

void LayerData::setZValue(qreal z) const
{
  for (const auto& p : m_layers)
    p.container->setZValue(z);
}

QPixmap LayerData::pixmap() const noexcept
{
  return mainView()->pixmap();
}

LayerRectItem::LayerRectItem(QGraphicsItem* parent)
  : score::ResizeableItem{parent}
{
  //this->setFlag(ItemHasNoContents, true);
}

void LayerRectItem::setRect(const QRectF& r)
{
  if(r != m_rect)
  {
    prepareGeometryChange();
    m_rect = r;
    sizeChanged({r.width(), r.height()});
  }
}

QRectF LayerRectItem::rect() const noexcept { return m_rect; }

QRectF LayerRectItem::boundingRect() const
{
  return m_rect;
}

void LayerRectItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  //painter->setPen(Qt::blue);
  //painter->setBrush(Qt::NoBrush);
  //painter->drawRect(m_rect);
  painter->setPen(score::Skin::instance().DarkGray.main.pen_cosmetic);
  painter->drawLine(m_rect.width(), 0., m_rect.width(), m_rect.height());
}

void LayerRectItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  this->setZValue(10);
}

void LayerRectItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  this->setZValue(0);
}

void LayerRectItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void LayerRectItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void LayerRectItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

}
