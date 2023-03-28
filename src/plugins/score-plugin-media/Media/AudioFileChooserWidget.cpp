#include "AudioFileChooserWidget.hpp"

#include <Media/MediaFileHandle.hpp>
#include <Media/Sound/QImagePool.hpp>
#include <Media/Sound/WaveformComputer.hpp>

#include <QGraphicsView>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsWaveformButton)
namespace score
{

QGraphicsWaveformButton::QGraphicsWaveformButton(QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_rect{0, 0, 400, 200}
    , m_computer{new Media::Sound::WaveformComputer{false}}
{
  setAcceptDrops(true);
  auto& skin = score::Skin::instance();

  setCursor(skin.CursorPointingHand);

  connect(
      m_computer, &Media::Sound::WaveformComputer::ready, this,
      [=](QVector<QImage*> img, Media::Sound::ComputedWaveform wf) {
    Media::Sound::QImagePool::instance().giveBack(m_images);
    m_images = std::move(img);

    // We display the image at the device ratio of the view
    if(auto view = ::getView(*this))
    {
      for(auto image : m_images)
      {
        image->setDevicePixelRatio(view->devicePixelRatioF());
      }
    }
    update();
      });
}

QGraphicsWaveformButton::~QGraphicsWaveformButton()
{
  delete m_computer;
  Media::Sound::QImagePool::instance().giveBack(m_images);
}

void QGraphicsWaveformButton::bang()
{
  m_pressed = true;
  QTimer::singleShot(32, this, [this] {
    m_pressed = false;
    update();
  });
  update();
}

void QGraphicsWaveformButton::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_pressed = true;
  pressed();
  update();
  event->accept();
}

void QGraphicsWaveformButton::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  m_pressed = true;
  pressed();
  update();
  event->accept();
}

void QGraphicsWaveformButton::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  m_pressed = false;
  pressed();
  update();
  event->accept();
}

void QGraphicsWaveformButton::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
}

void QGraphicsWaveformButton::dragMoveEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
}

void QGraphicsWaveformButton::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
}

void QGraphicsWaveformButton::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  auto m = event->mimeData();
  if(m->hasUrls())
  {
    auto url = m->urls().front().toLocalFile();
    if(QFileInfo(url).exists())
    {
      dropped(url);
    }
  }

  update();
  event->accept();
}

void QGraphicsWaveformButton::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  if(!m_images.empty())
  {
    painter->setRenderHint(QPainter::SmoothPixmapTransform, 0);
    int channels = m_images.size();
    auto h = m_rect.height() / channels;
    for(int i = 0; i < channels; i++)
    {
      painter->drawImage(QRectF{0, h * i, m_rect.width(), h}, *m_images[i]);
    }
    painter->setRenderHint(QPainter::SmoothPixmapTransform, 1);
  }
  else
  {
    auto& skin = score::Skin::instance();
    const QRectF brect = boundingRect().adjusted(1, 1, -1, -1);
    painter->setPen(skin.Base4.main.pen2);
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setFont(skin.Medium10Pt);
    painter->drawText(
        brect, "Click here or drop a sound file", QTextOption(Qt::AlignCenter));
  }
}

QRectF QGraphicsWaveformButton::boundingRect() const
{
  return m_rect;
}

void QGraphicsWaveformButton::on_finishedDecoding()
{
  double ms = ossia::flicks_per_second<double>
              * m_file->decodedSamples() / double(m_file->sampleRate());
  Media::Sound::WaveformRequest req{
      .file = m_file,
      .zoom = ms / this->m_rect.width(),
      .tempo_ratio = 1.,
      .layerSize = this->m_rect.size(),
      .devicePixelRatio = 1.,
      .view_x0 = 0,
      .view_xmax = 1e12,
      .startOffset = TimeVal{},
      .loopDuration = TimeVal::fromMsecs(
          1000. * m_file->decodedSamples() / double(m_file->sampleRate())),
      .loops = false,
      .colors = true};
  m_computer->recompute(req);
}

void QGraphicsWaveformButton::setFile(const QString& s)
{
  if(m_file)
    m_file->on_finishedDecoding
        .disconnect<&QGraphicsWaveformButton::on_finishedDecoding>(*this);
  m_string = std::move(s);
  m_file = Media::AudioFileManager::instance().get(m_string, 0);
  if(!m_file)
    return;

  m_file->on_finishedDecoding.connect<&QGraphicsWaveformButton::on_finishedDecoding>(
      *this);
}
}
