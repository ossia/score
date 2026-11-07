#include "WaveformInlet.hpp"

#include <Process/Commands/SetControlValue.hpp>

#include <State/Value.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/Skin.hpp>

#include <QClipboard>
#include <QDebug>
#include <QFocusEvent>
#include <QFontMetricsF>
#include <QGraphicsObject>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneWheelEvent>
#include <QGraphicsView>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <functional>

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
  // The canonical value this port holds, as undo and documents give it back,
  // is taken as it is.
  if(auto canonical = Process::wavetableFromValue(value); canonical && accepts(*canonical))
  {
    setWaveform(*canonical);
    return;
  }

  // Any other shape execution plays (Process::waveformFrame) is adapted to
  // this port as a cable's value would be: the first frame of a bank for a
  // single frame, the frame size of the constraint, the amplitude levels.
  auto data = Process::waveformFromValue(value);
  if(!data)
  {
    qWarning() << "Waveform port: ignoring a value that holds no list of numbers";
    return;
  }
  if(!m_bank && data->frameCount() > 1)
    data->samples.resize(data->frameSize);
  if(m_frameSizeConstraint && data->frameSize != m_frameSizeConstraint)
  {
    if(data->frameCount() > Process::WavetableData::maxSamples / m_frameSizeConstraint)
    {
      qWarning() << "Waveform port: ignoring a bank too large for its frame size";
      return;
    }
    *data = Process::resampleWavetable(*data, m_frameSizeConstraint);
  }
  if(m_amplitudeLevels >= 2)
    for(float& sample : data->samples)
      sample = Process::quantizeSample(sample, m_amplitudeLevels);
  setWaveform(*data);
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

QString waveformFrameText(std::span<const float> frame)
{
  QString text;
  text.reserve(frame.size() * 8);
  for(std::size_t i = 0; i < frame.size(); ++i)
  {
    char buffer[32];
    const auto end = std::to_chars(buffer, buffer + sizeof(buffer), frame[i]).ptr;
    if(i)
      text += QLatin1Char(' ');
    text += QLatin1String{buffer, end};
  }
  return text;
}

std::optional<std::vector<float>>
waveformFrameFromText(QStringView text, std::size_t size, int levels)
{
  if(size < 2)
    return std::nullopt;

  // A list message, with or without a pair of brackets: numbers separated by
  // spaces, commas or semicolons, each read exactly so that copied frames
  // paste back unchanged. Anything else, a bank's nested lists or bools, is
  // read as score reads a value.
  const auto plainList = [&]() -> std::optional<ossia::value> {
    if(text.count(u'[') > 1 || text.count(u'(') > 1)
      return std::nullopt;
    static const QRegularExpression separators{QStringLiteral("[\\s,;()\\[\\]]+")};
    const auto tokens = text.toString().split(separators, Qt::SkipEmptyParts);
    if(tokens.empty() || std::size_t(tokens.size()) > Process::WavetableData::maxFrameSize)
      return std::nullopt;
    std::vector<ossia::value> numbers;
    numbers.reserve(tokens.size());
    for(const auto& token : tokens)
    {
      const auto latin = token.toLatin1();
      const char* end = latin.constData() + latin.size();
      float number{};
      const auto res = std::from_chars(latin.constData(), end, number);
      if(res.ec != std::errc{} || res.ptr != end)
        return std::nullopt;
      numbers.emplace_back(number);
    }
    return numbers;
  };
  auto value = plainList();
  if(!value)
  {
    const auto utf8 = text.toUtf8();
    value = State::parseValue(std::string_view{utf8.constData(), std::size_t(utf8.size())});
    if(!value)
      return std::nullopt;
  }

  // One number, alone or in a list, fills the frame.
  std::optional<float> constant = Process::waveformSample(*value);
  if(const auto* list = value->target<std::vector<ossia::value>>(); list && list->size() == 1)
    constant = Process::waveformSample(list->front());

  std::vector<float> frame(size);
  if(constant)
    std::fill(frame.begin(), frame.end(), *constant);
  else
  {
    const auto samples = Process::waveformFrame(*value);
    if(!samples)
      return std::nullopt;
    Process::WavetableData source{samples->size(), {}};
    source.samples.reserve(samples->size());
    for(const auto& sample : *samples)
      source.samples.push_back(*Process::waveformSample(sample));
    if(source.frameSize == size)
      frame = std::move(source.samples);
    else
      for(std::size_t i = 0; i < size; ++i)
        frame[i] = Process::sampleWavetable(source, 0, double(i) / size);
  }
  for(float& sample : frame)
    sample = Process::quantizeSample(sample, levels);
  return frame;
}

