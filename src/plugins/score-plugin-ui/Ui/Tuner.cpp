#include "Tuner.hpp"

#include <QPainter>

#include <Ui/Monitor/LayerHelpers.hpp>
#include <Ui/Monitor/MidiText.hpp>
#include <Ui/Monitor/Paint.hpp>

#include <cmath>
#include <span>

namespace Ui::Tuner
{
Layer::Layer(
    const Process::ProcessModel& process, const Process::Context& doc,
    QGraphicsItem* parent)
    : Process::EffectLayerView{parent}
{
  using namespace Ui::Monitor;
  setAcceptedMouseButtons({});
  showInlet(process, 0, doc, this, this);
  m_reference = controlInlet(process, 1);
  repaintOn(this, m_reference);

  m_link = new Process::MonitorLink{
      process, *this, [this](Process::Monitor& m) { refresh(static_cast<TunerMonitor&>(m)); },
      [this](Process::Monitor*) {
    m_freq = 0.;
    m_signal = false;
    update();
  }, this};
}

double Layer::reference() const
{
  return Ui::Monitor::floatValue(m_reference, 440.f);
}

void Layer::refresh(TunerMonitor& mon)
{
  auto& ring = mon.ring;
  ring.provision();
  const int nch = std::min(ring.channels(), 8);
  if(nch == 0)
    return;

  // The channels over the same frames, mixed
  m_in.resize(nch);
  for(auto& in : m_in)
    in.resize(frame);
  uint64_t first{};
  if(!ring.read_latest(0, m_in[0], first))
    return;
  for(int c = 1; c < nch; c++)
    if(!ring.read(c, first, m_in[c]))
      return;
  m_mix.assign(frame, 0.f);
  for(const auto& in : m_in)
    for(std::size_t i = 0; i < frame; i++)
      m_mix[i] += in[i] / nch;

  m_signal = true;
  m_freq = m_pitch(m_mix, ring.rate());
  update();
}

void Layer::paint_impl(QPainter* p) const
{
  using namespace Ui::Monitor;
  p->save();
  p->fillRect(boundingRect(), background());
  const QRectF r = boundingRect().adjusted(8, 18, -8, -6);
  if(!m_signal)
  {
    drawLabel(*p, boundingRect(), Qt::AlignCenter, QStringLiteral("Tuner\nno signal"));
    p->restore();
    return;
  }
  p->setRenderHint(QPainter::Antialiasing, true);

  // Note and cents against the reference for A4
  QString note;
  double cents = 0.;
  if(m_freq > 0.)
  {
    const double midi = 69. + 12. * std::log2(m_freq / reference());
    const int n = int(std::lround(midi));
    cents = (midi - n) * 100.;
    note = midiNoteName(n);
  }

  QFont big = score::Skin::instance().Bold12Pt;
  big.setPixelSize(int(std::clamp(r.height() * 0.35, 12., 72.)));
  p->setFont(big);
  p->setPen(note.isEmpty() ? textColor() : QColor(235, 235, 235));
  const QRectF top{r.left(), r.top(), r.width(), r.height() * 0.5};
  p->drawText(top, Qt::AlignCenter, note.isEmpty() ? QStringLiteral("-") : note);
  if(m_freq > 0.)
    drawLabel(
        *p, QRectF(r.left(), top.bottom(), r.width(), 12), Qt::AlignCenter,
        QString::number(m_freq, 'f', 2) + " Hz");

  // Cents, -50 to +50
  const QRectF scale{
      r.left() + 6, top.bottom() + 16, r.width() - 12,
      std::min(24., r.bottom() - top.bottom() - 32)};
  if(scale.height() < 6)
  {
    p->restore();
    return;
  }
  p->fillRect(scale, QColor(30, 30, 32));
  for(int c = -50; c <= 50; c += 10)
  {
    const double x = scale.left() + (c + 50) / 100. * scale.width();
    p->setPen(c == 0 ? QPen(QColor(76, 175, 80), 2) : QPen(gridColor(), 1));
    p->drawLine(QPointF{x, scale.top()}, QPointF{x, scale.bottom()});
  }
  if(!note.isEmpty())
  {
    const double x = scale.left() + (std::clamp(cents, -50., 50.) + 50) / 100. * scale.width();
    const QColor col = std::abs(cents) < 5    ? QColor(76, 175, 80)
                       : std::abs(cents) < 15 ? QColor(240, 200, 40)
                                              : QColor(235, 45, 40);
    p->fillRect(QRectF(x - 2, scale.top() - 3, 4, scale.height() + 6), col);
    drawLabel(
        *p, QRectF(scale.left(), scale.bottom() + 3, scale.width(), 12), Qt::AlignCenter,
        QStringLiteral("%1%2 cents").arg(cents >= 0 ? "+" : "").arg(cents, 0, 'f', 1));
  }
  p->restore();
}
}
