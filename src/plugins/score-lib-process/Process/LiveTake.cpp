#include <ossia/detail/flicks.hpp>
#include "LiveTake.hpp"

#include <QFontMetrics>
#include <QPainter>

#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <score/document/DocumentInterface.hpp>
#include <QPainterPath>
#include <QRectF>

#include <wobjectimpl.h>

#include <algorithm>
#include <cmath>

W_OBJECT_IMPL(Process::LiveTake)
W_OBJECT_IMPL(Process::LiveTakes)
W_OBJECT_IMPL(Process::RecordArm)

namespace Process
{
namespace
{
constexpr int64_t flicksPerSecond = ossia::flicks_per_second<int64_t>;
}

LiveTake::LiveTake(QObject* itv, Kind k, QString n)
    : interval{itv}
    , kind{k}
    , name{std::move(n)}
{
}

LiveTake::~LiveTake() = default;

QString LiveTake::status() const
{
  if(!recording)
    return countingIn ? tr("Counting in") : tr("Waiting");
  const int64_t s = std::max<int64_t>(0, end - start) / flicksPerSecond;
  return tr("Recording %1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
}

void LiveTake::paintContent(
    QPainter& p, const QRectF& r, int64_t from, int64_t to) const
{
  if(to <= from || r.width() < 2 || r.height() < 2)
    return;
  const double span = double(to - from);
  const auto xOf = [&](int64_t d) { return r.left() + (d - from) / span * r.width(); };
  p.save();
  p.setClipRect(r);
  switch(kind)
  {
    case Audio:
    {
      if(peaks.empty())
        break;
      p.setPen(QPen{QColor{255, 200, 190}, 1.});
      const double mid = r.center().y();
      const double half = r.height() / 2. - 1.;
      const int x0 = int(std::max(r.left(), xOf(start)));
      const int x1 = int(std::min(r.right(), xOf(end)));
      for(int x = x0; x < x1; x++)
      {
        const double d0 = from + (x - r.left()) / r.width() * span;
        const double d1 = from + (x + 1 - r.left()) / r.width() * span;
        int64_t i0 = int64_t((d0 - start) / peakPeriod);
        int64_t i1 = int64_t((d1 - start) / peakPeriod);
        i0 = std::clamp<int64_t>(i0, 0, int64_t(peaks.size()) - 1);
        i1 = std::clamp<int64_t>(i1, i0 + 1, int64_t(peaks.size()));
        float m = 0.f;
        for(int64_t i = i0; i < i1; i++)
          m = std::max(m, peaks[i]);
        const double h = std::max(0.5, std::min(1.f, m) * half);
        p.drawLine(QPointF(x + 0.5, mid - h), QPointF(x + 0.5, mid + h));
      }
      break;
    }
    case Midi:
    {
      if(notes.empty())
        break;
      int lo = 127, hi = 0;
      for(auto& n : notes)
      {
        lo = std::min<int>(lo, n.pitch);
        hi = std::max<int>(hi, n.pitch);
      }
      // At least an octave, so few notes do not fill the height
      if(hi - lo < 12)
      {
        lo = std::max(0, (lo + hi) / 2 - 6);
        hi = lo + 12;
      }
      const double row = r.height() / double(hi - lo + 1);
      p.setPen(Qt::NoPen);
      for(auto& n : notes)
      {
        const int64_t e = n.end < 0 ? end : n.end;
        const double x0 = xOf(n.start);
        const double x1 = std::max(x0 + 2., xOf(e));
        const double y = r.bottom() - (n.pitch - lo + 1) * row;
        p.setBrush(QColor::fromHsv(5, 40 + n.velocity, 255));
        p.drawRect(QRectF{x0, y, x1 - x0, std::max(1.5, row - 1.)});
      }
      break;
    }
    case Values:
    {
      if(values.empty())
        break;
      double lo = values.front().value, hi = lo;
      for(auto& v : values)
      {
        lo = std::min(lo, v.value);
        hi = std::max(hi, v.value);
      }
      if(hi - lo < 1e-9)
      {
        lo -= 0.5;
        hi += 0.5;
      }
      const auto yOf = [&](double v) {
        return r.bottom() - 2. - (v - lo) / (hi - lo) * (r.height() - 4.);
      };
      QPainterPath path;
      path.moveTo(xOf(values.front().date), yOf(values.front().value));
      for(std::size_t i = 1; i < values.size(); i++)
      {
        // Each value holds until the next
        path.lineTo(xOf(values[i].date), yOf(values[i - 1].value));
        path.lineTo(xOf(values[i].date), yOf(values[i].value));
      }
      path.lineTo(xOf(end), yOf(values.back().value));
      p.setPen(QPen{QColor{255, 200, 190}, 1.5});
      p.setBrush(Qt::NoBrush);
      p.drawPath(path);
      break;
    }
    case Video:
    {
      // Film frames, one a second
      p.setPen(QPen{QColor{255, 200, 190, 160}, 1.});
      p.setBrush(Qt::NoBrush);
      for(int64_t d = start; d < end; d += flicksPerSecond)
      {
        const double x0 = xOf(d) + 1.;
        const double x1 = std::min(xOf(std::min(end, d + flicksPerSecond)) - 1., r.right());
        if(x1 > x0 + 2.)
          p.drawRect(QRectF{x0, r.top() + 3., x1 - x0, r.height() - 6.});
      }
      break;
    }
  }
  p.restore();
}

RecordArm& RecordArm::instance()
{
  static RecordArm a;
  return a;
}

bool RecordArm::armed(const ProcessModel& p) const noexcept
{
  for(const auto& a : m_armed)
    if(a == &p)
      return true;
  return false;
}

void RecordArm::setArmed(const ProcessModel& p, bool b)
{
  std::erase_if(m_armed, [](const auto& a) { return !a; });
  if(b == armed(p))
    return;
  if(b)
    m_armed.emplace_back(&p);
  else
    std::erase_if(m_armed, [&](const auto& a) { return a == &p; });
  armedChanged(&p, b);
}

std::vector<const ProcessModel*> RecordArm::armedIn(const QObject& document) const
{
  std::vector<const ProcessModel*> r;
  for(const auto& a : m_armed)
    if(a && score::IDocument::documentFromObject(*a) == &document)
      r.push_back(a.data());
  return r;
}

bool RecordArm::canArm(const ProcessModel& p) noexcept
{
  for(auto* out : p.outlets())
    switch(out->type())
    {
      case PortType::Audio:
      case PortType::Midi:
        return true;
      default:
        break;
    }
  return false;
}

LiveTakes& LiveTakes::instance()
{
  static LiveTakes t;
  return t;
}

void LiveTakes::add(LiveTake* t)
{
  m_takes.push_back(t);
  added(t);
}

void LiveTakes::remove(LiveTake* t)
{
  auto it = std::find(m_takes.begin(), m_takes.end(), t);
  if(it == m_takes.end())
    return;
  m_takes.erase(it);
  removed(t);
}

namespace
{
const QColor recordRed{220, 60, 50};

void paintHeader(QPainter& p, const LiveTake& t, const QRectF& r)
{
  p.setPen(Qt::NoPen);
  p.setBrush(recordRed);
  const double d = std::min(9., r.height() - 6.);
  if(t.recording)
    p.drawEllipse(QRectF{r.left() + 5., r.center().y() - d / 2., d, d});
  QFont f = p.font();
  f.setPointSizeF(8.5);
  f.setBold(true);
  p.setFont(f);
  p.setPen(Qt::white);
  const QString st = t.status();
  const double sw = QFontMetrics{f}.horizontalAdvance(st) + 6.;
  const QRectF text{r.left() + d + 9., r.top(), r.width() - d - 12. - sw, r.height()};
  p.drawText(
      text, Qt::AlignVCenter | Qt::AlignLeft,
      QFontMetrics{f}.elidedText(t.name, Qt::ElideMiddle, int(text.width())));
  f.setBold(false);
  p.setFont(f);
  p.drawText(
      QRectF{r.right() - sw, r.top(), sw - 3., r.height()},
      Qt::AlignVCenter | Qt::AlignRight, st);
}
}

LiveTakeNode::LiveTakeNode(LiveTake& take, QGraphicsItem* parent)
    : QGraphicsObject{parent}
    , m_take{&take}
{
  setZValue(10.);
  setAcceptedMouseButtons(Qt::NoButton);
  connect(&take, &LiveTake::changed, this, [this] { update(); });
}

QRectF LiveTakeNode::boundingRect() const
{
  return {0., 0., width, height};
}

void LiveTakeNode::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
  if(!m_take)
    return;
  auto& t = *m_take;
  p->setRenderHint(QPainter::Antialiasing, true);
  const QRectF r = boundingRect().adjusted(0.5, 0.5, -0.5, -0.5);
  p->setPen(QPen{recordRed, 1.5, Qt::DashLine});
  p->setBrush(QColor{40, 20, 20, 230});
  p->drawRoundedRect(r, 3., 3.);
  paintHeader(*p, t, QRectF{r.left(), r.top(), r.width(), 20.});
  // All of it, at least a few seconds wide
  const int64_t to = std::max(t.end, t.start + 4 * flicksPerSecond);
  p->setRenderHint(QPainter::Antialiasing, false);
  t.paintContent(*p, r.adjusted(4., 22., -4., -4.), t.start, to);
}

LiveTakeSpan::LiveTakeSpan(LiveTake& take, QGraphicsItem* parent)
    : QGraphicsObject{parent}
    , m_take{&take}
{
  setZValue(10.);
  setAcceptedMouseButtons(Qt::NoButton);
  connect(&take, &LiveTake::changed, this, [this] { layout(); });
  layout();
}

void LiveTakeSpan::setGeometry(double zoom, double top, double height)
{
  m_zoom = zoom > 0. ? zoom : 1.;
  m_top = top;
  m_height = std::max(24., height);
  layout();
}

void LiveTakeSpan::layout()
{
  QRectF r;
  if(m_take)
  {
    const double x0 = m_take->start / m_zoom;
    const double x1 = std::max(x0 + 60., m_take->end / m_zoom);
    r = QRectF{x0, m_top, x1 - x0, m_height};
  }
  if(r != m_rect)
  {
    prepareGeometryChange();
    m_rect = r;
  }
  update();
}

QRectF LiveTakeSpan::boundingRect() const
{
  return m_rect;
}

void LiveTakeSpan::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
  if(!m_take || m_rect.isEmpty())
    return;
  auto& t = *m_take;
  const QRectF r = m_rect.adjusted(0.5, 0.5, -0.5, -0.5);
  p->setPen(QPen{recordRed, 1.5, Qt::DashLine});
  p->setBrush(QColor{60, 20, 20, 200});
  p->drawRect(r);
  const QRectF header{r.left(), r.top(), std::max(r.width(), 160.), 18.};
  p->setRenderHint(QPainter::Antialiasing, true);
  paintHeader(*p, t, header.intersected(r.adjusted(0, 0, 200, 0)));
  p->setRenderHint(QPainter::Antialiasing, false);
  // At the box's scale: what the take will be once placed
  const QRectF content = r.adjusted(1., 19., -1., -2.);
  t.paintContent(
      *p, content, t.start, t.start + int64_t(content.width() * m_zoom));
}
}
