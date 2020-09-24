#include "View.hpp"
#include <Gfx/Video/Process.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <ossia/detail/flicks.hpp>
#include <score/tools/std/Invoke.hpp>
#include <QPainter>
#include <Video/Thumbnailer.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/ThreadPool.hpp>
namespace Gfx::Video
{

View::View(const Model& model, QGraphicsItem* parent)
  : LayerView{parent}
  , m_model{model}
{
  con(model, &Model::pathChanged,
      this,  &View::onPathChanged);
  onPathChanged(model.path());
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
    oldThread = m_thumb->thread();
    m_thumb->deleteLater();
  }

  m_thumb = new ::Video::VideoThumbnailer{str};
  if(oldThread)
    m_thumb->moveToThread(oldThread);
  else
    m_thumb->moveToThread(inst.acquireThread());

  connect(m_thumb, &::Video::VideoThumbnailer::thumbnailReady,
          this, [this] (const int64_t flicks, QImage img) {
    m_images[flicks] = std::move(img);
    update();
  }, Qt::QueuedConnection);
}

void View::widthChanged(qreal w)
{
  double frame_width = m_thumb->smallWidth;
  int count = w / frame_width;
  double flicks_advance = ossia::flicks_per_millisecond<double> * m_zoom * frame_width;

  QVector<int64_t> v;
  for(int i = 0; i < count; i++)
    v.push_back(i * flicks_advance);

  m_thumb->requestThumbnails(std::move(v));
  m_images.clear();
  update();
}

void View::paint_impl(QPainter* painter) const
{
  painter->setRenderHint(QPainter::SmoothPixmapTransform, 0);
  for(const auto& [flicks, image] : m_images)
  {
    const double px = flicks / (ossia::flicks_per_millisecond<double> * m_zoom);
    painter->drawImage(QPointF{px, 0.f}, image);
  }
  painter->setRenderHint(QPainter::SmoothPixmapTransform, 1);
}
}
