#include "WaveformInlet.hpp"

#include <Process/Commands/SetControlValue.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/Skin.hpp>

#include <QDebug>
#include <QFocusEvent>
#include <QFontMetricsF>
#include <QGraphicsObject>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QRandomGenerator>
#include <QResizeEvent>

#include <algorithm>
#include <cmath>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Dataflow::WaveformInlet)

namespace Dataflow
{
WaveformInlet::WaveformInlet(Id<Process::Port> id, QObject* parent)
    : WaveformInlet{id, parent, false}
{
}

WaveformInlet::WaveformInlet(Id<Process::Port> id, QObject* parent, bool bank)
    : ControlInlet{bank ? QStringLiteral("Wavetable") : QStringLiteral("Waveform"), id, parent}
    , m_bank{bank}
{
  const auto initial = Process::wavetableToValue(m_waveform);
  ControlInlet::setValue(initial);
  setInit(initial);
}

WaveformInlet::~WaveformInlet() = default;

WaveformInlet::WaveformInlet(DataStream::Deserializer& vis, QObject* parent, bool bank)
    : ControlInlet{vis, parent}, m_bank{bank}
{
  vis.writeTo(*this);
  initializeLoadedValue();
}

WaveformInlet::WaveformInlet(JSONObject::Deserializer& vis, QObject* parent, bool bank)
    : ControlInlet{vis, parent}, m_bank{bank}
{
  vis.writeTo(*this);
  initializeLoadedValue();
}

WaveformInlet::WaveformInlet(DataStream::Deserializer& vis, QObject* parent)
    : WaveformInlet{vis, parent, false}
{
}
WaveformInlet::WaveformInlet(JSONObject::Deserializer& vis, QObject* parent)
    : WaveformInlet{vis, parent, false}
{
}
WaveformInlet::WaveformInlet(DataStream::Deserializer&& vis, QObject* parent)
    : WaveformInlet{vis, parent, false}
{
}
WaveformInlet::WaveformInlet(JSONObject::Deserializer&& vis, QObject* parent)
    : WaveformInlet{vis, parent, false}
{
}

void WaveformInlet::initializeLoadedValue()
{
  // The document has to open whatever was saved: what this port cannot hold
  // is replaced, with a warning, instead of failing the whole load.
  if(m_amplitudeLevels < 0 || m_amplitudeLevels == 1)
  {
    qWarning() << "Waveform port: ignoring invalid amplitude levels";
    m_amplitudeLevels = 0;
  }
  if(m_frameSizeConstraint == 1 || m_frameSizeConstraint > Process::WavetableData::maxFrameSize)
  {
    qWarning() << "Waveform port: ignoring invalid frame size";
    m_frameSizeConstraint = 0;
  }

  auto loaded = Process::wavetableFromValue(value());
  auto initial = Process::wavetableFromValue(init());
  const bool loadedValid = loaded && accepts(*loaded);
  const bool initialValid = initial && accepts(*initial);
  if(loadedValid)
    m_waveform = std::move(*loaded);
  else if(initialValid)
    m_waveform = *initial;
  else
    m_waveform = Process::makeTriangleWavetable(
        m_frameSizeConstraint ? m_frameSizeConstraint : m_waveform.frameSize);
  if(!loadedValid)
    qWarning() << "Waveform port: replacing invalid content";

  ControlInlet::setValue(Process::wavetableToValue(m_waveform));
  if(!initialValid)
    setInit(value());
}

bool WaveformInlet::accepts(const Process::WavetableData& data) const noexcept
{
  return data.valid() && (m_bank || data.frameCount() == 1)
         && (!m_frameSizeConstraint || data.frameSize == m_frameSizeConstraint);
}

void WaveformInlet::setValue(const ossia::value& value)
{
  auto data = Process::wavetableFromValue(value);
  if(!data || !accepts(*data))
    return;
  if(*data == m_waveform)
    return;
  auto canonical = Process::wavetableToValue(*data);
  m_waveform = std::move(*data);
  ControlInlet::setValue(canonical);
}

void WaveformInlet::setWaveform(const Process::WavetableData& waveform)
{
  if(!accepts(waveform) || waveform == m_waveform)
    return;
  auto canonical = Process::wavetableToValue(waveform);
  m_waveform = waveform;
  ControlInlet::setValue(canonical);
}

void WaveformInlet::setAmplitudeLevels(int levels)
{
  if(levels != 0 && levels < 2)
    return;
  if(m_amplitudeLevels == levels)
    return;
  m_amplitudeLevels = levels;
  constraintsChanged();
}

void WaveformInlet::setFrameSizeConstraint(std::size_t size)
{
  if(size && (size < 2 || size > Process::WavetableData::maxFrameSize
              || size > Process::WavetableData::maxSamples / m_waveform.frameCount()))
    return;
  if(size == m_frameSizeConstraint)
    return;
  m_frameSizeConstraint = size;
  if(size && size != m_waveform.frameSize)
  {
    setWaveform(Process::resampleWavetable(m_waveform, size));
    if(auto initial = Process::wavetableFromValue(init()); initial && initial->frameCount() == m_waveform.frameCount())
      setInit(Process::wavetableToValue(Process::resampleWavetable(*initial, size)));
    else
      setInit(value());
  }
  constraintsChanged();
}

WavetableInlet::WavetableInlet(Id<Process::Port> id, QObject* parent)
    : WaveformInlet{id, parent, true}
{
}
WavetableInlet::WavetableInlet(DataStream::Deserializer& vis, QObject* parent)
    : WaveformInlet{vis, parent, true}
{
}
WavetableInlet::WavetableInlet(JSONObject::Deserializer& vis, QObject* parent)
    : WaveformInlet{vis, parent, true}
{
}
WavetableInlet::WavetableInlet(DataStream::Deserializer&& vis, QObject* parent)
    : WaveformInlet{vis, parent, true}
{
}
WavetableInlet::WavetableInlet(JSONObject::Deserializer&& vis, QObject* parent)
    : WaveformInlet{vis, parent, true}
{
}

namespace
{
//! Frames up to this size are drawn as one bar per sample; larger ones as a
//! line through the column centres.
constexpr std::size_t maxBarFrameSize = 128;
constexpr qreal stripHeight = 16.;
constexpr qreal minThumbnailWidth = 24.;
constexpr qreal arrowWidth = 16.;

QString levelDescription(int levels)
{
  if(levels < 2)
    return {};
  if((levels & (levels - 1)) == 0)
  {
    int bits{};
    while((1 << bits) < levels)
      ++bits;
    return QObject::tr("%1 bit").arg(bits);
  }
  return QObject::tr("%1 levels").arg(levels);
}

//! Both scene and inspector own this same editor. Its selection never enters
//! the document selection stack, and every edit targets the real port path.
//!
//! Samples occupy equal columns of the plot; drawing and hit testing share
//! that geometry. Banks add a strip above the plot to choose the frame being
//! edited. Shapes, transforms, resampling and frame operations are in the
//! context menu, each one undoable command.
class WaveformItem final : public QGraphicsObject
{
public:
  WaveformItem(WaveformInlet& model, const score::DocumentContext& ctx, QGraphicsItem* parent)
      : QGraphicsObject{parent}, m_dispatcher{ctx.commandStack}
  {
    setObjectName(QStringLiteral("WaveformEditor"));
    setData(0, QStringLiteral("WaveformEditor"));
    setFlags(ItemIsFocusable);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
    setModel(model);
  }

