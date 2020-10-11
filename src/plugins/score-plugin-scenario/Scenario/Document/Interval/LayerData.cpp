#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <Process/Process.hpp>
#include <Scenario/Document/Interval/LayerData.hpp>

#include <score/graphics/RectItem.hpp>
#include <score/tools/Debug.hpp>

#include <QPainter>

namespace Scenario
{
LayerData::LayerData(const Process::ProcessModel* m) : m_model(m) { }

void LayerData::cleanup()
{
  for (const auto& layer : m_layers)
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
    const Process::Context& context,
    QGraphicsItem* parentItem,
    QObject* parent)
{
  auto container = new LayerRectItem{parentItem};
  container->setFlag(QGraphicsItem::ItemClipsChildrenToShape);
  container->setY(m_slotY);
  auto view = factory.makeLayerView(*m_model, context, container);
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
  return !m_layers.empty() ? m_layers.front().presenter : nullptr;
}

Process::LayerView* LayerData::mainView() const noexcept
{
  return !m_layers.empty() ? m_layers.front().view : nullptr;
}

bool LayerData::focused() const
{
  // TODO is this correct
  if (auto p = mainPresenter())
    return p->focused();
  return false;
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
    const Process::Context& ctx,
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
  layer.container->setSize({view_width, view_height});

  layer.presenter->setWidth(parent_width, parent_default_width);
  layer.presenter->setHeight(view_height);
  layer.presenter->parentGeometryChanged();
}

void LayerData::updateLoops(
    const Process::Context& ctx,
    ZoomRatio r,
    qreal parent_width,
    qreal parent_default_width,
    qreal slot_height,
    QGraphicsItem* parentItem,
    QObject* parent)
{
  auto f = ctx.processList.findDefaultFactory(m_model->concreteKey());
  SCORE_ASSERT(f);
  if (m_model->loops())
  {
    const auto view_width = m_model->loopDuration().toPixels(r);
    constexpr double min_view_width = 10.;
    // TODO here it should be different between fullview and temporal
    // (parent_width vs default_width)
    const auto num_views
        = (view_width < min_view_width) ? 1 : std::max((int)1, (int)std::ceil(parent_width / view_width));
    if ((int)m_layers.size() < num_views)
    {
      int missing = num_views - m_layers.size();

      for (int i = 0; i < missing; i++)
      {
        addView(*f, r, ctx, parentItem, parent);
      }
    }
    else
    {
      for (int i = int(m_layers.size()) - 1; i >= num_views; i--)
        removeView(i);
    }

    // Update sizes for everyone
    for (int i = 0; i < int(m_layers.size()); i++)
    {
      setupView(m_layers[i], i, parent_width, parent_default_width, view_width, slot_height);
    }

    if (!m_layers.empty())
      m_layers.front().container->setFlag(QGraphicsItem::ItemHasNoContents, false);
    else
      SCORE_SOFT_ASSERT(!"Missing layer view !");
  }
  else
  {
    if (m_layers.empty())
    {
      addView(*f, r, ctx, parentItem, parent);
    }
    else
    {
      for (int i = int(m_layers.size()) - 1; i > 0; i--)
        removeView(i);
    }

    // Update sizes for first layer
    setupView(m_layers.front(), 0, parent_width, parent_default_width, parent_width, slot_height);

    if (!m_layers.empty())
      m_layers.front().container->setFlag(QGraphicsItem::ItemHasNoContents, true);
    else
      SCORE_SOFT_ASSERT(!"Missing layer view !");
  }

  if (!m_layers.empty())
  {
    auto& last = m_layers.back();
    auto w = parent_width - last.container->x();
    if (w > 0)
    {
      last.container->setWidth(w);
    }
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
  if (auto p = mainPresenter())
    return p->fillContextMenu(m, pos, scenepos, mgr);
}

Process::GraphicsShapeItem* LayerData::makeSlotHeaderDelegate() const
{
  if (auto p = mainPresenter())
    return p->makeSlotHeaderDelegate();
  return nullptr;
}

void LayerData::updateYPositions(qreal y)
{
  m_slotY = y;
  for (const auto& p : m_layers)
    p.container->setY(y);
}

void LayerData::updateContainerHeights(qreal h) const
{
  for (const auto& p : m_layers)
    p.container->setSize({p.container->size().width(), h});
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
  if (auto v = mainView())
    return v->pixmap();
  return {};
}

void LayerData::disconnect(const Process::ProcessModel& proc, QObject& intervalPresenter)
{
  QObject::disconnect(&proc, &Process::ProcessModel::loopsChanged, &intervalPresenter, nullptr);
  QObject::disconnect(
        &proc, &Process::ProcessModel::startOffsetChanged, &intervalPresenter, nullptr);
  QObject::disconnect(
        &proc, &Process::ProcessModel::loopDurationChanged, &intervalPresenter, nullptr);
}

LayerRectItem::LayerRectItem(QGraphicsItem* parent) : score::ResizeableItem{parent}
{
  // this->setFlag(ItemHasNoContents, true);
}

void LayerRectItem::setSize(const QSizeF& r)
{
  if (r != m_size)
  {
    prepareGeometryChange();
    m_size = r;
    sizeChanged(m_size);

    if (r.width() <= 2 && isVisible())
      setVisible(false);
    else if (r.width() > 2 && !isVisible())
      setVisible(true);
  }
}

void LayerRectItem::setWidth(qreal w)
{
  if (w != m_size.width())
  {
    prepareGeometryChange();
    m_size.setWidth(w);
    sizeChanged(m_size);

    if (w <= 2 && isVisible())
      setVisible(false);
    else if (w > 2 && !isVisible())
      setVisible(true);
  }
}
void LayerRectItem::setHeight(qreal h)
{
  if (h != m_size.height())
  {
    prepareGeometryChange();
    m_size.setHeight(h);
    sizeChanged(m_size);
  }
}

QSizeF LayerRectItem::size() const noexcept
{
  return m_size;
}

QRectF LayerRectItem::boundingRect() const
{
  return {0., 0., m_size.width(), m_size.height()};
}

void LayerRectItem::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  // painter->setPen(Qt::blue);
  // painter->setBrush(Qt::NoBrush);
  // painter->drawRect(m_rect);
  painter->setPen(score::Skin::instance().DarkGray.main.pen_cosmetic);
  painter->drawLine(m_size.width(), 0., m_size.width(), m_size.height());
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
