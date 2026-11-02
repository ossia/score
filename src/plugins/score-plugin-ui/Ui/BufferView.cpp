#include "BufferView.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QLocale>
#include <QPainter>
#include <QtGui/private/qrhi_p.h>

#include <Ui/Monitor/BufferGatherPass.hpp>
#include <Ui/Monitor/Paint.hpp>

#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>

namespace Ui::BufferView
{
namespace
{
const QString component_names[4]{
    QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("z"), QStringLiteral("w")};

//! A readback that has not completed in this long never will: the batch
//! that carried it was dropped.
constexpr auto lost_readback = std::chrono::seconds{1};

bool finite(float v) noexcept
{
  return std::isfinite(v);
}

QString ordinal(int64_t n)
{
  const int64_t tens = n % 100;
  const char* suffix = "th";
  if(tens < 11 || tens > 13)
    switch(n % 10)
    {
      case 1:
        suffix = "st";
        break;
      case 2:
        suffix = "nd";
        break;
      case 3:
        suffix = "rd";
        break;
    }
  return QString::number(n) + QLatin1String(suffix);
}

QString fallbackReason(Fallback f)
{
  switch(f)
  {
    case Fallback::NoCompute:
      return QStringLiteral("read as one range: no compute shaders");
    case Fallback::NotStorage:
      return QStringLiteral("read as one range: not a storage buffer");
    case Fallback::TooLarge:
      return QStringLiteral("read as one range: too large to bind");
    case Fallback::Failed:
      return QStringLiteral("read as one range: the gather pass failed");
    case Fallback::None:
      break;
  }
  return {};
}
}

struct Node::Readback
{
  QRhiBufferReadbackResult result;
  //! How the elements lie in `result`
  Layout local;
  //! The layout as the controls give it
  Layout layout;
  Ui::Monitor::Buffer::Sampling sampling;
  int64_t available{};
  int64_t buffer_bytes{};
  Fallback fallback{};
  //! Whether `result` starts with the header of a measure
  bool measured{};
  bool pending{};
  std::chrono::steady_clock::time_point requested{};

  // What the last readback read: a change is read at once, not at the next
  // refresh
  QRhiBuffer* handle{};
  int64_t byte_offset{};
  int64_t byte_size{};
  Layout read_layout{};
  int64_t max_elements{};
  int64_t first{};
  bool overview{};
  bool empty{};

  Ui::Monitor::Buffer::GatherPass gather;