namespace
{
//! Frames up to this size are drawn as one bar per sample; larger ones as a
//! line through the column centres.
constexpr std::size_t maxBarFrameSize = 128;
constexpr qreal stripHeight = 16.;
constexpr qreal minThumbnailWidth = 24.;
constexpr qreal arrowWidth = 16.;
//! How far, in pixels, a continuous pen is caught by a reference line.
constexpr qreal snapDistance = 3.;
constexpr qreal dragDistance = 4.;

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

enum class Transform { Normalize, Invert, Reverse, RotateLeft, RotateRight, Smooth, Clear };

void applyTransform(Transform transform, std::span<float> frame, int levels)
{
  switch(transform)
  {
    case Transform::Normalize: Process::normalizeWaveform(frame, levels); break;
    case Transform::Invert: Process::invertWaveform(frame, levels); break;
    case Transform::Reverse: Process::reverseWaveform(frame); break;
    case Transform::RotateLeft: Process::rotateWaveform(frame, true); break;
    case Transform::RotateRight: Process::rotateWaveform(frame, false); break;
    case Transform::Smooth: Process::smoothWaveform(frame, levels); break;
    case Transform::Clear: Process::clearWaveform(frame, levels); break;
  }
}

const std::initializer_list<std::pair<const char*, Process::WaveformShape>>& shapeNames()
{
  using Shape = Process::WaveformShape;
  static const std::initializer_list<std::pair<const char*, Shape>> names{
      {QT_TR_NOOP("Sine"), Shape::Sine},
      {QT_TR_NOOP("Triangle"), Shape::Triangle},
      {QT_TR_NOOP("Saw up"), Shape::SawUp},
      {QT_TR_NOOP("Saw down"), Shape::SawDown},
      {QT_TR_NOOP("Square"), Shape::Square},
      {QT_TR_NOOP("Pulse 25%"), Shape::Pulse25},
      {QT_TR_NOOP("Pulse 12.5%"), Shape::Pulse12},
      {QT_TR_NOOP("Noise"), Shape::Noise}};
  return names;
}

//! Both scene and inspector own this same editor. Its selection never enters
//! the document selection stack, and every edit targets the real port path.
//!
//! Samples occupy equal columns of the plot; drawing and hit testing share
//! that geometry. Banks add a strip above the plot to choose the frame being
//! edited. Shapes, transforms, resampling and frame operations are in the
//! context menu, each one undoable command. Key repeats of a nudge go through
//! the document's ongoing dispatcher, so that a held key is one command.
class WaveformItem final : public QGraphicsObject
{
public:
  WaveformItem(WaveformInlet& model, const score::DocumentContext& ctx, QGraphicsItem* parent)
      : QGraphicsObject{parent}
      , m_dispatcher{ctx.commandStack}
      , m_ongoing{ctx.dispatcher}
  {
    setObjectName(QStringLiteral("WaveformEditor"));
    setData(0, QStringLiteral("WaveformEditor"));
    setFlags(ItemIsFocusable);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
    setModel(model);
  }

  ~WaveformItem() override
  {
    finish(false);
    commitNudge();
  }

  //! Called when the edited frame of a bank changes.
  std::function<void()> frameChanged;

  WaveformInlet* model() const noexcept { return m_model; }

