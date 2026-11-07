#include "AudioFileChooserWidget.hpp"

#include <Media/MediaFileHandle.hpp>
#include <Media/Sound/WaveformRenderer.hpp>

#include <QGraphicsView>
#include <QMimeData>
#include <QPainter>
#include <QTimer>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsWaveformButton)
namespace score
{

QGraphicsWaveformButton::QGraphicsWaveformButton(QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_rect{0, 0, 400, 200}
    , m_waveform{std::make_unique<Media::Sound::WaveformRenderer>(m_rect.size(), false)}
{
  setAcceptDrops(true);
  auto& skin = score::Skin::instance();

  setCursor(skin.CursorPointingHand);

  connect(
      m_waveform.get(), &Media::Sound::WaveformRenderer::updated, this,
      [this] { update(); });
}

QGraphicsWaveformButton::~QGraphicsWaveformButton() = default;

bool QGraphicsWaveformButton::hasWaveform() const noexcept
{
  return m_waveform->hasWaveform();
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
    if(QFileInfo::exists(url))
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
  if(m_waveform->hasWaveform())
  {
    m_waveform->paint(*painter, m_rect, widget ? widget->devicePixelRatioF() : 1.);
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

void QGraphicsWaveformButton::setFile(const QString& s, const QString& absolute)
{
  m_string = std::move(s);
  // The RESOLVED path: the audio file manager opens what it is handed, so a
  // "<PROJECT>:" path reached it as a filename that does not exist and the
  // waveform stayed empty with no error anywhere.
  m_waveform->setFile(absolute.isEmpty() ? m_string : absolute);
  update();
}
}