  //! Swapped with the monitor's: the vectors keep their storage
  Frame frame;
};

Node::Node()
    : m_readback{std::make_shared<Readback>()}
{
}

Node::~Node() = default;

Layout Node::layout() const noexcept
{
  return Layout{
      .type = ElementType(std::clamp(int(inputs.type.value), 0, int(ElementType::UInt8))),
      .components = std::clamp(inputs.components.value, 1, 4),
      .stride = std::max(0, inputs.stride.value),
      .offset = std::max(0, inputs.offset.value)};
}

void Node::publishEmpty(
    int64_t buffer_bytes, int64_t available, int64_t first, const Layout& l)
{
  auto& rb = *m_readback;
  auto& f = rb.frame;
  f.values.clear();
  f.count = 0;
  f.available = available;
  f.first = first;
  f.step = 1;
  f.buffer_bytes = buffer_bytes;
  f.read_bytes = 0;
  f.layout = l;
  f.fallback = Fallback::None;
  f.capped = false;
  f.exact = false;
  if(monitor)
    monitor->frames.produce(f);
  rb.empty = true;
}

void Node::inputAboutToFinish(
    score::gfx::RenderList& r, const score::gfx::Port& p, QRhiResourceUpdateBatch*& res)
{
  using namespace Ui::Monitor::Buffer;
  using clock = std::chrono::steady_clock;
  auto& rb = *m_readback;
  if(!monitor || !monitor->watched() || !r.state.rhi)
    return;
  const auto now = clock::now();
  if(rb.pending)
  {
    if(now - rb.requested < lost_readback)
      return;
    rb.pending = false;
  }

  score::gfx::BufferView buf;
  for(auto* e : p.edges)
    if((buf = r.bufferForInput(*e)))
      break;

  const Layout l = layout();
  if(!buf || buf.byte_size <= 0 || buf.byte_offset < 0
     || buf.byte_offset >= int64_t(buf.handle->size()))
  {
    if(!rb.empty || rb.handle)
      publishEmpty(0, 0, 0, l);
    rb.handle = nullptr;
    return;
  }

  const int64_t max_elements
      = std::clamp<int64_t>(inputs.max_elements.value, 1, max_view_elements);
  const int64_t first = std::max(0, inputs.first.value);
  const bool overview = inputs.overview.value;
  const bool changed = buf.handle != rb.handle || buf.byte_offset != rb.byte_offset
                       || buf.byte_size != rb.byte_size || !(l == rb.read_layout)
                       || max_elements != rb.max_elements || first != rb.first
                       || overview != rb.overview;
  if(!changed
     && now - rb.requested < std::chrono::milliseconds{
            monitor->interval.load(std::memory_order_relaxed)})
    return;
  rb.handle = buf.handle;
  rb.byte_offset = buf.byte_offset;
  rb.byte_size = buf.byte_size;
  rb.read_layout = l;
  rb.max_elements = max_elements;
  rb.first = first;
  rb.overview = overview;
  rb.requested = now;

  // The range a producer publishes may run past the end of its buffer
  const int64_t buffer_size = int64_t(buf.handle->size());
  const int64_t bytes = std::min<int64_t>(buf.byte_size, buffer_size - buf.byte_offset);
  const int64_t available = elementCount(l, bytes);
  const int64_t e = elementBytes(l);
  const int64_t origin = buf.byte_offset + l.offset;

  rb.layout = l;
  rb.available = available;
  rb.buffer_bytes = buf.byte_size;
  rb.result.completed = [this] { decodeReadback(); };

  auto& rhi = *r.state.rhi;
  auto* cb = r.currentCommandBuffer();
  Fallback fallback = cb ? rb.gather.check(rhi, *buf.handle) : Fallback::Failed;
  if(fallback == Fallback::None)
  {
    // An element in the last bytes of a buffer whose size is not a multiple
    // of 4 is out of the shader's reach
    const int64_t readable = std::min(
        bytes, gatherReadable(buffer_size) - buf.byte_offset);
    const int64_t gathered = elementCount(l, readable);
    const int64_t cap = max_readback_bytes - (overview ? gather_header_bytes : 0);
    const Sampling s = sampleElements(gathered, first, max_elements, e, cap, overview);
    if(s.count == 0)
    {
      publishEmpty(buf.byte_size, available, s.first, l);
      return;
    }
    const GatherPlan plan = planGather(
        l, origin, buffer_size, s, overview, r.state.caps.maxStorageBufferSize,
        rhi.resourceLimit(QRhi::MaxThreadGroupsPerDimension));
    if(!plan.valid)
      fallback = Fallback::TooLarge;
    else if(!rb.gather.record(r.state, *cb, res, *buf.handle, plan, rb.result))
      fallback = Fallback::Failed;
    else
    {
      rb.local = Layout{
          .type = l.type,
          .components = l.components,
          .stride = 0,
          .offset = overview ? gather_header_bytes : 0};
      rb.sampling = s;
      rb.available = gathered;
      rb.fallback = Fallback::None;
      rb.measured = overview;
      rb.pending = true;
      return;
    }
  }

  // One range from the first element read to the last, as much of it as a
  // readback holds
  if(!res)
    return;
  Sampling s = sampleElements(available, first, max_elements, e, max_readback_bytes, false);
  if(s.count == 0)
  {
    publishEmpty(buf.byte_size, available, s.first, l);
    return;
  }
  const int64_t stride = strideOf(l);
  const int64_t start = origin + s.first * stride;
  // From and to 4-byte boundaries: some backends copy buffers by words
  const int64_t begin = start & ~int64_t(3);
  const int64_t fit = contiguousFit(stride, e, start - begin, max_readback_bytes);
  if(fit < s.count)
  {
    s.count = std::max<int64_t>(fit, 1);
    s.capped = true;
  }
  const int64_t last = start + (s.count - 1) * stride + e;
  const int64_t end = std::min(buf.byte_offset + bytes, (last + 3) & ~int64_t(3));

  rb.local = l;
  rb.local.offset = start - begin;
  rb.sampling = s;
  rb.fallback = fallback;
  rb.measured = false;
  rb.pending = true;
  res->readBackBuffer(buf.handle, quint32(begin), quint32(end - begin), &rb.result);
}

void Node::decodeReadback()
{
  using namespace Ui::Monitor::Buffer;
  auto& rb = *m_readback;
  rb.pending = false;
  auto& f = rb.frame;
  const int n = components(rb.layout);
  const QByteArray& data = rb.result.data;
  const std::span<const unsigned char> bytes{
      reinterpret_cast<const unsigned char*>(data.constData()), std::size_t(data.size())};
  f.values.resize(std::size_t(rb.sampling.count * n));
  f.count = decode(rb.local, bytes, f.values);
  f.values.resize(std::size_t(f.count * n));
  f.exact = rb.measured && bytes.size() >= std::size_t(gather_header_bytes);
  if(f.exact)
  {
    uint32_t header[gather_header_words];
    std::memcpy(header, bytes.data(), sizeof(header));
    f.range = readGatherHeader(header, n);
  }
  f.available = rb.available;
  f.first = rb.sampling.first;
  f.step = rb.sampling.step;
  f.capped = rb.sampling.capped;
  f.fallback = rb.fallback;
  f.buffer_bytes = rb.buffer_bytes;
  f.read_bytes = data.size();
  f.layout = rb.layout;
  rb.empty = false;
  if(monitor)
    monitor->frames.produce(f);
}

void Node::release(score::gfx::RenderList& r)
{
  auto& rb = *m_readback;
  // The readback writes into the result when its frame completes
  if(rb.pending && r.state.rhi)
    r.state.rhi->finish();
  rb.pending = false;
  rb.handle = nullptr;
  rb.gather.release();
}

Layer::Layer(
    const Process::ProcessModel& process, const Process::Context& doc,
    QGraphicsItem* parent)
    : Process::EffectLayerView{parent}
{
  using namespace Ui::Monitor;
  // For the double click; a press goes on to the node
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
  showInlet(process, 0, doc, this, this);
  ModeSwitch::make(process, doc, this, this);
  m_mode = controlInlet(process, 1);
  m_width = controlInlet(process, 6);
  m_autoscale = controlInlet(process, 7);
  repaintOn(this, m_mode, m_width, m_autoscale);
  for(auto* c : {m_width, m_autoscale})
    if(c)
      connect(c, &Process::ControlInlet::valueChanged, this, [this] { m_imageDirty = true; });
  reset();

  m_link = new Process::MonitorLink{
      process, *this,
      [this](Process::Monitor& m) { refresh(static_cast<BufferMonitor&>(m)); },
      [this](Process::Monitor*) { reset(); }, this};
}

Mode Layer::mode() const
{
  return Ui::Monitor::enumValue(m_mode, Mode::Curves, Mode::Histogram);
}

bool Layer::autoscale() const
{
  return Ui::Monitor::boolValue(m_autoscale, true);
}

int Layer::components() const noexcept
{
  return Ui::Monitor::Buffer::components(m_frame.layout);
}

std::pair<float, float> Layer::rangeOf(int c) const
{
  if(!autoscale())
    return {0.f, 1.f};
  float lo = m_lo[c], hi = m_hi[c];
  if(!(lo <= hi))
    return {0.f, 1.f};
  if(lo == hi)
  {
    // A constant that reads as a level keeps it: an opaque alpha stays
    // opaque, a cleared channel stays black
    if(lo >= 0.f && lo <= 1.f)
      return {0.f, 1.f};
    const float pad = std::max(0.5f, std::abs(lo) * 0.05f);
    lo -= pad;
    hi += pad;
  }
  // Out to a 1, 2 or 5 step, a quarter of the span or less: the scale ends
  // on round values, which the axes then label
  const double raw = (double(hi) - lo) / 4.;
  const double mag = std::pow(10., std::floor(std::log10(raw)));
  double step = mag;
  for(double m : {1., 2., 2.5, 5., 10.})
    if((step = m * mag) >= raw)
      break;
  if(!std::isfinite(step) || step <= 0.)
    return {lo, hi};
  return {float(std::floor(lo / step) * step), float(std::ceil(hi / step) * step)};
}

void Layer::reset()
{
  m_has = false;
  m_frame.count = 0;
  m_frame.available = 0;
  m_frame.buffer_bytes = 0;
  m_frame.values.clear();
  m_lo.fill(std::numeric_limits<float>::max());
  m_hi.fill(std::numeric_limits<float>::lowest());
  m_frameLo = m_lo;
  m_frameHi = m_hi;
  m_nonFinite = 0;
  m_imageDirty = true;
  update();
}

void Layer::refresh(BufferMonitor& mon)
{
  mon.interval.store(Process::MonitorLink::refreshInterval(), std::memory_order_relaxed);
  const Layout before = m_frame.layout;
  const bool had = m_has;
  if(!mon.frames.consume(m_frame))
    return;
  m_has = true;
  measure();
  // The scales hold what the elements spanned since the layout was set or
  // the user fitted them: they do not jump with each frame
  const bool relayout = !had || !(before == m_frame.layout);
  if(m_frame.count > 0)
    for(int c = 0; c < 4; c++)
    {
      if(relayout || !(m_lo[c] <= m_hi[c]))
      {
        m_lo[c] = m_frameLo[c];
        m_hi[c] = m_frameHi[c];
      }
      else
      {
        m_lo[c] = std::min(m_lo[c], m_frameLo[c]);
        m_hi[c] = std::max(m_hi[c], m_frameHi[c]);
      }
    }
  m_imageDirty = true;
  update();
}

void Layer::measure()
{
  if(m_frame.exact)
  {
    m_frameLo = m_frame.range.lo;
    m_frameHi = m_frame.range.hi;
    m_nonFinite = m_frame.range.non_finite;
    return;
  }
  m_frameLo.fill(std::numeric_limits<float>::max());
  m_frameHi.fill(std::numeric_limits<float>::lowest());
  m_nonFinite = 0;
  const int n = components();
  const float* v = m_frame.values.data();
  for(int64_t i = 0; i < m_frame.count; i++)
    for(int c = 0; c < n; c++, v++)
    {
      if(!finite(*v))
      {
        m_nonFinite++;
        continue;
      }
      m_frameLo[c] = std::min(m_frameLo[c], *v);
      m_frameHi[c] = std::max(m_frameHi[c], *v);
    }
}

void Layer::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  if(!m_doubleClick(*ev))
  {
    ev->ignore();
    return;
  }
  ev->accept();
  if(!m_has || m_frame.count == 0)
    return;
  m_lo = m_frameLo;
  m_hi = m_frameHi;
  m_imageDirty = true;
  update();
}