  ~WaveformItem() override { finish(false); }

  void setModel(WaveformInlet& model)
  {
    if(m_model == &model)
      return;
    finish(false);
    if(m_model)
      disconnect(m_model, nullptr, this, nullptr);
    m_model = &model;
    m_frame = 0;
    m_execution.reset();
    connect(&model, &Process::ControlInlet::executionValueChanged, this,
            [this](const ossia::value& value) {
      if(auto received = Process::waveformFromValue(value))
      {
        m_execution = std::move(*received);
        update(plotRect());
      }
    });
    connect(&model, &Process::Port::executionReset, this, [this] {
      m_execution.reset();
      update(plotRect());
    });
    connect(&model, &Process::ControlInlet::valueChanged, this, [this] {
      if(!m_submitting)
        finish(false);
      m_frame = std::min(m_frame, m_model->wavetable().frameCount() - 1);
      clampHover();
      update();
    });
    connect(&model, &WaveformInlet::constraintsChanged, this, [this] {
      finish(false);
      clampHover();
      update();
    });
    connect(&model, &QObject::destroyed, this, [this] {
      m_dragging = false;
      m_preview.clear();
      m_execution.reset();
      m_hover.reset();
      m_model = nullptr;
      update();
    });
    clampHover();
    update();
  }

  void setRect(const QRectF& rect)
  {
    finish(true);
    prepareGeometryChange();
    m_rect = rect.normalized();
    m_hover.reset();
    update();
  }

  QRectF boundingRect() const override { return m_rect; }

