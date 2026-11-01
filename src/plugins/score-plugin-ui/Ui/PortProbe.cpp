#include "PortProbe.hpp"

#include <State/ValuePrettyPrint.hpp>

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Execution/PortTap.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessContext.hpp>

#include <Scenario/Settings/ScenarioSettingsModel.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/TexturePort.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/application/GUIApplicationContext.hpp>

#include <core/application/ApplicationSettings.hpp>

#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QQuaternion>
#include <QtMath>

#include <Ui/Monitor/MidiText.hpp>
#include <Ui/Monitor/Paint.hpp>
#include <Ui/Monitor/Ring.hpp>

#include <cmath>
#include <utility>

namespace Ui
{
namespace
{
constexpr double width = 180., height = 110., header = 14.;
constexpr std::size_t max_lines = 6;
constexpr std::size_t trail_length = 120;
constexpr std::size_t audio_window = 2048, trace_length = 512;

bool executionUpdates()
{
  return score::AppContext().settings<Scenario::Settings::Model>().getExecutionUpdate();
}

//! The texture outlet a texture port shows: itself, or the source of its
//! first cable.
const Gfx::TextureOutlet* textureSource(const Process::Port& port, const score::DocumentContext& doc)
{
  if(auto* out = qobject_cast<const Gfx::TextureOutlet*>(&port))
    return out;
  for(const auto& path : port.cables())
    if(auto* cable = path.try_find(doc))
      if(auto* src = qobject_cast<Gfx::TextureOutlet*>(cable->source().try_find(doc)))
        return src;
  return nullptr;
}
}

ProbeItem::ProbeItem(const Process::Port& port, const Process::Context& ctx)
    : m_port{&port}
    , m_context{ctx}
{
  setAcceptHoverEvents(true);
  setAcceptedMouseButtons(Qt::NoButton);
  const auto* proc = Process::parentProcess(&port);
  const QString& name = port.visualName().isEmpty() ? port.name() : port.visualName();
  m_title = (proc ? proc->metadata().getName() + QStringLiteral(" : ") : QString{}) + name;
  attach();
  startTimer(std::max(1, score::AppContext().applicationSettings.uiEventRate));
}

ProbeItem::~ProbeItem()
{
  m_texture.detach();
}

QRectF ProbeItem::boundingRect() const
{
  return {0., 0., width, height};
}

void ProbeItem::attach()
{
  if(!m_port)
    return;
  m_sinceAttach = 0;
  const auto type = m_port->type();
  if(type == Process::PortType::Texture)
  {
    auto* plug = m_context.findPlugin<Gfx::DocumentPlugin>();
    const auto* src = textureSource(*m_port, m_context);
    if(plug && src && src->graphicsPort().node != score::gfx::invalid_node_index)
    {
      if(!m_texture.attached())
        m_texture.attach(&plug->context, src->graphicsPort(), QSize{int(width) - 8, int(height - header) - 6});
      else
        m_texture.setProducer(src->graphicsPort());
    }
    return;
  }

  // Taps follow execution only when the user lets views do so
  if(!executionUpdates())
    return;
  auto* exec = m_context.findPlugin<Execution::DocumentPlugin>();
  if(!exec || !exec->isPlaying())
    return;
  m_tap = std::make_unique<Execution::PortTap>(*m_port, exec->context());
  if(!m_tap->monitor())
    m_tap.reset();
}

void ProbeItem::timerEvent(QTimerEvent*)
{
  const bool had_tap = bool(m_tap);
  const bool updates = executionUpdates();
  const bool setting_changed = std::exchange(m_executionUpdates, updates) != updates;

  // Execution updates turned off while the probe is open: stop tapping
  if(m_tap && !updates)
    m_tap.reset();

  // Not running yet, or anymore: look again twice a second
  const bool live = (m_tap && m_tap->monitor()) || m_texture.attached();
  if(!live && ++m_sinceAttach * score::AppContext().applicationSettings.uiEventRate >= 500)
    attach();
  auto* exec = m_context.findPlugin<Execution::DocumentPlugin>();
  if(m_tap && (!exec || !exec->isPlaying()))
    m_tap.reset();

  // The texture preview renders on its own: it is redrawn at the event rate
  const bool changed
      = drain() || m_texture.attached() || had_tap != bool(m_tap) || setting_changed;
  if(changed)
    update();
}

bool ProbeItem::drain()
{
  if(!m_tap || !m_tap->monitor())
    return false;
  auto& mon = *m_tap->monitor();
  mon.viewers = 1;
  bool changed = false;
  switch(mon.type)
  {
    case Process::PortType::Audio: {
      mon.samples.provision();
      const int nch = std::min(mon.samples.channels(), 16);
      if(int(m_peak.size()) != nch)
        changed = true;
      m_peak.assign(nch, 0.f);
      m_rms.assign(nch, 0.f);
      m_trace.resize(std::min(nch, 2));
      m_buffer.resize(audio_window);
      for(int c = 0; c < nch; c++)
      {
        uint64_t first{};
        if(!mon.samples.read_latest(c, m_buffer, first))
          continue;
        // Nothing written since the last tick: what is shown is current
        if(c == 0)
        {
          changed |= first != m_audioFirst;
          m_audioFirst = first;
        }
        double sum = 0;
        for(float v : m_buffer)
        {
          m_peak[c] = std::max(m_peak[c], std::abs(v));
          sum += v * v;
        }
        m_rms[c] = float(std::sqrt(sum / m_buffer.size()));
        if(c < 2)
          m_trace[c].assign(m_buffer.end() - trace_length, m_buffer.end());
      }
      break;
    }
    case Process::PortType::Midi:
      mon.midi.drain([&](const Execution::TapMonitor::Midi& m) {
        // Clock and active sensing would hide everything else
        if(m.bytes[0] == 0xF8 || m.bytes[0] == 0xFE)
          return;
        Ui::Monitor::pushRing(
            m_lines,
            Ui::Monitor::describeMidi(
                m.bytes[0], m.size > 1 ? m.bytes[1] : 0, m.size > 2 ? m.bytes[2] : 0,
                m.size, false),
            max_lines);
        changed = true;
      });
      break;
    default: {
      ossia::value v;
      std::string text;
      m_unit = mon.unit;
      const bool position = bool(m_unit.v.target<ossia::position_u>());
      while(mon.values.try_dequeue(v))
      {
        changed = true;
        if(m_unit)
        {
          m_last = v;
          if(position)
          {
            const auto xyz = ossia::convert<ossia::vec3f>(
                ossia::convert(v, m_unit, ossia::cartesian_3d_u{}));
            Ui::Monitor::pushRing(m_path, QPointF(xyz[0], xyz[1]), trail_length);
          }
        }
        text.clear();
        State::prettyPrintValue(text, v);
        Ui::Monitor::pushRing(m_lines, QString::fromStdString(text), max_lines);
        switch(v.get_type())
        {
          case ossia::val_type::FLOAT:
          case ossia::val_type::INT:
          case ossia::val_type::BOOL:
            Ui::Monitor::pushRing(m_trail, ossia::convert<float>(v), trail_length);
            break;
          default:
            m_trail.clear();
            break;
        }
      }
      break;
    }
  }
  return changed;
}

void ProbeItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
  using namespace Ui::Monitor;
  const QRectF r = boundingRect();
  p->setRenderHint(QPainter::Antialiasing, true);
  p->setPen(QPen(QColor(90, 90, 100), 1));
  p->setBrush(QColor(22, 22, 26, 240));
  p->drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);

  // Header: what is shown
  drawLabel(*p, QRectF(6, 0, width - 12, header), Qt::AlignLeft | Qt::AlignVCenter, m_title, QColor(220, 220, 225));

  const QRectF body = r.adjusted(4, header + 2, -4, -4);
  if(!m_port)
    return;

  if(m_port->type() == Process::PortType::Texture)
  {
    const QImage img = m_texture.image();
    if(img.isNull())
      drawLabel(*p, body, Qt::AlignCenter, QStringLiteral("not rendering"));
    else
      p->drawImage(Gfx::TexturePreview::fit(body, img.size()), img);
    return;
  }
  if(m_port->type() == Process::PortType::Geometry)
  {
    drawLabel(*p, body, Qt::AlignCenter, QStringLiteral("geometry\nsee the Geometry viewer"));
    return;
  }
  if(!m_tap)
  {
    drawLabel(
        *p, body, Qt::AlignCenter,
        executionUpdates()
            ? QStringLiteral("not running")
            : QStringLiteral("execution updates are off"));
    return;
  }

  if(m_port->type() == Process::PortType::Audio)
  {
    // Bars on the left, trace on the right
    const int nch = int(m_peak.size());
    const double bars_w = std::min(body.width() * 0.4, nch * 10.);
    const double bw = nch > 0 ? bars_w / nch : 0.;
    auto y_of = [&](float lin) {
      const double db = std::clamp(20. * std::log10(std::max(lin, 1e-6f)), -60., 0.);
      return body.bottom() - (db + 60.) / 60. * body.height();
    };
    p->setRenderHint(QPainter::Antialiasing, false);
    for(int c = 0; c < nch; c++)
    {
      const QRectF strip{body.left() + c * bw, body.top(), std::max(1., bw - 1), body.height()};
      p->fillRect(strip, background());
      p->fillRect(QRectF(strip.left(), y_of(m_rms[c]), strip.width(), strip.bottom() - y_of(m_rms[c])), QColor(76, 175, 80, 150));
      p->fillRect(QRectF(strip.left(), y_of(m_peak[c]) - 1, strip.width(), 2), m_peak[c] >= 1.f ? overColor() : QColor(240, 240, 240));
    }
    const QRectF tr{body.left() + bars_w + 6, body.top(), body.width() - bars_w - 6, body.height()};
    p->fillRect(tr, background());
    p->setRenderHint(QPainter::Antialiasing, true);
    for(std::size_t c = 0; c < m_trace.size(); c++)
    {
      const auto& t = m_trace[c];
      if(t.size() < 2)
        continue;
      QPolygonF line;
      for(std::size_t i = 0; i < t.size(); i++)
        line << QPointF{tr.left() + i * tr.width() / (t.size() - 1), tr.center().y() - std::clamp(t[i], -1.f, 1.f) * tr.height() / 2};
      p->setPen(QPen(channelColor(int(c)), 1));
      p->drawPolyline(line);
    }
    return;
  }

  // MIDI and values: the latest lines, a trail of numbers under them, or
  // what their unit means on the left
  QRectF text = body;
  if(m_unit && m_last.valid())
  {
    const double side = std::min(body.height(), body.width() * 0.45);
    const QRectF area{body.left(), body.top(), side, body.height()};
    if(paintUnit(*p, area))
      text.setLeft(area.right() + 6);
  }
  else if(m_trail.size() > 1)
  {
    const QRectF tr{body.left(), body.bottom() - 28, body.width(), 28};
    text.setBottom(tr.top() - 2);
    const auto [lo, hi] = std::minmax_element(m_trail.begin(), m_trail.end());
    const float a = *lo, b = *hi > *lo ? *hi : *lo + 1.f;
    QPolygonF line;
    for(std::size_t i = 0; i < m_trail.size(); i++)
      line << QPointF{tr.left() + i * tr.width() / (trail_length - 1), tr.bottom() - (m_trail[i] - a) / (b - a) * tr.height()};
    p->fillRect(tr, background());
    p->setPen(QPen(traceColor(), 1.2));
    p->drawPolyline(line);
  }
  QFont f = score::Skin::instance().MonoFontSmall;
  f.setPixelSize(10);
  p->setFont(f);
  p->setPen(QColor(215, 215, 220));
  QString all;
  for(auto it = m_lines.rbegin(); it != m_lines.rend(); ++it)
    all += *it + QLatin1Char('\n');
  p->drawText(text, Qt::AlignLeft | Qt::AlignTop | Qt::TextWrapAnywhere, all.isEmpty() ? QStringLiteral("nothing yet") : all);
}