void Layer::hoverMoveEvent(QGraphicsSceneHoverEvent* ev)
{
  m_hover = ev->pos();
  update();
}

void Layer::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
  m_hover.reset();
  update();
}

int64_t Layer::indexOf(int64_t i) const noexcept
{
  return m_frame.first + i * m_frame.step;
}

QString Layer::formatValue(float v) const
{
  if(!finite(v))
    return QString::number(v);
  if(Ui::Monitor::Buffer::integral(m_frame.layout.type))
    return QString::number(qint64(v));
  return QString::number(v, 'g', 6);
}

QRectF Layer::plotRect() const
{
  // Wide enough for the labels of the value axes
  using namespace Ui::Monitor;
  const QFontMetricsF fm{smallFont()};
  const int n = components();
  const QRectF r = boundingRect();
  const double rh = std::max(1., (r.height() - 34.) / n);
  double w = 0.;
  for(int c = 0; c < n; c++)
  {
    const auto [lo, hi] = rangeOf(c);
    for(const auto& t : linearTicks(lo, hi, rh, 18.))
      w = std::max(w, fm.horizontalAdvance(t.label));
  }
  return r.adjusted(std::clamp(w + 6., 24., 72.), 20, -4, -14);
}

void Layer::paint_impl(QPainter* p) const
{
  using namespace Ui::Monitor;
  p->save();
  p->fillRect(boundingRect(), background());
  if(!m_has || m_frame.buffer_bytes == 0)
    drawLabel(*p, boundingRect(), Qt::AlignCenter, QStringLiteral("Buffer view\nno buffer"));
  else if(m_frame.count == 0 && m_frame.available > 0)
    drawLabel(
        *p, boundingRect(), Qt::AlignCenter,
        QStringLiteral("%1 elements\nnone from element %2")
            .arg(m_frame.available)
            .arg(m_frame.first));
  else if(m_frame.count == 0)
    drawLabel(
        *p, boundingRect(), Qt::AlignCenter,
        QStringLiteral("%1 bytes\nnot one element at this offset").arg(m_frame.buffer_bytes));
  else
    switch(mode())
    {
      case Mode::Curves:
        paintCurves(p);
        break;
      case Mode::Table:
        paintTable(p);
        break;
      case Mode::Image:
        paintImage(p);
        break;
      case Mode::Histogram:
        paintHistogram(p);
        break;
    }
  p->restore();
}

