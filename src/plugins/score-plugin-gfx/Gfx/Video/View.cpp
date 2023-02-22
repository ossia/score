#include "View.hpp"

#include <Process/Style/ScenarioStyle.hpp>

#include <Gfx/Video/Process.hpp>
#include <Video/Thumbnailer.hpp>

#include <score/graphics/GraphicsItem.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/ThreadPool.hpp>
#include <score/tools/std/Invoke.hpp>

#include <ossia/detail/closest_element.hpp>
#include <ossia/detail/flicks.hpp>

#include <QGraphicsView>
#include <QPainter>

namespace Gfx::Video
{

View::View(const Model& model, QGraphicsItem* parent)
    : LayerView{parent}
{
  this->setAcceptDrops(true);
  setFlag(ItemClipsToShape, true);
  con(model, &Model::pathChanged, this,
      [this, &model] { onPathChanged(model.absolutePath()); });
  onPathChanged(model.absolutePath());
}

View::~View()
{
  if(m_thumb)
  {
    ossia::qt::run_async(m_thumb, &QObject::deleteLater);

    score::ThreadPool::instance().releaseThread();
  }
}

void View::setZoom(ZoomRatio r)
{
  if(r <= 1000)
    return;
  m_zoom = r;
  widthChanged(width());
  update();
}

void View::onPathChanged(const QString& str)
{
  auto& inst = score::ThreadPool::instance();
  QThread* oldThread{};

  if(m_thumb)
  {
    disconnect(m_thumb, &::Video::VideoThumbnailer::thumbnailReady, this, nullptr);
    oldThread = m_thumb->thread();
    m_thumb->deleteLater();
  }

  m_images.clear();

  m_thumb = new ::Video::VideoThumbnailer{str};
  if(oldThread)
    m_thumb->moveToThread(oldThread);
  else
    m_thumb->moveToThread(inst.acquireThread());

  connect(
      m_thumb, &::Video::VideoThumbnailer::thumbnailReady, this,
      [this](const int64_t req, const int64_t flicks, QImage img) {
    if(req == m_lastRequestIndex)
    {
      if(m_images.size() > 50)
      {
        auto it = m_images.upper_bound(flicks);
        if(it != m_images.end())
          m_images.erase(it);
      }
      m_images[flicks] = std::move(img);
    }
    update();
      },
      Qt::QueuedConnection);

  widthChanged(width());
}

void View::widthChanged(qreal w)
{
  if(w < 10)
    return;

  // TODO we also have to fetch new frames if we scroll !
  const double frame_width = m_thumb->smallWidth;
  if(frame_width < 1.)
    return;

  auto view = ::getView(*this);
  if(!view)
    return;
  QPointF sceneDrawableTopLeft = view->mapToScene(-10, 0);
  QPointF sceneDrawableBottomRight
      = view->mapToScene(view->width() + 10, view->height() + 10);
  double itemDrawableLeft = this->mapFromScene(sceneDrawableTopLeft).x();
  double itemDrawableRight = this->mapFromScene(sceneDrawableBottomRight).x();

  const int count = (itemDrawableRight - itemDrawableLeft) / frame_width + 2;
  const int start = itemDrawableLeft / frame_width;

  const double flicks_advance = TimeVal::fromPixels(frame_width, m_zoom).impl;
  if(flicks_advance < ossia::flicks_per_millisecond<double>)
    return;

  QVector<int64_t> v;
  for(int i = 0; i < count; i++)
    v.push_back((i + start) * flicks_advance);

  m_lastRequestIndex++;
  m_thumb->requestThumbnails(m_lastRequestIndex, std::move(v));
  update();
}

void View::paint_impl(QPainter* painter) const
{
  if(m_images.empty())
    return;

  auto view = ::getView(*this);
  if(!view)
    return;
  QPointF sceneDrawableTopLeft = view->mapToScene(-10, 0);
  QPointF sceneDrawableBottomRight
      = view->mapToScene(view->width() + 10, view->height() + 10);
  double itemDrawableLeft = this->mapFromScene(sceneDrawableTopLeft).x();
  double itemDrawableRight = this->mapFromScene(sceneDrawableBottomRight).x();

  const double frame_width = m_thumb->smallWidth;
  const int count = (itemDrawableRight - itemDrawableLeft) / frame_width + 2;
  const int start = itemDrawableLeft / frame_width;
  const double flicks_advance = TimeVal::fromPixels(frame_width, m_zoom).impl;
  if(flicks_advance < ossia::flicks_per_millisecond<double>)
    return;

  auto& images = m_images;

  auto it = m_images.lower_bound(start * flicks_advance);
  if(it != images.cbegin())
    --it;

  for(int i = 0; i < count; i++)
  {
    int64_t flicks = (start + i) * flicks_advance;
    it = ossia::closest_next_element(
        it, images.cend(), flicks);
    if(it != images.end())
    {
      const double px = TimeVal{flicks}.toPixels(m_zoom);
      painter->drawImage(QPointF{px, 0.f}, it->second);
    }
  }
}

void View::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  dropReceived(event->pos(), *event->mimeData());
  event->accept();
}
}
