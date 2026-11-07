#include "Waveform.hpp"

#include <Media/MediaFileHandle.hpp>
#include <Media/Sound/WaveformRenderer.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/tools/FilePath.hpp>

#include <QGraphicsItem>
#include <QPainter>

namespace oscr
{
struct CustomItemWaveforms::Slot
{
  const halp::waveform_view* view{};
  std::string file;
  std::unique_ptr<Media::Sound::WaveformRenderer> renderer;
};

CustomItemWaveforms::CustomItemWaveforms(QGraphicsItem& item) noexcept
    : m_item{item}
{
}

CustomItemWaveforms::~CustomItemWaveforms() = default;

void CustomItemWaveforms::draw(
    QPainter& painter, halp::waveform_view& view, const QRectF& rect, double dpr)
{
  Slot* slot{};
  for(auto& s : m_slots)
    if(s->view == &view)
      slot = s.get();
  if(!slot)
  {
    auto s = std::make_unique<Slot>();
    s->view = &view;
    s->renderer = std::make_unique<Media::Sound::WaveformRenderer>(rect.size());
    QObject::connect(
        s->renderer.get(), &Media::Sound::WaveformRenderer::updated, s->renderer.get(),
        [item = &m_item] { item->update(); });
    slot = s.get();
    m_slots.push_back(std::move(s));
  }

  auto& r = *slot->renderer;
  if(slot->file != view.file)
  {
    slot->file = view.file;
    QString path = QString::fromStdString(view.file);
    if(m_doc && !path.isEmpty())
      path = score::locateFilePath(path, *m_doc);
    r.setFile(path);
  }
  r.setRange(view.begin, view.end);
  r.paint(painter, rect, dpr);

  view.ready = r.hasWaveform();
  view.duration = 0.;
  view.channels = 0;
  if(auto& f = r.file(); f && f->finishedDecoding() && f->sampleRate() > 0)
  {
    view.duration = double(f->decodedSamples()) / f->sampleRate();
    view.channels = int(f->channels());
  }
}
}