void Layer::paintInfo(
    QPainter* p, const QRectF& plot, const QStringList& extra, bool bottom) const
{
  using namespace Ui::Monitor;
  if(!isUnderMouse())
    return;
  QStringList lines;
  lines << QStringLiteral("%1 bytes, %2 elements")
               .arg(m_frame.buffer_bytes)
               .arg(m_frame.available);
  const int64_t last = m_frame.first + (m_frame.count - 1) * m_frame.step;
  if(m_frame.step > 1)
    lines << QStringLiteral("every %1 element, %2 to %3")
                 .arg(ordinal(m_frame.step))
                 .arg(m_frame.first)
                 .arg(last);
  else if(m_frame.count < m_frame.available)
    lines << QStringLiteral("elements %1 to %2").arg(m_frame.first).arg(last);
  if(m_frame.capped)
    lines << QStringLiteral("fewer elements: 16 MiB per readback");
  if(m_frame.fallback != Fallback::None)
    lines << fallbackReason(m_frame.fallback);
  lines << QStringLiteral("%1 read back")
               .arg(QLocale::c().formattedDataSize(m_frame.read_bytes, 1));
  if(m_frame.exact && m_frame.step > 1)
    lines << QStringLiteral("extents of all %1 elements")
                 .arg(m_frame.available - m_frame.first);
  const int n = components();
  for(int c = 0; c < n; c++)
    if(m_frameLo[c] <= m_frameHi[c])
      lines << QStringLiteral("%1  %2 to %3")
                   .arg(n > 1 ? component_names[c] : QStringLiteral("value"))
                   .arg(formatValue(m_frameLo[c]))
                   .arg(formatValue(m_frameHi[c]));
  if(m_nonFinite > 0)
    lines << QStringLiteral("%1 not finite").arg(m_nonFinite);
  lines << extra;

  // In its corner, or the opposite one when the mouse is over it: clear of
  // what the mouse points at
  const QFontMetricsF fm{smallFont()};
  double w = 0;
  for(const auto& l : lines)
    w = std::max(w, fm.horizontalAdvance(l));
  const QSizeF size{w + 6, fm.height() * lines.size() + 4};
  const QPointF topLeft{plot.left() + 2, plot.top() + 2};
  const QPointF bottomRight{
      plot.right() - size.width() - 2, plot.bottom() - size.height() - 2};
  QPointF at = bottom ? bottomRight : topLeft;
  if(m_hover && QRectF(at, size).contains(*m_hover))
    at = bottom ? topLeft : QPointF{bottomRight.x(), topLeft.y()};
  drawReadout(*p, at, lines);
}

