#include "WaveformRenderer.hpp"

#include <Media/MediaFileHandle.hpp>
#include <Media/Sound/QImagePool.hpp>

#include <score/model/Skin.hpp>

#include <ossia/detail/flicks.hpp>

#include <QPainter>

#include <algorithm>
#include <cmath>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Media::Sound::WaveformRenderer)

namespace Media::Sound
{
WaveformRenderer::WaveformRenderer(QSizeF size, bool threaded, QObject* parent)
    : QObject{parent}
    , m_computer{new WaveformComputer{threaded}}
    , m_size{size}
{
  connect(
      m_computer, &WaveformComputer::ready, this,
      [this](QVector<QImage*> img, ComputedWaveform wf) {
    m_computer->claim(img);
    QImagePool::instance().giveBack(m_images);
    m_images = std::move(img);
    m_wf = wf;
    updated();
  });

  // The colours are not part of the request.
  score::onSkinChange(this, [this] {
    m_lastRequest.reset();
    request();
  });
}

WaveformRenderer::~WaveformRenderer()
{
  if(m_file)
    m_file->on_finishedDecoding.disconnect<&WaveformRenderer::on_finishedDecoding>(
        *this);

  m_computer->stop();
  m_computer->deleteLater();
  m_computer = nullptr;
  QImagePool::instance().giveBack(m_images);
}

void WaveformRenderer::clearImages()
{
  QImagePool::instance().giveBack(m_images);
  m_images.clear();
  m_wf = {};
  m_lastRequest.reset();
}

void WaveformRenderer::setFile(const QString& path)
{
  if(m_file)
    m_file->on_finishedDecoding.disconnect<&WaveformRenderer::on_finishedDecoding>(
        *this);

  clearImages();
  m_path = path;
  m_file.reset();
  if(!m_path.isEmpty())
    m_file = Media::AudioFileManager::instance().get(m_path, 0);
  if(!m_file)
  {
    updated();
    return;
  }

  m_file->on_finishedDecoding.connect<&WaveformRenderer::on_finishedDecoding>(*this);

  // The manager caches files: this one may be decoded already, and then the
  // signal will not come again.
  if(m_file->finishedDecoding())
    on_finishedDecoding();
  else
    updated();
}

void WaveformRenderer::setRange(double begin, double end)
{
  begin = std::clamp(begin, 0., 1.);
  end = std::clamp(end, begin, 1.);
  if(begin == m_begin && end == m_end)
    return;
  m_begin = begin;
  m_end = end;
  request();
}

void WaveformRenderer::setSize(QSizeF size, double devicePixelRatio)
{
  if(size == m_size && devicePixelRatio == m_dpr)
    return;
  m_size = size;
  m_dpr = devicePixelRatio;
  request();
}

void WaveformRenderer::on_finishedDecoding()
{
  request();
}

void WaveformRenderer::request()
{
  if(!m_file || !m_file->finishedDecoding() || m_file->sampleRate() < 1)
    return;
  if(m_size.width() < 2. || m_size.height() < 2.)
    return;

  const int64_t samples = m_file->decodedSamples();
  if(samples <= 0)
    return;

  // The whole file is laid out on a layer wide enough that [begin, end] of it
  // fills the widget, and only that window of the layer is rendered.
  const double span = std::max(m_end - m_begin, 1e-6);
  const double layerWidth = m_size.width() / span;
  const double fileFlicks
      = ossia::flicks_per_second<double> * samples / double(m_file->sampleRate());

  m_layerWidth = layerWidth;
  m_zoom = fileFlicks / layerWidth;

  WaveformRequest req{
      .file = m_file,
      .zoom = m_zoom,
      .tempo_ratio = 1.,
      .layerSize = QSizeF{layerWidth, m_size.height()},
      .devicePixelRatio = m_dpr,
      // Whole pixels, so that identical requests compare equal
      .view_x0 = std::floor(m_begin * layerWidth),
      .view_xmax = std::ceil(m_end * layerWidth),
      .startOffset = TimeVal{},
      .loopDuration = TimeVal::fromMsecs(1000. * samples / double(m_file->sampleRate())),
      .loops = false,
      .colors = true};

  if(m_lastRequest == req)
    return;
  m_lastRequest = req;
  m_computer->recompute(std::move(req));
}

void WaveformRenderer::paint(QPainter& painter, const QRectF& rect, double devicePixelRatio)
{
  setSize(rect.size(), devicePixelRatio);

  const int channels = m_images.size();
  if(channels == 0 || m_zoom <= 0. || m_wf.zoom <= 0.)
    return;

  // Images may have been rendered for a previous zoom: they are stretched to
  // the current one until the new ones arrive.
  const double ratio = m_wf.zoom / m_zoom;
  const double x0 = rect.x() + m_wf.x0 * ratio - m_begin * m_layerWidth;
  const double w = (m_wf.xf - m_wf.x0) * ratio;
  const double h = rect.height() / channels;

  painter.save();
  painter.setClipRect(rect, Qt::IntersectClip);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
  for(int i = 0; i < channels; i++)
    painter.drawImage(QRectF{x0, rect.y() + h * i, w, h}, *m_images[i]);
  painter.restore();
}
}