  QRectF stripRect() const
  {
    if(!m_model || !m_model->supportsFrames())
      return {};
    return {m_rect.left() + 2., m_rect.top() + 2., std::max(0., m_rect.width() - 4.), stripHeight};
  }

  QRectF plotRect() const
  {
    const auto strip = stripRect();
    const qreal top = strip.isNull() ? m_rect.top() + 2. : strip.bottom() + 3.;
    return QRectF{QPointF{m_rect.left() + 2., top}, QPointF{m_rect.right() - 2., m_rect.bottom() - 2.}}
        .normalized();
  }

  std::size_t frame() const noexcept { return m_frame; }

  QString label() const
  {
    if(!m_model)
      return {};
    QString text = tr("%1 smp").arg(m_model->wavetable().frameSize);
    if(const auto levels = levelDescription(m_model->amplitudeLevels()); !levels.isEmpty())
      text += QStringLiteral(", ") + levels;
    return text;
  }

  QString readout() const
  {
    if(!m_model || !m_hover)
      return {};
    const auto values = shownFrame();
    if(*m_hover >= values.size())
      return {};
    const float value = quantize(values[*m_hover]);
    const int levels = m_model->amplitudeLevels();
    const QString amplitude = levels >= 2
        ? QStringLiteral("%1/%2").arg(Process::sampleLevel(value, levels)).arg(levels - 1)
        : QString::number(value, 'f', 2);
    return QStringLiteral("#%1 %2").arg(*m_hover).arg(amplitude);
  }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
  {
    const auto& skin = score::Skin::instance();
    painter->fillRect(m_rect, skin.Dark.main.brush);
    if(!m_model)
      return;
    // Main views use DontSavePainterState; keep our drawing state local.
    painter->save();
    if(m_model->supportsFrames())
      paintStrip(*painter, skin);
    const auto area = plotRect();
    if(area.width() > 2. && area.height() > 2.)
    {
      painter->setClipRect(area, Qt::IntersectClip);
      paintPlot(*painter, skin, area);
    }
    painter->restore();
  }