void Layer::paintCurves(QPainter* p) const
{
  using namespace Ui::Monitor;
  const QRectF area = plotRect();
  if(area.width() < 8 || area.height() < 8)
    return;
  const int n = components();
  const int64_t count = m_frame.count;
  // Apart, so that the labels of a row's bottom and of the next one's top
  // do not meet
  const double gap = n > 1 ? 8. : 0.;
  const double rh = (area.height() - gap * (n - 1)) / n;
  if(rh < 4)
    return;
  auto xOf = [&](double i) {
    return count > 1 ? area.left() + i / double(count - 1) * area.width() : area.center().x();
  };
  const float* values = m_frame.values.data();

  for(int c = 0; c < n; c++)
  {
    const QRectF r{area.left(), area.top() + c * (rh + gap), area.width(), rh};
    const auto [lo, hi] = rangeOf(c);
    auto yOf = [&](double v) {
      if(!std::isfinite(v))
        v = lo;
      return r.bottom() - std::clamp((v - lo) / (hi - lo), 0., 1.) * r.height();
    };
    const auto ticks = linearTicks(lo, hi, r.height(), 18.);
    p->setRenderHint(QPainter::Antialiasing, false);
    p->setPen(gridColor());
    for(const auto& t : ticks)
      p->drawLine(QPointF{r.left(), yOf(t.value)}, QPointF{r.right(), yOf(t.value)});
    p->setPen(majorGridColor());
    p->drawLine(r.bottomLeft(), r.bottomRight());
    drawYAxisLabels(*p, r, ticks, yOf);

    auto& line = m_polyline;
    line.resize(0);
    const int cols = std::max(1, int(r.width()));
    if(count <= 2 * cols)
    {
      for(int64_t i = 0; i < count; i++)
        line << QPointF{xOf(i), yOf(values[i * n + c])};
    }
    else
    {
      // More elements than pixels: the extent of those under each column,
      // drawn up and down as one line
      for(int x = 0; x < cols; x++)
      {
        const int64_t i0 = x * count / cols;
        const int64_t i1 = std::max(i0 + 1, (x + 1) * count / cols);
        float mn = std::numeric_limits<float>::max(), mx = std::numeric_limits<float>::lowest();
        for(int64_t i = i0; i < i1; i++)
        {
          const float v = values[i * n + c];
          if(!finite(v))
            continue;
          mn = std::min(mn, v);
          mx = std::max(mx, v);
        }
        if(mn > mx)
          mn = mx = lo;
        const double px = r.left() + x + 0.5;
        if(x % 2)
          line << QPointF{px, yOf(mx)} << QPointF{px, yOf(mn)};
        else
          line << QPointF{px, yOf(mn)} << QPointF{px, yOf(mx)};
      }
    }
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(QPen(channelColor(c), 1.2));
    if(line.size() == 1)
      p->drawEllipse(line.front(), 2., 2.);
    else
      p->drawPolyline(line);
    if(n > 1)
      drawLabel(
          *p, QRectF(r.left(), r.top() + 1, r.width() - 3, 10), Qt::AlignRight | Qt::AlignTop,
          component_names[c], channelColor(c));
  }

  // Element indices in the buffer, whole ones only
  const double first = double(m_frame.first), step = double(m_frame.step);
  std::vector<Tick> ticks;
  for(auto& t : linearTicks(first, first + double(count - 1) * step, area.width(), 50.))
    if(t.value == std::floor(t.value))
      ticks.push_back({t.value, QString::number(qint64(t.value)), true});
  drawXAxisLabels(*p, area, ticks, [&](double v) { return xOf((v - first) / step); });

  QStringList extra;
  if(m_hover && area.contains(*m_hover))
  {
    const int64_t i = count > 1 ? std::clamp<int64_t>(
                                      std::llround((m_hover->x() - area.left()) / area.width()
                                                   * double(count - 1)),
                                      0, count - 1)
                                : 0;
    p->setRenderHint(QPainter::Antialiasing, false);
    p->setPen(QPen(textColor(), 1, Qt::DotLine));
    p->drawLine(QPointF{xOf(i), area.top()}, QPointF{xOf(i), area.bottom()});
    QString s = QStringLiteral("element %1:").arg(indexOf(i));
    for(int c = 0; c < n; c++)
      s += QLatin1Char(' ') + formatValue(values[i * n + c]);
    extra << s;
  }
  paintInfo(p, area, extra);
}