bool ProbeItem::paintUnit(QPainter& p, const QRectF& area) const
{
  using namespace Ui::Monitor;
  const auto& ds = m_unit.v;
  const QRectF sq{area.left(), area.top(), area.width(), std::min(area.width(), area.height() - 12)};
  drawLabel(
      p, QRectF(area.left(), sq.bottom(), area.width(), 12), Qt::AlignCenter,
      QString::fromUtf8(ossia::get_pretty_unit_text(m_unit).data()));
  p.setRenderHint(QPainter::Antialiasing, true);

  if(ds.target<ossia::color_u>())
  {
    const auto c = ossia::convert<ossia::vec4f>(ossia::convert(m_last, m_unit, ossia::rgba_u{}));
    // Transparency over a checkerboard
    const double cell = sq.width() / 4.;
    for(int i = 0; i < 4; i++)
      for(int j = 0; j < 4; j++)
        p.fillRect(
            QRectF(sq.left() + i * cell, sq.top() + j * cell, cell, cell),
            (i + j) % 2 ? QColor(150, 150, 150) : QColor(100, 100, 100));
    p.fillRect(sq, QColor::fromRgbF(
                       std::clamp(c[0], 0.f, 1.f), std::clamp(c[1], 0.f, 1.f),
                       std::clamp(c[2], 0.f, 1.f), std::clamp(c[3], 0.f, 1.f)));
    return true;
  }
  if(ds.target<ossia::position_u>())
  {
    // The XY plane, scaled to fit what went by, with the path and the latest
    p.fillRect(sq, background());
    double ext = 1.;
    for(auto pt : m_path)
      ext = std::max({ext, std::abs(pt.x()), std::abs(pt.y())});
    auto at = [&](QPointF pt) {
      return QPointF{
          sq.center().x() + pt.x() / ext * sq.width() * 0.45,
          sq.center().y() - pt.y() / ext * sq.height() * 0.45};
    };
    p.setPen(QPen(gridColor(), 1));
    p.drawLine(QPointF{sq.left(), sq.center().y()}, QPointF{sq.right(), sq.center().y()});
    p.drawLine(QPointF{sq.center().x(), sq.top()}, QPointF{sq.center().x(), sq.bottom()});
    QPolygonF line;
    for(auto pt : m_path)
      line << at(pt);
    p.setPen(QPen(QColor(94, 170, 242, 140), 1));
    p.drawPolyline(line);
    if(!m_path.empty())
    {
      p.setPen(Qt::NoPen);
      p.setBrush(traceColor());
      p.drawEllipse(at(m_path.back()), 3, 3);
    }
    return true;
  }
  if(ds.target<ossia::orientation_u>())
  {
    // The axes of the rotation, seen from the front, Z towards the viewer
    const auto q = ossia::convert<ossia::vec4f>(ossia::convert(m_last, m_unit, ossia::quaternion_u{}));
    // ossia's quaternions are x, y, z, w
    const QQuaternion rot{q[3], q[0], q[1], q[2]};
    p.fillRect(sq, background());
    const QPointF c = sq.center();
    const double len = sq.width() * 0.4;
    struct Axis
    {
      QVector3D dir;
      QColor color;
    };
    Axis axes[]{
        {rot.rotatedVector({1, 0, 0}), QColor(235, 70, 60)},
        {rot.rotatedVector({0, 1, 0}), QColor(90, 200, 90)},
        {rot.rotatedVector({0, 0, 1}), QColor(80, 140, 240)}};
    // Farthest first
    std::sort(std::begin(axes), std::end(axes), [](const Axis& a, const Axis& b) {
      return a.dir.z() < b.dir.z();
    });
    for(const auto& a : axes)
    {
      p.setPen(QPen(a.color, a.dir.z() >= 0 ? 2. : 1.));
      p.drawLine(c, c + QPointF{a.dir.x() * len, -a.dir.y() * len});
    }
    return true;
  }
  if(ds.target<ossia::gain_u>())
  {
    const float db = ossia::convert<float>(ossia::convert(m_last, m_unit, ossia::decibel_u{}));
    const QRectF bar{sq.center().x() - 6, sq.top(), 12, sq.height()};
    p.fillRect(bar, background());
    const double t = std::clamp((db + 60.) / 72., 0., 1.);
    p.fillRect(
        QRectF(bar.left(), bar.bottom() - t * bar.height(), bar.width(), t * bar.height()),
        db > 0 ? overColor() : QColor(76, 175, 80));
    // 0 dB
    const double y0 = bar.bottom() - 60. / 72. * bar.height();
    p.setPen(QPen(textColor(), 1));
    p.drawLine(QPointF{bar.left() - 3, y0}, QPointF{bar.right() + 3, y0});
    return true;
  }
  if(ds.target<ossia::angle_u>())
  {
    const float deg = ossia::convert<float>(ossia::convert(m_last, m_unit, ossia::degree_u{}));
    const QRectF dial = sq.adjusted(4, 4, -4, -4);
    p.setPen(QPen(gridColor(), 1));
    p.setBrush(background());
    p.drawEllipse(dial);
    const double a = qDegreesToRadians(double(deg));
    p.setPen(QPen(traceColor(), 2));
    p.drawLine(
        dial.center(), dial.center()
                           + QPointF{std::cos(a) * dial.width() / 2, -std::sin(a) * dial.height() / 2});
    return true;
  }
  return false;
}

void ProbeItem::hoverEnterEvent(QGraphicsSceneHoverEvent* ev)
{
  Process::PortProbeHover::instance().hold();
  ev->accept();
}

void ProbeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* ev)
{
  Process::PortProbeHover::instance().leave();
  ev->accept();
}

Process::PortProbeItem*
ProbeFactory::make(const Process::Port& port, const Process::Context& ctx)
{
  return new ProbeItem{port, ctx};
}
}