  void fillContextMenu(QMenu& menu)
  {
    if(!m_model)
      return;
    finish(true);
    using Shape = Process::WaveformShape;
    auto* shapes = menu.addMenu(tr("Shape"));
    for(auto [name, shape] : std::initializer_list<std::pair<QString, Shape>>{
            {tr("Sine"), Shape::Sine},
            {tr("Triangle"), Shape::Triangle},
            {tr("Saw up"), Shape::SawUp},
            {tr("Saw down"), Shape::SawDown},
            {tr("Square"), Shape::Square},
            {tr("Pulse 25%"), Shape::Pulse25},
            {tr("Pulse 12.5%"), Shape::Pulse12},
            {tr("Noise"), Shape::Noise}})
      connect(shapes->addAction(name), &QAction::triggered, this, [this, shape] {
        const auto seed = QRandomGenerator::global()->generate();
        editFrame([shape, seed](std::span<float> frame, int levels) {
          Process::generateWaveform(frame, shape, levels, seed);
        });
      });

    menu.addSeparator();
    const auto transform = [&](const QString& name, auto function) {
      connect(menu.addAction(name), &QAction::triggered, this,
              [this, function] { editFrame(function); });
    };
    transform(tr("Normalize"), Process::normalizeWaveform);
    transform(tr("Invert"), Process::invertWaveform);
    transform(tr("Reverse"), [](std::span<float> frame, int) { Process::reverseWaveform(frame); });
    transform(tr("Rotate left"), [](std::span<float> frame, int) { Process::rotateWaveform(frame, true); });
    transform(tr("Rotate right"), [](std::span<float> frame, int) { Process::rotateWaveform(frame, false); });
    transform(tr("Smooth"), Process::smoothWaveform);
    transform(tr("Clear"), Process::clearWaveform);

    const auto& table = m_model->wavetable();
    if(!m_model->frameSizeConstraint())
    {
      auto* sizes = menu.addMenu(tr("Resample"));
      const auto largest = std::min(Process::WavetableData::maxFrameSize,
          Process::WavetableData::maxSamples / table.frameCount());
      for(std::size_t size = 16; size <= std::min<std::size_t>(largest, 4096); size *= 2)
      {
        auto* action = sizes->addAction(tr("%1 samples").arg(size));
        action->setCheckable(true);
        action->setChecked(size == table.frameSize);
        connect(action, &QAction::triggered, this, [this, size] { resizeFrames(size); });
      }
    }

    if(m_model->supportsFrames())
    {
      menu.addSeparator();
      const bool canAdd = table.frameCount() < Process::WavetableData::maxFrames
          && table.samples.size() <= Process::WavetableData::maxSamples - table.frameSize;
      const auto frameAction = [&](const QString& name, BankAction action, bool enabled) {
        auto* item = menu.addAction(name);
        item->setEnabled(enabled);
        connect(item, &QAction::triggered, this, [this, action] { editBank(action); });
      };
      frameAction(tr("Add frame"), BankAction::Add, canAdd);
      frameAction(tr("Duplicate frame"), BankAction::Duplicate, canAdd);
      frameAction(tr("Remove frame"), BankAction::Remove, table.frameCount() > 1);
      frameAction(tr("Move frame left"), BankAction::Earlier, m_frame > 0);
      frameAction(tr("Move frame right"), BankAction::Later, m_frame + 1 < table.frameCount());
    }
  }

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override
  {
    event->accept();
    if(!m_model || event->button() != Qt::LeftButton)
      return;
    setFocus(Qt::MouseFocusReason);
    finish(true);
    if(stripRect().contains(event->pos()))
    {
      clickStrip(event->pos());
      return;
    }
    if(!plotRect().contains(event->pos()))
      return;
    const auto frame = m_model->wavetable().frame(m_frame);
    m_preview.assign(frame.begin(), frame.end());
    m_dragging = true;
    m_previous = sampleAt(event->pos());
    m_hover = m_previous.first;
    paintSamples(m_previous);
  }

  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override
  {
    event->accept();
    if(m_dragging && m_model)
    {
      const auto sample = sampleAt(event->pos());
      m_hover = sample.first;
      paintSamples(sample);
    }
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
  {
    event->accept();
    if(m_dragging && m_model)
      paintSamples(sampleAt(event->pos()));
    finish(true);
    hover(event->pos());
  }

  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override
  {
    mousePressEvent(event);
  }

  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override { hover(event->pos()); }
  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override { hover(event->pos()); }
  void hoverLeaveEvent(QGraphicsSceneHoverEvent*) override
  {
    if(m_hover)
    {
      m_hover.reset();
      update();
    }
  }

  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override
  {
    event->accept();
    if(!m_model)
      return;
    QMenu menu;
    fillContextMenu(menu);
    menu.exec(event->screenPos());
  }

  void keyPressEvent(QKeyEvent* event) override
  {
    if(event->key() == Qt::Key_Escape)
    {
      finish(false);
      event->accept();
    }
    else
      QGraphicsObject::keyPressEvent(event);
  }

  void focusOutEvent(QFocusEvent* event) override
  {
    finish(true);
    QGraphicsObject::focusOutEvent(event);
  }

  bool sceneEvent(QEvent* event) override
  {
    if(event->type() == QEvent::ShortcutOverride
       && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape)
    {
      event->accept();
      return true;
    }
    if(event->type() == QEvent::UngrabMouse)
      finish(true);
    return QGraphicsObject::sceneEvent(event);
  }

private:
  enum class BankAction { Add, Duplicate, Remove, Earlier, Later };

  std::span<const float> shownFrame() const
  {
    return m_dragging ? std::span<const float>{m_preview} : m_model->wavetable().frame(m_frame);
  }

  qreal columnLeft(const QRectF& area, std::size_t column, std::size_t count) const
  {
    return area.left() + area.width() * double(column) / count;
  }

  qreal amplitudeY(const QRectF& area, double value) const
  {
    return area.top() + area.height() * (1. - value) * .5;
  }

  //! Thumbnails when every frame gets a legible one, otherwise arrows around
  //! the frame number.
  bool thumbnails() const
  {
    const auto strip = stripRect();
    return strip.width() >= m_model->wavetable().frameCount() * minThumbnailWidth;
  }

  QRectF thumbnailRect(std::size_t frame) const
  {
    const auto strip = stripRect();
    const auto count = m_model->wavetable().frameCount();
    const qreal width = std::min(48., strip.width() / count);
    return {strip.left() + frame * width, strip.top(), width - 2., strip.height()};
  }

  void paintStrip(QPainter& painter, const score::Skin& skin)
  {
    const auto strip = stripRect();
    const auto& table = m_model->wavetable();
    const auto count = table.frameCount();
    painter.setFont(skin.MonoFontSmall);
    if(thumbnails())
    {
      for(std::size_t i = 0; i < count; ++i)
      {
        const auto box = thumbnailRect(i);
        const bool selected = i == m_frame;
        painter.setPen(selected ? skin.Base4.main.pen1 : skin.DarkGray.main.pen1);
        painter.setBrush(selected ? skin.Emphasis2.main.brush : skin.NoBrush);
        painter.drawRect(box.adjusted(.5, .5, -.5, -.5));
        const auto inner = box.adjusted(2., 2., -2., -2.);
        const auto samples = table.frame(i);
        const std::size_t points = std::min(samples.size(), std::size_t(std::max(2., inner.width())));
        QPolygonF line;
        line.reserve(points);
        for(std::size_t p = 0; p < points; ++p)
        {
          const auto index = p * samples.size() / points;
          line.push_back({inner.left() + inner.width() * (p + .5) / points,
                          amplitudeY(inner, quantize(samples[index]))});
        }
        painter.setPen(selected ? skin.Base4.main.pen1 : skin.Gray.main.pen1);
        painter.drawPolyline(line);
      }
      painter.setBrush(skin.NoBrush);
      return;
    }
    const QRectF previous{strip.left(), strip.top(), arrowWidth, strip.height()};
    const QRectF next{strip.right() - arrowWidth, strip.top(), arrowWidth, strip.height()};
    painter.setPen(m_frame > 0 ? skin.HalfLight.main.pen1 : skin.DarkGray.main.pen1);
    painter.drawText(previous, Qt::AlignCenter, QStringLiteral("<"));
    painter.setPen(m_frame + 1 < count ? skin.HalfLight.main.pen1 : skin.DarkGray.main.pen1);
    painter.drawText(next, Qt::AlignCenter, QStringLiteral(">"));
    painter.setPen(skin.HalfLight.main.pen1);
    painter.drawText(strip, Qt::AlignCenter, QStringLiteral("%1/%2").arg(m_frame + 1).arg(count));
  }

  void paintPlot(QPainter& painter, const score::Skin& skin, const QRectF& area)
  {
    const auto values = shownFrame();
    const auto size = values.size();
    const int levels = m_model->amplitudeLevels();
    const bool bars = size <= maxBarFrameSize && area.width() / size >= 2.;
    const qreal zero = amplitudeY(area, 0.);

    painter.setRenderHint(QPainter::Antialiasing, false);
    if(m_hover && *m_hover < size)
      painter.fillRect(
          QRectF{QPointF{columnLeft(area, *m_hover, size), area.top()},
                 QPointF{columnLeft(area, *m_hover + 1, size), area.bottom()}},
          skin.Emphasis2.main.brush);

    // Sparse, as the curve box: zero, the quarters of the period, and either
    // the half amplitudes or small ticks at the levels a chip can play.
    painter.setPen(skin.DarkGray.main.pen0);
    painter.drawLine(QPointF{area.left(), zero}, QPointF{area.right(), zero});
    for(int quarter = 1; quarter < 4; ++quarter)
    {
      const qreal x = area.left() + area.width() * quarter / 4.;
      painter.drawLine(QPointF{x, area.top()}, QPointF{x, area.bottom()});
    }
    if(levels >= 2)
    {
      int step = 1;
      while(step < levels && area.height() * step / (levels - 1) < 4.)
        step *= 2;
      for(int level = 0; level < levels; level += step)
      {
        const qreal y = amplitudeY(area, 2. * level / (levels - 1) - 1.);
        painter.drawLine(QPointF{area.left(), y}, QPointF{area.left() + 3., y});
      }
    }
    else
      for(double half : {-.5, .5})
        painter.drawLine(QPointF{area.left(), amplitudeY(area, half)},
                         QPointF{area.right(), amplitudeY(area, half)});

    if(m_dragging)
    {
      QColor faint = skin.Gray.color();
      faint.setAlphaF(.6);
      paintOutline(painter, QPen{faint, 1.}, area, m_model->wavetable().frame(m_frame), bars);
    }

    if(bars)
    {
      QColor fill = skin.Base4.color();
      fill.setAlphaF(.35);
      const qreal gap = area.width() / size >= 5. ? 1. : 0.;
      for(std::size_t i = 0; i < size; ++i)
      {
        const qreal left = columnLeft(area, i, size);
        const qreal right = columnLeft(area, i + 1, size) - gap;
        const qreal y = amplitudeY(area, quantize(values[i]));
        painter.fillRect(QRectF{QPointF{left, std::min(y, zero)}, QPointF{right, std::max(y, zero)}}, fill);
        painter.fillRect(QRectF{left, y - 1., right - left, 2.}, skin.Base4.main.brush);
      }
    }
    else
    {
      painter.setRenderHint(QPainter::Antialiasing, true);
      paintOutline(painter, skin.Base4.main.pen1_5, area, values, false);
      painter.setRenderHint(QPainter::Antialiasing, false);
    }

    if(m_execution && !m_dragging)
    {
      // What execution last received, at the size of the edited frame.
      const auto& received = *m_execution;
      const auto frame = std::min(m_frame, received.frameCount() - 1);
      std::vector<float> resampled(size);
      for(std::size_t i = 0; i < size; ++i)
        resampled[i] = Process::sampleWavetable(received, frame, double(i) / size);
      painter.setRenderHint(QPainter::Antialiasing, !bars);
      paintOutline(painter, skin.Base1.main.pen1, area, resampled, bars);
      painter.setRenderHint(QPainter::Antialiasing, false);
    }

    paintText(painter, skin, area);
  }

  //! Steps along the tops of the columns, or a periodic line through their
  //! centres.
  void paintOutline(
      QPainter& painter, const QPen& pen, const QRectF& area, std::span<const float> values,
      bool steps)
  {
    const auto size = values.size();
    QPolygonF line;
    if(steps)
    {
      line.reserve(2 * size);
      for(std::size_t i = 0; i < size; ++i)
      {
        const qreal y = amplitudeY(area, quantize(values[i]));
        line.push_back({columnLeft(area, i, size), y});
        line.push_back({columnLeft(area, i + 1, size), y});
      }
    }
    else
    {
      line.reserve(size + 2);
      const double wrap = .5 * (quantize(values.front()) + quantize(values.back()));
      line.push_back({area.left(), amplitudeY(area, wrap)});
      for(std::size_t i = 0; i < size; ++i)
        line.push_back({area.left() + area.width() * (i + .5) / size,
                        amplitudeY(area, quantize(values[i]))});
      line.push_back({area.right(), amplitudeY(area, wrap)});
    }
    painter.setPen(pen);
    painter.drawPolyline(line);
  }

  //! On a translucent backing, so that bars under the text do not hide it.
  void paintText(QPainter& painter, const score::Skin& skin, const QRectF& area)
  {
    painter.setFont(skin.MonoFontSmall);
    const QFontMetricsF metrics{skin.MonoFontSmall};
    const auto box = area.adjusted(6., 2., -3., -2.);
    const auto status = readout();
    const auto description = label();
    const qreal statusWidth = status.isEmpty() ? 0. : metrics.horizontalAdvance(status) + 8.;
    QColor backing = skin.Dark.color();
    backing.setAlphaF(.6);
    const auto draw = [&](const QString& text, Qt::Alignment alignment, const QPen& pen) {
      const auto bounds = metrics.boundingRect(box, int(alignment | Qt::AlignTop), text);
      painter.fillRect(bounds.adjusted(-2., 0., 2., 0.), backing);
      painter.setPen(pen);
      painter.drawText(box, int(alignment | Qt::AlignTop), text);
    };
    if(metrics.horizontalAdvance(description) + statusWidth <= box.width())
      draw(description, Qt::AlignLeft, skin.Gray.main.pen1);
    if(!status.isEmpty() && statusWidth <= box.width() + 8.)
      draw(status, Qt::AlignRight, skin.HalfLight.main.pen1);
  }

  void hover(QPointF point)
  {
    std::optional<std::size_t> column;
    if(m_model && plotRect().contains(point))
      column = sampleAt(point).first;
    if(column != m_hover)
    {
      m_hover = column;
      update();
    }
  }

  void clampHover()
  {
    if(m_hover && (!m_model || *m_hover >= m_model->wavetable().frameSize))
      m_hover.reset();
  }

  void clickStrip(QPointF point)
  {
    const auto count = m_model->wavetable().frameCount();
    std::size_t selected = m_frame;
    if(thumbnails())
    {
      for(std::size_t i = 0; i < count; ++i)
        if(thumbnailRect(i).adjusted(0., 0., 2., 0.).contains(point))
          selected = i;
    }
    else
    {
      const auto strip = stripRect();
      if(point.x() < strip.left() + arrowWidth && m_frame > 0)
        selected = m_frame - 1;
      else if(point.x() > strip.right() - arrowWidth && m_frame + 1 < count)
        selected = m_frame + 1;
    }
    if(selected != m_frame)
    {
      m_frame = selected;
      update();
    }
  }

  float quantize(double value) const
  {
    return Process::quantizeSample(value, m_model->amplitudeLevels());
  }

  std::pair<std::size_t, float> sampleAt(QPointF point) const
  {
    const auto area = plotRect();
    const auto size = m_model->wavetable().frameSize;
    const double phase = std::clamp((point.x() - area.left()) / area.width(), 0., 1.);
    return {std::min(std::size_t(phase * size), size - 1),
        quantize(1. - 2. * (point.y() - area.top()) / area.height())};
  }

  void paintSamples(std::pair<std::size_t, float> current)
  {
    const auto low = std::min(m_previous.first, current.first);
    const auto high = std::max(m_previous.first, current.first);
    for(auto i = low; i <= high; ++i)
    {
      const double t = current.first == m_previous.first ? 1.
          : (double(i) - double(m_previous.first)) / (double(current.first) - double(m_previous.first));
      m_preview[i]
          = quantize(m_previous.second + t * (current.second - m_previous.second));
    }
    m_previous = current;
    update();
  }

  void submit(const Process::WavetableData& data)
  {
    if(data == m_model->wavetable())
      return;
    m_submitting = true;
    m_dispatcher.submit<Process::SetControlValue>(*m_model, Process::wavetableToValue(data));
    m_submitting = false;
  }

  void finish(bool commit)
  {
    if(!m_dragging)
      return;
    m_dragging = false;
    if(commit && m_model)
    {
      const auto& table = m_model->wavetable();
      const auto frame = table.frame(m_frame);
      if(!std::equal(m_preview.begin(), m_preview.end(), frame.begin(), frame.end()))
      {
        auto data = table;
        std::copy(m_preview.begin(), m_preview.end(), data.samples.begin() + m_frame * data.frameSize);
        submit(data);
      }
    }
    m_preview.clear();
    update();
  }

  //! Applies `function` to the edited frame, quantized to the port's levels.
  template <typename Function>
  void editFrame(Function function)
  {
    if(!m_model)
      return;
    finish(true);
    auto data = m_model->wavetable();
    const int levels = m_model->amplitudeLevels();
    const std::span<float> frame{data.samples.data() + m_frame * data.frameSize, data.frameSize};
    function(frame, levels);
    for(float& sample : frame)
      sample = Process::quantizeSample(sample, levels);
    submit(data);
  }

  void editBank(BankAction action)
  {
    if(!m_model || !m_model->supportsFrames())
      return;
    finish(true);
    auto data = m_model->wavetable();
    auto selected = m_frame;
    const auto size = data.frameSize;
    if(action == BankAction::Add || action == BankAction::Duplicate)
    {
      if(data.frameCount() >= Process::WavetableData::maxFrames
         || data.samples.size() > Process::WavetableData::maxSamples - size)
        return;
      std::vector<float> frame(size, quantize(0.));
      if(action == BankAction::Duplicate)
        std::copy_n(data.samples.begin() + selected * size, size, frame.begin());
      data.samples.insert(data.samples.begin() + (++selected) * size, frame.begin(), frame.end());
    }
    else if(action == BankAction::Remove && data.frameCount() > 1)
    {
      data.samples.erase(data.samples.begin() + selected * size, data.samples.begin() + (selected + 1) * size);
      selected = std::min(selected, data.frameCount() - 1);
    }
    else if((action == BankAction::Earlier && selected > 0)
            || (action == BankAction::Later && selected + 1 < data.frameCount()))
    {
      const auto target = action == BankAction::Earlier ? selected - 1 : selected + 1;
      std::swap_ranges(data.samples.begin() + selected * size,
          data.samples.begin() + (selected + 1) * size, data.samples.begin() + target * size);
      selected = target;
    }
    else
      return;
    submit(data);
    m_frame = selected;
    update();
  }

  void resizeFrames(std::size_t size)
  {
    if(!m_model || m_model->frameSizeConstraint())
      return;
    finish(true);
    const auto& current = m_model->wavetable();
    if(size < 2 || size == current.frameSize || size > Process::WavetableData::maxFrameSize
       || size > Process::WavetableData::maxSamples / current.frameCount())
      return;
    submit(Process::resampleWavetable(current, size));
  }

  QPointer<WaveformInlet> m_model;
  CommandDispatcher<> m_dispatcher;
  QRectF m_rect{0., 0., 240., 120.};
  std::size_t m_frame{};
  std::optional<std::size_t> m_hover;
  std::pair<std::size_t, float> m_previous{};
  std::vector<float> m_preview;
  std::optional<Process::WavetableData> m_execution;
  bool m_dragging{};
  bool m_submitting{};
};

class WaveformWidget final : public QGraphicsView
{
public:
  WaveformWidget(WaveformInlet& model, const score::DocumentContext& ctx, QWidget* parent)
      : QGraphicsView{parent}, m_item{new WaveformItem{model, ctx, nullptr}}
  {
    auto* graphicsScene = new QGraphicsScene{this};
    setScene(graphicsScene);
    graphicsScene->addItem(m_item);
    setFrameShape(QFrame::NoFrame);
    setBackgroundBrush(score::Skin::instance().Dark.main.brush);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setMinimumSize(80, 90);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setRenderHint(QPainter::Antialiasing);
  }
  QSize sizeHint() const override { return {320, 160}; }

protected:
  void resizeEvent(QResizeEvent* event) override
  {
    QGraphicsView::resizeEvent(event);
    const QRectF rect{0., 0., qreal(viewport()->width()), qreal(viewport()->height())};
    m_item->setRect(rect);
    setSceneRect(rect);
  }

private:
  WaveformItem* m_item{};
};

WaveformItem& waveformItem(const QGraphicsItem& item)
{
  auto* waveform = dynamic_cast<const WaveformItem*>(&item);
  SCORE_ASSERT(waveform);
  return const_cast<WaveformItem&>(*waveform);
}
}

void setWaveformItemModel(QGraphicsItem& item, WaveformInlet& model)
{
  waveformItem(item).setModel(model);
}

void setWaveformItemRect(QGraphicsItem& item, const QRectF& rect)
{
  waveformItem(item).setRect(rect);
}

WaveformItemState waveformItemState(const QGraphicsItem& item)
{
  const auto& waveform = waveformItem(item);
  return {waveform.plotRect(), waveform.stripRect(), waveform.frame(), waveform.label(),
          waveform.readout()};
}

void fillWaveformItemMenu(QGraphicsItem& item, QMenu& menu)
{
  waveformItem(item).fillContextMenu(menu);
}
}

QWidget* WidgetFactory::WaveformInletItems::make_widget(
    const Dataflow::WaveformInlet& inlet, const score::DocumentContext& ctx,
    QWidget* parent, QObject*)
{
  return new Dataflow::WaveformWidget{const_cast<Dataflow::WaveformInlet&>(inlet), ctx, parent};
}

QGraphicsItem* WidgetFactory::WaveformInletItems::make_item(
    const Dataflow::WaveformInlet&, const Dataflow::WaveformInlet& inlet,
    const score::DocumentContext& ctx, QGraphicsItem* parent, QObject*)
{
  return new Dataflow::WaveformItem{const_cast<Dataflow::WaveformInlet&>(inlet), ctx, parent};
}

template <>
void DataStreamReader::read(const Dataflow::WaveformInlet& port)
{
  read(static_cast<const Process::ControlInlet&>(port));
  m_stream << port.amplitudeLevels() << quint64(port.frameSizeConstraint());
}

template <>
void DataStreamWriter::write(Dataflow::WaveformInlet& port)
{
  quint64 size{};
  m_stream >> port.m_amplitudeLevels >> size;
  // Checked by the port once loaded.
  port.m_frameSizeConstraint = std::min<quint64>(size, Process::WavetableData::maxFrameSize + 1);
}

template <>
void JSONReader::read(const Dataflow::WaveformInlet& port)
{
  read(static_cast<const Process::ControlInlet&>(port));
  obj["AmplitudeLevels"] = port.amplitudeLevels();
  obj["FrameSizeConstraint"] = int(port.frameSizeConstraint());
}

template <>
void JSONWriter::write(Dataflow::WaveformInlet& port)
{
  port.m_amplitudeLevels = obj["AmplitudeLevels"].toInt();
  // Checked by the port once loaded.
  const int size = obj["FrameSizeConstraint"].toInt();
  port.m_frameSizeConstraint = size < 0 ? Process::WavetableData::maxFrameSize + 1 : std::size_t(size);
}

template <>
void DataStreamReader::read(const Dataflow::WavetableInlet& port)
{
  read(static_cast<const Dataflow::WaveformInlet&>(port));
}

template <>
void DataStreamWriter::write(Dataflow::WavetableInlet& port)
{
  write(static_cast<Dataflow::WaveformInlet&>(port));
}

template <>
void JSONReader::read(const Dataflow::WavetableInlet& port)
{
  read(static_cast<const Dataflow::WaveformInlet&>(port));
}

template <>
void JSONWriter::write(Dataflow::WavetableInlet& port)
{
  write(static_cast<Dataflow::WaveformInlet&>(port));
}