void Layer::paintTable(QPainter* p) const
{
  using namespace Ui::Monitor;
  const QRectF area = boundingRect().adjusted(4, 20, -4, -2);
  constexpr double lh = 11.;
  const int rows = int(area.height() / lh) - 1;
  if(area.width() < 16 || rows < 1)
    return;
  const QFontMetricsF fm{smallFont()};
  const int n = components();
  const double iw = std::max(16., fm.horizontalAdvance(QString::number(m_frame.available)) + 8.);
  const double cw = (area.width() - iw) / n;

  auto cell = [&](int col) {
    return col < 0 ? QRectF(area.left(), 0, iw - 6, lh)
                   : QRectF(area.left() + iw + col * cw, 0, cw - 4, lh);
  };
  auto at = [](QRectF r, double y) {
    r.moveTop(y);
    return r;
  };

  // Header
  drawLabel(*p, at(cell(-1), area.top()), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("#"));
  for(int c = 0; c < n; c++)
    drawLabel(
        *p, at(cell(c), area.top()), Qt::AlignRight | Qt::AlignVCenter,
        n > 1 ? component_names[c] : QStringLiteral("value"), channelColor(c));
  p->setPen(majorGridColor());
  p->drawLine(
      QPointF{area.left(), area.top() + lh}, QPointF{area.right(), area.top() + lh});

  // The first elements that fit, and a line for those that do not
  int64_t shown = std::min<int64_t>(m_frame.count, rows);
  const auto more = [&] { return m_frame.available - 1 - indexOf(shown - 1); };
  if(more() > 0 && shown == rows)
    shown = rows - 1;
  const float* values = m_frame.values.data();
  for(int64_t i = 0; i < shown; i++)
  {
    const double y = area.top() + lh * (i + 1);
    if(i % 2)
      p->fillRect(QRectF(area.left(), y, area.width(), lh), QColor(255, 255, 255, 8));
    drawLabel(
        *p, at(cell(-1), y), Qt::AlignRight | Qt::AlignVCenter,
        QString::number(indexOf(i)), QColor(120, 120, 125));
    for(int c = 0; c < n; c++)
      drawLabel(
          *p, at(cell(c), y), Qt::AlignRight | Qt::AlignVCenter,
          formatValue(values[i * n + c]));
  }
  if(more() > 0)
    drawLabel(
        *p, QRectF(area.left(), area.top() + lh * (shown + 1), area.width(), lh),
        Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("%1 more").arg(more()));

  // The values start at the top left
  paintInfo(p, area, {}, true);
}