  void setModel(WaveformInlet& model)
  {
    if(m_model == &model)
      return;
    finish(false);
    commitNudge();
    if(m_model)
      disconnect(m_model, nullptr, this, nullptr);
    m_model = &model;
    endLive();
    m_cursor.reset();
    m_strip.reset();
    connect(&model, &Process::ControlInlet::executionValueChanged, this,
            [this](const ossia::value& value) {
      if(auto received = Process::waveformFromValue(value))
      {
        m_execution = std::move(*received);
        m_liveStale = true;
        update();
      }
    });
    connect(&model, &Process::Port::executionReset, this, [this] { endLive(); });
    // Nothing reaches the port any more: what it last received is no longer
    // what plays from it.
    const auto unwired = [this] {
      if(m_model && m_model->cables().empty() && !m_model->address().isSet())
        endLive();
    };
    connect(&model, &Process::Port::cablesChanged, this, unwired);
    connect(&model, &Process::Port::addressChanged, this, unwired);
    connect(&model, &Process::ControlInlet::valueChanged, this, [this] {
      if(!m_submitting)
      {
        finish(false);
        commitNudge();
      }
      // An edit, an undo or a preset shows: execution shows again when it
      // next receives something.
      m_execution.reset();
      m_liveStale = true;
      if(m_frame >= m_model->wavetable().frameCount())
        setFrame(m_model->wavetable().frameCount() - 1);
      clampHover();
      update();
    });
    connect(&model, &WaveformInlet::constraintsChanged, this, [this] {
      m_liveStale = true;
      finish(false);
      clampHover();
      update();
    });
    connect(&model, &QObject::destroyed, this, [this] {
      m_dragging = false;
      m_nudging = false;
      m_preview.clear();
      m_execution.reset();
      m_liveFrame.clear();
      m_hover.reset();
      m_cursor.reset();
      m_strip.reset();
      m_model = nullptr;
      update();
    });
    setFrame(0);
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
  bool live() const noexcept { return m_model && m_execution && !m_dragging; }

  QString label() const
  {
    if(!m_model)
      return {};
    QString text = tr("%1 smp").arg(m_model->wavetable().frameSize);
    if(const auto levels = levelDescription(m_model->amplitudeLevels()); !levels.isEmpty())
      text += QStringLiteral(", ") + levels;
    return text;
  }

  //! What the readout and the column highlight describe.
  std::optional<std::size_t> pointed() const
  {
    if(m_hover)
      return m_hover;
    if(hasFocus())
      return m_cursor;
    return std::nullopt;
  }

  QString readout() const
  {
    const auto sample = pointed();
    if(!m_model || !sample)
      return {};
    const auto values = displayedFrame();
    if(*sample >= values.size())
      return {};
    const float value = quantize(values[*sample]);
    const int levels = m_model->amplitudeLevels();
    const QString amplitude = levels >= 2
        ? QStringLiteral("%1/%2").arg(Process::sampleLevel(value, levels)).arg(levels - 1)
        : QString::number(value, 'f', 2);
    return QStringLiteral("#%1 %2").arg(*sample).arg(amplitude);
  }

  QString frameText() const
  {
    return m_model ? waveformFrameText(m_model->wavetable().frame(m_frame)) : QString{};
  }

  //! Replaces the edited frame with a list of numbers; false if it is not one.
  bool pasteText(QStringView text)
  {
    if(!m_model)
      return false;
    auto frame = waveformFrameFromText(text, m_model->wavetable().frameSize, m_model->amplitudeLevels());
    if(!frame)
      return false;
    editFrame([&](std::span<float> target, int) { std::copy(frame->begin(), frame->end(), target.begin()); });
    return true;
  }

  void applyShape(Process::WaveformShape shape)
  {
    const auto seed = QRandomGenerator::global()->generate();
    editFrame([shape, seed](std::span<float> frame, int levels) {
      Process::generateWaveform(frame, shape, levels, seed);
    });
  }

  void transform(Transform transform)
  {
    editFrame([transform](std::span<float> frame, int levels) { applyTransform(transform, frame, levels); });
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

  void fillContextMenu(QMenu& menu, bool withShapes = true)
  {
    if(!m_model)
      return;
    finish(true);
    commitNudge();
    if(withShapes)
    {
      auto* shapes = menu.addMenu(tr("Shape"));
      for(auto [name, shape] : shapeNames())
        connect(shapes->addAction(tr(name)), &QAction::triggered, this,
                [this, shape] { applyShape(shape); });
      menu.addSeparator();
    }
    for(auto [name, function] : std::initializer_list<std::pair<QString, Transform>>{
            {tr("Normalize"), Transform::Normalize},
            {tr("Invert"), Transform::Invert},
            {tr("Reverse"), Transform::Reverse},
            {tr("Rotate left"), Transform::RotateLeft},
            {tr("Rotate right"), Transform::RotateRight},
            {tr("Smooth"), Transform::Smooth},
            {tr("Clear"), Transform::Clear}})
      connect(menu.addAction(name), &QAction::triggered, this,
              [this, function] { transform(function); });

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
      const bool canAdd = this->canAdd();
      const auto frameAction = [&](const QString& name, auto action, bool enabled) {
        auto* item = menu.addAction(name);
        item->setEnabled(enabled);
        connect(item, &QAction::triggered, this, action);
      };
      frameAction(tr("Add frame"), [this] { insertFrame(m_frame, false); }, canAdd);
      frameAction(tr("Duplicate frame"), [this] { insertFrame(m_frame, true); }, canAdd);
      frameAction(tr("Remove frame"), [this] { removeFrame(); }, table.frameCount() > 1);
      frameAction(tr("Move frame left"), [this] { moveFrame(m_frame, m_frame - 1); }, m_frame > 0);
      frameAction(tr("Move frame right"), [this] { moveFrame(m_frame, m_frame + 1); },
                  m_frame + 1 < table.frameCount());
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
    commitNudge();
    if(stripRect().contains(event->pos()))
    {
      clickStrip(event->pos());
      if(thumbnails())
        m_strip = StripDrag{m_frame, event->pos(), std::nullopt};
      return;
    }
    if(!plotRect().contains(event->pos()))
      return;
    const auto frame = m_model->wavetable().frame(m_frame);
    m_preview.assign(frame.begin(), frame.end());
    m_dragging = true;
    m_line = event->modifiers() & Qt::ShiftModifier;
    m_previous = sampleAt(event->pos(), event->modifiers());
    m_lineStart = m_previous;
    m_hover = m_cursor = m_previous.first;
    paintSamples(m_previous);
  }

  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override
  {
    event->accept();
    if(m_strip && m_model)
    {
      if(m_strip->target || std::abs(event->pos().x() - m_strip->press.x()) >= dragDistance)
      {
        m_strip->target = thumbnailAt(event->pos().x());
        update(stripRect());
      }
      return;
    }
    if(m_dragging && m_model)
    {
      const auto sample = sampleAt(event->pos(), event->modifiers());
      m_hover = m_cursor = sample.first;
      paintSamples(sample);
    }
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
  {
    event->accept();
    if(m_strip)
    {
      const auto drag = *m_strip;
      m_strip.reset();
      if(m_model && drag.target && *drag.target != drag.frame)
        moveFrame(drag.frame, *drag.target);
      update();
      return;
    }
    if(m_dragging && m_model)
      paintSamples(sampleAt(event->pos(), event->modifiers()));
    finish(true);
    hover(event->pos());
  }

  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override
  {
    if(m_model && event->button() == Qt::LeftButton && stripRect().contains(event->pos())
       && thumbnails())
    {
      event->accept();
      setFocus(Qt::MouseFocusReason);
      finish(true);
      commitNudge();
      clickStrip(event->pos());
      if(canAdd())
        insertFrame(m_frame, true);
      return;
    }
    mousePressEvent(event);
  }

  void wheelEvent(QGraphicsSceneWheelEvent* event) override
  {
    if(!m_model || !stripRect().contains(event->pos()) || event->orientation() != Qt::Vertical
       || event->delta() == 0)
    {
      event->ignore();
      return;
    }
    event->accept();
    finish(true);
    commitNudge();
    stepFrame(event->delta() > 0 ? -1 : 1);
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

  //! The keys the editor takes over from the application's shortcuts. Only
  //! the focus item is asked, so these stay global while it is not focused.
  bool handlesKey(const QKeyEvent& event) const
  {
    if(!m_model)
      return false;
    if(event.matches(QKeySequence::Copy) || event.matches(QKeySequence::Paste))
      return true;
    const auto modifiers = event.modifiers() & ~Qt::KeypadModifier;
    switch(event.key())
    {
      case Qt::Key_Escape:
        return true;
      case Qt::Key_Up:
      case Qt::Key_Down:
        return modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier;
      case Qt::Key_Left:
      case Qt::Key_Right:
      case Qt::Key_Delete:
      case Qt::Key_Backspace:
        return modifiers == Qt::NoModifier;
      case Qt::Key_PageUp:
      case Qt::Key_PageDown:
        return modifiers == Qt::NoModifier && m_model->supportsFrames();
      default:
        return false;
    }
  }

  void keyPressEvent(QKeyEvent* event) override
  {
    if(!handlesKey(*event))
    {
      QGraphicsObject::keyPressEvent(event);
      return;
    }
    event->accept();
    const bool nudge = event->key() == Qt::Key_Up || event->key() == Qt::Key_Down;
    if(!nudge)
      commitNudge();
    if(event->key() == Qt::Key_Escape)
    {
      finish(false);
      return;
    }
    finish(true);
    if(event->matches(QKeySequence::Copy))
    {
      if(auto* clipboard = QGuiApplication::clipboard())
        clipboard->setText(frameText());
      return;
    }
    if(event->matches(QKeySequence::Paste))
    {
      if(auto* clipboard = QGuiApplication::clipboard())
        pasteText(clipboard->text());
      return;
    }
    switch(event->key())
    {
      case Qt::Key_Up:
      case Qt::Key_Down:
        nudgeSample(event->key() == Qt::Key_Up ? 1 : -1, event->modifiers() & Qt::ShiftModifier);
        break;
      case Qt::Key_Left:
        transform(Transform::RotateLeft);
        break;
      case Qt::Key_Right:
        transform(Transform::RotateRight);
        break;
      case Qt::Key_Delete:
      case Qt::Key_Backspace:
        transform(Transform::Clear);
        break;
      case Qt::Key_PageUp:
        stepFrame(-1);
        break;
      case Qt::Key_PageDown:
        stepFrame(1);
        break;
      default:
        break;
    }
  }

  void keyReleaseEvent(QKeyEvent* event) override
  {
    if((event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) && !event->isAutoRepeat())
    {
      commitNudge();
      event->accept();
      return;
    }
    QGraphicsObject::keyReleaseEvent(event);
  }

  void focusOutEvent(QFocusEvent* event) override
  {
    finish(true);
    commitNudge();
    update();
    QGraphicsObject::focusOutEvent(event);
  }

  void focusInEvent(QFocusEvent* event) override
  {
    update();
    QGraphicsObject::focusInEvent(event);
  }

  bool sceneEvent(QEvent* event) override
  {
    if(event->type() == QEvent::ShortcutOverride && handlesKey(*static_cast<QKeyEvent*>(event)))
    {
      event->accept();
      return true;
    }
    if(event->type() == QEvent::UngrabMouse)
    {
      finish(true);
      if(m_strip)
      {
        m_strip.reset();
        update();
      }
    }
    return QGraphicsObject::sceneEvent(event);
  }

private:
  struct StripDrag
  {
    std::size_t frame{};
    QPointF press;
    std::optional<std::size_t> target;
  };

  std::span<const float> shownFrame() const
  {
    return m_dragging ? std::span<const float>{m_preview} : m_model->wavetable().frame(m_frame);
  }

  void endLive()
  {
    m_execution.reset();
    m_liveStale = true;
    update();
  }

  //! What execution last received, at the size of the edited frame. Kept
  //! until what it depends on changes, as each paint reads it twice.
  std::span<const float> liveFrame() const
  {
    if(m_liveStale)
    {
      const auto& received = *m_execution;
      const auto size = m_model->wavetable().frameSize;
      const auto frame = std::min(m_frame, received.frameCount() - 1);
      m_liveFrame.resize(size);
      for(std::size_t i = 0; i < size; ++i)
        m_liveFrame[i] = Process::sampleWavetable(received, frame, double(i) / size);
      m_liveStale = false;
    }
    return m_liveFrame;
  }

  //! The frame drawn in front: the edit preview, what plays, or the document.
  std::span<const float> displayedFrame() const
  {
    return live() ? liveFrame() : shownFrame();
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

  qreal thumbnailWidth() const
  {
    return std::min(48., stripRect().width() / m_model->wavetable().frameCount());
  }

  QRectF thumbnailRect(std::size_t frame) const
  {
    const auto strip = stripRect();
    const qreal width = thumbnailWidth();
    return {strip.left() + frame * width, strip.top(), width - 2., strip.height()};
  }

  std::size_t thumbnailAt(qreal x) const
  {
    const auto count = m_model->wavetable().frameCount();
    const double index = std::floor((x - stripRect().left()) / thumbnailWidth());
    return std::size_t(std::clamp(index, 0., double(count - 1)));
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
      // Where the dragged frame lands: before the hovered thumbnail when
      // moving left, after it when moving right.
      if(m_strip && m_strip->target && *m_strip->target != m_strip->frame)
      {
        const auto box = thumbnailRect(*m_strip->target);
        const qreal x = *m_strip->target > m_strip->frame ? box.right() : box.left() - 2.;
        painter.fillRect(QRectF{x, strip.top(), 2., strip.height()}, skin.Base4.main.brush);
      }
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
    const bool playing = live();
    const auto values = displayedFrame();
    const auto size = values.size();
    const int levels = m_model->amplitudeLevels();
    const bool bars = size <= maxBarFrameSize && area.width() / size >= 2.;
    const qreal zero = amplitudeY(area, 0.);

    painter.setRenderHint(QPainter::Antialiasing, false);
    if(const auto sample = pointed(); sample && *sample < size)
      painter.fillRect(
          QRectF{QPointF{columnLeft(area, *sample, size), area.top()},
                 QPointF{columnLeft(area, *sample + 1, size), area.bottom()}},
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

    // Behind the front frame: the document, dimmed while execution plays
    // something else, grey while being edited.
    if(m_dragging || playing)
    {
      QColor faint = playing ? skin.Base4.color() : skin.Gray.color();
      faint.setAlphaF(playing ? .45 : .6);
      paintOutline(painter, QPen{faint, 1.}, area, m_model->wavetable().frame(m_frame), bars);
    }

    // What plays takes another hue than the authored frame, so that the two
    // read apart at a glance.
    const QColor front = playing ? skin.Base1.color() : skin.Base4.color();
    if(bars)
    {
      QColor fill = front;
      fill.setAlphaF(.35);
      const qreal gap = area.width() / size >= 5. ? 1. : 0.;
      for(std::size_t i = 0; i < size; ++i)
      {
        const qreal left = columnLeft(area, i, size);
        const qreal right = columnLeft(area, i + 1, size) - gap;
        const qreal y = amplitudeY(area, quantize(values[i]));
        painter.fillRect(QRectF{QPointF{left, std::min(y, zero)}, QPointF{right, std::max(y, zero)}}, fill);
        painter.fillRect(QRectF{left, y - 1., right - left, 2.}, front);
      }
    }
    else
    {
      painter.setRenderHint(QPainter::Antialiasing, true);
      paintOutline(painter, QPen{front, 1.5}, area, values, false);
      painter.setRenderHint(QPainter::Antialiasing, false);
    }

    if(m_dragging && m_line)
    {
      const auto centre = [&](std::pair<std::size_t, float> sample) {
        return QPointF{area.left() + area.width() * (sample.first + .5) / size,
                       amplitudeY(area, sample.second)};
      };
      painter.setRenderHint(QPainter::Antialiasing, true);
      painter.setPen(skin.HalfLight.main.pen1);
      painter.drawLine(centre(m_lineStart), centre(m_previous));
      painter.setRenderHint(QPainter::Antialiasing, false);
    }

    paintText(painter, skin, area, playing);
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
  void paintText(QPainter& painter, const score::Skin& skin, const QRectF& area, bool playing)
  {
    painter.setFont(skin.MonoFontSmall);
    const QFontMetricsF metrics{skin.MonoFontSmall};
    const auto box = area.adjusted(6., 2., -3., -2.);
    const auto status = readout();
    const auto description = label();
    const QString marker = playing ? tr("live") : QString{};
    const qreal statusWidth = status.isEmpty() ? 0. : metrics.horizontalAdvance(status) + 8.;
    const qreal markerWidth = marker.isEmpty() ? 0. : metrics.horizontalAdvance(marker) + 8.;
    QColor backing = skin.Dark.color();
    backing.setAlphaF(.6);
    const auto draw = [&](const QRectF& where, const QString& text, Qt::Alignment alignment,
                          const QPen& pen) {
      const auto bounds = metrics.boundingRect(where, int(alignment | Qt::AlignTop), text);
      painter.fillRect(bounds.adjusted(-2., 0., 2., 0.), backing);
      painter.setPen(pen);
      painter.drawText(where, int(alignment | Qt::AlignTop), text);
    };
    qreal left = 0.;
    if(markerWidth && markerWidth + statusWidth <= box.width() + 8.)
    {
      draw(box, marker, Qt::AlignLeft, QPen{skin.Base1.color(), 1.});
      left = markerWidth;
    }
    if(left + metrics.horizontalAdvance(description) + statusWidth <= box.width())
      draw(box.adjusted(left, 0., 0., 0.), description, Qt::AlignLeft, skin.Gray.main.pen1);
    if(!status.isEmpty() && statusWidth <= box.width() + 8.)
      draw(box, status, Qt::AlignRight, skin.HalfLight.main.pen1);
  }

  void hover(QPointF point)
  {
    std::optional<std::size_t> column;
    if(m_model && plotRect().contains(point))
      column = sampleAt(point, {}).first;
    if(column)
      m_cursor = column;
    if(column != m_hover)
    {
      m_hover = column;
      update();
    }
  }

  void clampHover()
  {
    const auto size = m_model ? m_model->wavetable().frameSize : 0;
    if(m_hover && *m_hover >= size)
      m_hover.reset();
    if(m_cursor && *m_cursor >= size)
      m_cursor.reset();
  }

  void setFrame(std::size_t frame)
  {
    if(frame == m_frame)
      return;
    m_frame = frame;
    m_liveStale = true;
    update();
    if(frameChanged)
      frameChanged();
  }

  void stepFrame(int direction)
  {
    const auto count = m_model->wavetable().frameCount();
    if(direction < 0 && m_frame > 0)
      setFrame(m_frame - 1);
    else if(direction > 0 && m_frame + 1 < count)
      setFrame(m_frame + 1);
  }

  void clickStrip(QPointF point)
  {
    const auto count = m_model->wavetable().frameCount();
    if(thumbnails())
    {
      for(std::size_t i = 0; i < count; ++i)
        if(thumbnailRect(i).adjusted(0., 0., 2., 0.).contains(point))
          setFrame(i);
      return;
    }
    const auto strip = stripRect();
    if(point.x() < strip.left() + arrowWidth)
      stepFrame(-1);
    else if(point.x() > strip.right() - arrowWidth)
      stepFrame(1);
  }

  float quantize(double value) const
  {
    return Process::quantizeSample(value, m_model->amplitudeLevels());
  }

  //! The sample under `point` and the amplitude it draws. A continuous pen is
  //! caught by the zero, half and full amplitude lines; Alt draws freely.
  std::pair<std::size_t, float> sampleAt(QPointF point, Qt::KeyboardModifiers modifiers) const
  {
    const auto area = plotRect();
    const auto size = m_model->wavetable().frameSize;
    const double phase = std::clamp((point.x() - area.left()) / area.width(), 0., 1.);
    double amplitude = 1. - 2. * (point.y() - area.top()) / area.height();
    if(m_model->amplitudeLevels() < 2 && !(modifiers & Qt::AltModifier))
    {
      const double tolerance = 2. * snapDistance / area.height();
      for(double line : {-1., -.5, 0., .5, 1.})
        if(std::abs(amplitude - line) <= tolerance)
          amplitude = line;
    }
    return {std::min(std::size_t(phase * size), size - 1), quantize(amplitude)};
  }

  //! Draws from the previous sample to `current`, or for a line from where
  //! the drag started over the frame as it was.
  void paintSamples(std::pair<std::size_t, float> current)
  {
    if(m_line)
    {
      const auto frame = m_model->wavetable().frame(m_frame);
      std::copy(frame.begin(), frame.end(), m_preview.begin());
      m_previous = m_lineStart;
    }
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
    m_line = false;
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

  //! Moves one sample by a level, or a 64th of the range on a continuous
  //! port; `large` moves an eighth of the range.
  void nudgeSample(int direction, bool large)
  {
    if(!m_model)
      return;
    auto data = m_model->wavetable();
    if(!m_cursor)
      m_cursor = 0;
    const auto index = m_frame * data.frameSize + *m_cursor;
    const int levels = m_model->amplitudeLevels();
    const double step = levels >= 2
        ? 2. / (levels - 1) * (large ? std::max(1l, std::lround((levels - 1) / 8.)) : 1)
        : (large ? 1. / 8. : 1. / 64.);
    const float value = quantize(data.samples[index] + direction * step);
    update();
    if(value == data.samples[index])
      return;
    data.samples[index] = value;
    m_submitting = true;
    m_ongoing.submit<Process::SetControlValue>(*m_model, Process::wavetableToValue(data));
    m_submitting = false;
    m_nudging = true;
  }

  void commitNudge()
  {
    if(!m_nudging)
      return;
    m_nudging = false;
    if(m_model && m_ongoing.watching(m_model))
      m_ongoing.commit();
  }

  //! Applies `function` to the edited frame, quantized to the port's levels.
  template <typename Function>
  void editFrame(Function function)
  {
    if(!m_model)
      return;
    finish(true);
    commitNudge();
    auto data = m_model->wavetable();
    const int levels = m_model->amplitudeLevels();
    const std::span<float> frame{data.samples.data() + m_frame * data.frameSize, data.frameSize};
    function(frame, levels);
    for(float& sample : frame)
      sample = Process::quantizeSample(sample, levels);
    submit(data);
  }

  bool canAdd() const
  {
    const auto& table = m_model->wavetable();
    return table.frameCount() < Process::WavetableData::maxFrames
           && table.samples.size() <= Process::WavetableData::maxSamples - table.frameSize;
  }

  //! A silent frame, or a copy of `after`, inserted after it and chosen.
  void insertFrame(std::size_t after, bool duplicate)
  {
    if(!m_model || !m_model->supportsFrames() || !canAdd())
      return;
    finish(true);
    commitNudge();
    auto data = m_model->wavetable();
    const auto size = data.frameSize;
    std::vector<float> frame(size, quantize(0.));
    if(duplicate)
      std::copy_n(data.samples.begin() + after * size, size, frame.begin());
    data.samples.insert(data.samples.begin() + (after + 1) * size, frame.begin(), frame.end());
    submit(data);
    setFrame(after + 1);
  }

  void removeFrame()
  {
    if(!m_model || !m_model->supportsFrames() || m_model->wavetable().frameCount() < 2)
      return;
    finish(true);
    commitNudge();
    auto data = m_model->wavetable();
    const auto size = data.frameSize;
    data.samples.erase(data.samples.begin() + m_frame * size, data.samples.begin() + (m_frame + 1) * size);
    const auto selected = std::min(m_frame, data.frameCount() - 1);
    submit(data);
    setFrame(selected);
  }

  //! Moves frame `from` to index `to`, the frames between shifting over.
  void moveFrame(std::size_t from, std::size_t to)
  {
    if(!m_model || !m_model->supportsFrames())
      return;
    const auto count = m_model->wavetable().frameCount();
    if(from >= count || to >= count || from == to)
      return;
    finish(true);
    commitNudge();
    auto data = m_model->wavetable();
    const auto size = data.frameSize;
    const auto begin = data.samples.begin();
    if(from < to)
      std::rotate(begin + from * size, begin + (from + 1) * size, begin + (to + 1) * size);
    else
      std::rotate(begin + to * size, begin + from * size, begin + (from + 1) * size);
    submit(data);
    setFrame(to);
  }

  void resizeFrames(std::size_t size)
  {
    if(!m_model || m_model->frameSizeConstraint())
      return;
    finish(true);
    commitNudge();
    const auto& current = m_model->wavetable();
    if(size < 2 || size == current.frameSize || size > Process::WavetableData::maxFrameSize
       || size > Process::WavetableData::maxSamples / current.frameCount())
      return;
    submit(Process::resampleWavetable(current, size));
  }

  QPointer<WaveformInlet> m_model;
  CommandDispatcher<> m_dispatcher;
  OngoingCommandDispatcher& m_ongoing;
  QRectF m_rect{0., 0., 240., 120.};
  std::size_t m_frame{};
  std::optional<std::size_t> m_hover;
  //! The sample the keyboard edits: the hovered or last edited one.
  std::optional<std::size_t> m_cursor;
  std::pair<std::size_t, float> m_previous{};
  std::pair<std::size_t, float> m_lineStart{};
  std::vector<float> m_preview;
  std::optional<Process::WavetableData> m_execution;
  mutable std::vector<float> m_liveFrame;
  mutable bool m_liveStale{true};
  std::optional<StripDrag> m_strip;
  bool m_dragging{};
  bool m_line{};
  bool m_nudging{};
  bool m_submitting{};
};

class WaveformView final : public QGraphicsView
{
public:
  WaveformView(WaveformInlet& model, const score::DocumentContext& ctx, QWidget* parent)
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
  WaveformItem& item() const noexcept { return *m_item; }

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

//! The inspector's editor: the same item, a row with the shapes and the
//! context menu, and the frame as an editable list of numbers.
class WaveformPanel final : public QWidget
{
public:
  WaveformPanel(WaveformInlet& model, const score::DocumentContext& ctx, QWidget* parent)
      : QWidget{parent}
  {
    setObjectName(QStringLiteral("WaveformPanel"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* layout = new QVBoxLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    auto* view = new WaveformView{model, ctx, this};
    layout->addWidget(view);
    auto& item = view->item();

    auto* tools = new QHBoxLayout;
    tools->setSpacing(2);
    // Menu buttons, narrow enough for the inspector's value column.
    auto* shapes = new QToolButton{this};
    shapes->setObjectName(QStringLiteral("WaveformShape"));
    shapes->setText(tr("Shape"));
    shapes->setPopupMode(QToolButton::InstantPopup);
    auto* shapeMenu = new QMenu{shapes};
    for(auto [name, shape] : shapeNames())
      connect(shapeMenu->addAction(tr(name)), &QAction::triggered, this,
              [&item, shape] { item.applyShape(shape); });
    shapes->setMenu(shapeMenu);
    tools->addWidget(shapes);
    // The context menu's transforms, resampling and frames.
    auto* edit = new QToolButton{this};
    edit->setObjectName(QStringLiteral("WaveformEdit"));
    edit->setText(tr("Edit"));
    edit->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu{edit};
    edit->setMenu(menu);
    connect(menu, &QMenu::aboutToShow, this, [menu, &item] {
      qDeleteAll(menu->findChildren<QMenu*>(Qt::FindDirectChildrenOnly));
      menu->clear();
      item.fillContextMenu(*menu, false);
    });
    tools->addWidget(edit);
    tools->addStretch(1);
    layout->addLayout(tools);

    m_text = new QLineEdit{this};
    m_text->setObjectName(QStringLiteral("WaveformText"));
    m_text->setToolTip(tr("The frame as a list of numbers, from -1 to 1. Enter applies."));
    layout->addWidget(m_text);
    const auto refresh = [this, &item] {
      if(!m_text->hasFocus())
        m_text->setText(item.frameText());
      m_text->setCursorPosition(0);
    };
    refresh();
    item.frameChanged = refresh;
    connect(&model, &Process::ControlInlet::valueChanged, this, refresh);
    connect(m_text, &QLineEdit::returnPressed, this, [this, &item] {
      if(!item.pasteText(m_text->text()))
        m_text->setText(item.frameText());
      m_text->clearFocus();
    });
    connect(m_text, &QLineEdit::editingFinished, this, [this, &item] {
      if(!m_text->hasFocus())
        m_text->setText(item.frameText());
    });
  }

private:
  QLineEdit* m_text{};
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
          waveform.readout(), waveform.live()};
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
  return new Dataflow::WaveformPanel{const_cast<Dataflow::WaveformInlet&>(inlet), ctx, parent};
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