QRectF Layer::imageRect(QSize& size) const
{
  const int64_t count = std::max<int64_t>(1, m_frame.count);
  const int setting = int(Ui::Monitor::floatValue(m_width, 0.f));
  const int64_t w = setting > 0 ? setting : int64_t(std::ceil(std::sqrt(double(count))));
  const int64_t h = (count + w - 1) / w;
  size = QSize{int(std::min<int64_t>(w, 16384)), int(std::min<int64_t>(h, 1 << 22))};
  const QRectF area = boundingRect().adjusted(4, 20, -4, -4);
  if(area.width() <= 0 || area.height() <= 0)
    return {};
  const double scale = std::min(area.width() / size.width(), area.height() / size.height());
  const QSizeF shown{size.width() * scale, size.height() * scale};
  return QRectF{
      area.left() + (area.width() - shown.width()) / 2.,
      area.top() + (area.height() - shown.height()) / 2., shown.width(), shown.height()};
}

void Layer::buildImage() const
{
  QSize size;
  imageRect(size);
  if(!m_imageDirty && m_image.size() == size)
    return;
  m_imageDirty = false;
  if(m_image.size() != size)
    m_image = QImage(size, QImage::Format_ARGB32_Premultiplied);
  m_image.fill(Qt::transparent);

  const int n = components();
  std::array<float, 4> lo{}, span{};
  for(int c = 0; c < n; c++)
  {
    const auto [l, h] = rangeOf(c);
    lo[c] = l;
    span[c] = h - l;
  }
  const int w = size.width();
  const float* v = m_frame.values.data();
  const int64_t count = std::min<int64_t>(m_frame.count, int64_t(w) * size.height());
  for(int64_t i = 0; i < count; i++, v += n)
  {
    std::array<int, 4> ch{0, 0, 0, 255};
    bool bad = false;
    for(int c = 0; c < n; c++)
    {
      if(!finite(v[c]))
      {
        bad = true;
        break;
      }
      ch[c] = int(std::lround(std::clamp((v[c] - lo[c]) / span[c], 0.f, 1.f) * 255.f));
    }
    QRgb px;
    if(bad)
      px = qRgb(255, 0, 255);
    else if(n == 1)
      px = qRgb(ch[0], ch[0], ch[0]);
    else
      px = qPremultiply(qRgba(ch[0], ch[1], ch[2], ch[3]));
    reinterpret_cast<QRgb*>(m_image.scanLine(int(i / w)))[i % w] = px;
  }
}

void Layer::paintImage(QPainter* p) const
{
  using namespace Ui::Monitor;
  QSize size;
  const QRectF r = imageRect(size);
  if(r.isEmpty())
    return;
  buildImage();
  if(components() == 4)
  {
    // Under the alpha
    const QColor a(70, 70, 74), b(100, 100, 106);
    constexpr double sq = 8.;
    p->save();
    p->setClipRect(r, Qt::IntersectClip);
    for(double y = r.top(); y < r.bottom(); y += sq)
      for(double x = r.left(); x < r.right(); x += sq)
        p->fillRect(
            QRectF(x, y, sq, sq),
            (int((x - r.left()) / sq) + int((y - r.top()) / sq)) % 2 ? a : b);
    p->restore();
  }
  p->setRenderHint(QPainter::SmoothPixmapTransform, false);
  p->drawImage(r, m_image);

  QStringList extra;
  extra << QStringLiteral("%1 x %2 elements").arg(size.width()).arg(size.height());
  if(m_hover && r.contains(*m_hover))
  {
    const int x = std::clamp(
        int((m_hover->x() - r.left()) / r.width() * size.width()), 0, size.width() - 1);
    const int y = std::clamp(
        int((m_hover->y() - r.top()) / r.height() * size.height()), 0, size.height() - 1);
    const int64_t i = int64_t(y) * size.width() + x;
    if(i < m_frame.count)
    {
      const int n = components();
      QString s = QStringLiteral("element %1:").arg(indexOf(i));
      for(int c = 0; c < n; c++)
        s += QLatin1Char(' ') + formatValue(m_frame.values[i * n + c]);
      extra << s;
    }
  }
  paintInfo(p, boundingRect().adjusted(4, 20, -4, -4), extra);
}

void Layer::paintHistogram(QPainter* p) const
{
  using namespace Ui::Monitor;
  const QRectF area = boundingRect().adjusted(4, 20, -4, -2);
  const int n = components();
  const double rh = area.height() / n;
  if(area.width() < 8 || rh < 6)
    return;
  const bool labels = rh >= 30;
  const int nb = std::clamp(int(area.width() / 4), 8, int(m_bins[0].size()));
  const float* values = m_frame.values.data();

  QStringList extra;
  for(int c = 0; c < n; c++)
  {
    const QRectF row{area.left(), area.top() + c * rh, area.width(), rh};
    const QRectF r = row.adjusted(0, 1, 0, labels ? -12 : -1);
    const auto [lo, hi] = rangeOf(c);
    auto& bins = m_bins[c];
    std::fill(bins.begin(), bins.end(), 0);
    for(int64_t i = 0; i < m_frame.count; i++)
    {
      const float v = values[i * n + c];
      if(finite(v))
        bins[std::clamp(int((v - lo) / (hi - lo) * nb), 0, nb - 1)]++;
    }
    const int peak = std::max(1, *std::max_element(bins.begin(), bins.begin() + nb));
    const double bw = r.width() / nb;
    QColor col = channelColor(c);
    col.setAlpha(200);
    for(int b = 0; b < nb; b++)
    {
      const double h = double(bins[b]) / peak * r.height();
      p->fillRect(QRectF(r.left() + b * bw, r.bottom() - h, std::max(1., bw - 1), h), col);
    }
    p->setPen(majorGridColor());
    p->drawLine(r.bottomLeft(), r.bottomRight());
    if(labels)
    {
      drawLabel(
          *p, QRectF(r.left(), r.bottom() + 1, r.width() / 2, 11), Qt::AlignLeft | Qt::AlignTop,
          formatValue(lo));
      drawLabel(
          *p, QRectF(r.center().x(), r.bottom() + 1, r.width() / 2, 11),
          Qt::AlignRight | Qt::AlignTop, formatValue(hi));
    }
    if(n > 1)
      drawLabel(
          *p, QRectF(r.left(), r.top(), r.width() - 3, 10), Qt::AlignRight | Qt::AlignTop,
          component_names[c], channelColor(c));

    if(m_hover && r.contains(*m_hover))
    {
      const int b = std::clamp(int((m_hover->x() - r.left()) / bw), 0, nb - 1);
      const double step = double(hi - lo) / nb;
      extra << QStringLiteral("%1 to %2: %3")
                   .arg(lo + b * step, 0, 'g', 4)
                   .arg(lo + (b + 1) * step, 0, 'g', 4)
                   .arg(bins[b]);
    }
  }
  paintInfo(p, area, extra);
}
}
