#include "WaveformInlet.hpp"

#include <Process/Commands/SetControlValue.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/Skin.hpp>

#include <QComboBox>
#include <QDebug>
#include <QFocusEvent>
#include <QGraphicsObject>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPainter>
#include <QPointer>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Dataflow::WaveformInlet)

namespace Dataflow
{
namespace
{
Process::WavetableData resample(const Process::WavetableData& source, std::size_t size)
{
  Process::WavetableData result{size, std::vector<float>(source.frameCount() * size)};
  for(std::size_t frame = 0; frame < source.frameCount(); ++frame)
    for(std::size_t sample = 0; sample < size; ++sample)
      result.samples[frame * size + sample]
          = Process::sampleWavetable(source, frame, double(sample) / size);
  return result;
}
}

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
    setWaveform(resample(m_waveform, size));
    if(auto initial = Process::wavetableFromValue(init()); initial && initial->frameCount() == m_waveform.frameCount())
      setInit(Process::wavetableToValue(resample(*initial, size)));
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
//! Both scene and inspector own this same editor. Its selection never enters
//! the document selection stack, and every edit targets the real port path.
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
    createToolbar();
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
        update(drawingRect());
      }
    });
    connect(&model, &Process::Port::executionReset, this, [this] {
      m_execution.reset();
      update(drawingRect());
    });
    connect(&model, &Process::ControlInlet::valueChanged, this, [this] {
      if(!m_submitting)
        finish(false);
      m_frame = std::min(m_frame, m_model->wavetable().frameCount() - 1);
      updateControls();
      update();
    });
    connect(&model, &WaveformInlet::constraintsChanged, this, [this] {
      finish(false);
      updateControls();
      update();
    });
    connect(&model, &QObject::destroyed, this, [this] {
      m_dragging = false;
      m_preview.clear();
      m_execution.reset();
      m_model = nullptr;
      m_toolbar->setEnabled(false);
      update();
    });
    updateControls();
    update();
  }

  void setRect(const QRectF& rect)
  {
    finish(true);
    prepareGeometryChange();
    m_rect = rect.normalized();
    layoutToolbar();
    update();
  }

  QRectF boundingRect() const override { return m_rect; }
  QRectF drawingRect() const
  {
    return m_rect.adjusted(4., m_model && m_model->supportsFrames() ? 56. : 26., -4., -4.);
  }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
  {
    const auto& skin = score::Skin::instance();
    painter->fillRect(m_rect, skin.Dark.main.brush);
    if(!m_model)
      return;
    const auto& table = m_model->wavetable();
    const auto area = drawingRect();
    if(area.width() <= 0. || area.height() <= 0.)
      return;
    // Main views use DontSavePainterState; keep our drawing state local.
    painter->save();
    painter->setPen(skin.DarkGray.main.pen1);
    painter->drawRect(area);
    painter->drawLine(QPointF{area.left(), area.center().y()}, QPointF{area.right(), area.center().y()});
    // Keep the grid legible at high resolutions without changing the samples.
    const auto stride = std::max<std::size_t>(1, std::ceil(table.frameSize * 4. / area.width()));
    for(std::size_t i = 0; i < table.frameSize; i += stride)
    {
      const auto x = area.left() + area.width() * double(i) / table.frameSize;
      painter->drawLine(QPointF{x, area.top()}, QPointF{x, area.bottom()});
    }
    const int levels = m_model->amplitudeLevels();
    if(levels >= 2 && area.height() / (levels - 1) >= 4.)
      for(int i = 1; i < levels - 1; ++i)
      {
        const auto y = area.top() + area.height() * i / (levels - 1);
        painter->drawLine(QPointF{area.left(), y}, QPointF{area.right(), y});
      }
    painter->setClipRect(area, Qt::IntersectClip);
    painter->setPen(skin.Base4.main.pen2);
    const std::span<const float> frame = m_dragging
        ? std::span<const float>{m_preview} : table.frame(m_frame);
    auto point = [&](std::size_t i) {
      return QPointF{area.left() + area.width() * double(i) / frame.size(),
          area.top() + area.height() * (1. - quantize(frame[i % frame.size()])) * .5};
    };
    QPointF previous = point(0);
    for(std::size_t i = 1; i <= frame.size(); ++i)
    {
      const auto next = point(i);
      painter->drawLine(previous, next);
      previous = next;
    }
    if(area.width() / frame.size() >= 5.)
      for(std::size_t i = 0; i < frame.size(); ++i)
        painter->drawEllipse(point(i), 1.5, 1.5);
    if(m_execution && !m_dragging)
    {
      // What execution last received, drawn as received: the engine resamples
      // it to its own frame size.
      const auto received
          = m_execution->frame(std::min(m_frame, m_execution->frameCount() - 1));
      painter->setPen(QPen{skin.Base4.lighter180.brush, 1.});
      auto receivedPoint = [&](std::size_t i) {
        return QPointF{area.left() + area.width() * double(i) / received.size(),
            area.top() + area.height() * (1. - received[i % received.size()]) * .5};
      };
      QPointF last = receivedPoint(0);
      for(std::size_t i = 1; i <= received.size(); ++i)
      {
        const auto next = receivedPoint(i);
        painter->drawLine(last, next);
        last = next;
      }
    }
    painter->restore();
  }

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override
  {
    event->accept();
    if(!m_model || event->button() != Qt::LeftButton)
      return;
    setFocus(Qt::MouseFocusReason);
    finish(true);
    if(!drawingRect().contains(event->pos()))
      return;
    const auto frame = m_model->wavetable().frame(m_frame);
    m_preview.assign(frame.begin(), frame.end());
    m_dragging = true;
    m_previous = sampleAt(event->pos());
    paintSamples(m_previous);
  }

  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override
  {
    event->accept();
    if(m_dragging && m_model)
      paintSamples(sampleAt(event->pos()));
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
  {
    event->accept();
    if(m_dragging && m_model)
      paintSamples(sampleAt(event->pos()));
    finish(true);
  }

  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override
  {
    mousePressEvent(event);
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

  void createToolbar()
  {
    m_toolbar = new QWidget;
    m_toolbar->setObjectName(QStringLiteral("WaveformToolbar"));
    m_toolbar->setFont(score::Skin::instance().Medium8Pt);
    auto* rows = new QVBoxLayout{m_toolbar};
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(2);
    auto* top = new QHBoxLayout;
    top->setContentsMargins(0, 0, 0, 0);
    top->setSpacing(2);
    m_selector = new QComboBox{m_toolbar};
    m_selector->setObjectName(QStringLiteral("WaveformFrameSelector"));
    m_selector->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_selector->setMinimumContentsLength(7);
    m_selector->setFixedHeight(22);
    m_resolution = new QSpinBox{m_toolbar};
    m_resolution->setObjectName(QStringLiteral("WaveformResolution"));
    m_resolution->setSuffix(tr(" samples"));
    m_resolution->setToolTip(tr("Resample every frame to this size"));
    m_resolution->setKeyboardTracking(false);
    m_resolution->setFixedHeight(22);
    top->addWidget(m_selector, 1);
    top->addWidget(m_resolution, 1);
    rows->addLayout(top);
    m_bankTools = new QWidget{m_toolbar};
    m_bankTools->setFixedHeight(28);
    auto* buttons = new QHBoxLayout{m_bankTools};
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(2);
    auto button = [&](const char* name, const QString& text, const QString& tip, BankAction action) {
      auto* result = new QToolButton{m_bankTools};
      result->setObjectName(QString::fromLatin1(name));
      result->setText(text);
      result->setToolTip(tip);
      result->setFixedHeight(24);
      buttons->addWidget(result);
      connect(result, &QToolButton::clicked, this, [this, action] { editBank(action); });
      return result;
    };
    m_add = button("WaveformAddFrame", tr("+"), tr("Add empty frame"), BankAction::Add);
    m_duplicate = button("WaveformDuplicateFrame", tr("Dup"), tr("Duplicate frame"), BankAction::Duplicate);
    m_remove = button("WaveformRemoveFrame", tr("-"), tr("Remove frame"), BankAction::Remove);
    m_earlier = button("WaveformMoveFrameLeft", tr("<"), tr("Move frame earlier"), BankAction::Earlier);
    m_later = button("WaveformMoveFrameRight", tr(">"), tr("Move frame later"), BankAction::Later);
    buttons->addStretch();
    rows->addWidget(m_bankTools);
    m_proxy = new QGraphicsProxyWidget{this};
    m_proxy->setWidget(m_toolbar);
    connect(m_selector, &QComboBox::currentIndexChanged, this, [this](int index) {
      if(!m_model || index < 0 || std::size_t(index) >= m_model->wavetable().frameCount())
        return;
      finish(true);
      m_frame = index;
      updateControls();
      update();
    });
    connect(m_resolution, &QSpinBox::editingFinished, this, [this] { resizeFrames(); });
  }

  void layoutToolbar()
  {
    if(!m_model)
      return;
    const qreal height = m_model->supportsFrames() ? 52. : 22.;
    m_proxy->setGeometry({m_rect.topLeft(), QSizeF{m_rect.width(), height}});
  }

  void updateControls()
  {
    if(!m_model)
      return;
    m_toolbar->setEnabled(true);
    const auto& table = m_model->wavetable();
    const QSignalBlocker selectorBlocker{m_selector};
    const QSignalBlocker resolutionBlocker{m_resolution};
    if(m_selector->count() != int(table.frameCount()))
    {
      m_selector->clear();
      for(std::size_t i = 0; i < table.frameCount(); ++i)
        m_selector->addItem(tr("Frame %1").arg(i + 1));
    }
    m_selector->setCurrentIndex(int(m_frame));
    m_selector->setVisible(m_model->supportsFrames());
    m_resolution->setRange(2, int(std::min(Process::WavetableData::maxFrameSize,
        Process::WavetableData::maxSamples / table.frameCount())));
    m_resolution->setValue(int(table.frameSize));
    m_resolution->setEnabled(!m_model->frameSizeConstraint());
    m_bankTools->setVisible(m_model->supportsFrames());
    const bool canAdd = table.frameCount() < Process::WavetableData::maxFrames
        && table.samples.size() <= Process::WavetableData::maxSamples - table.frameSize;
    m_add->setEnabled(canAdd);
    m_duplicate->setEnabled(canAdd);
    m_remove->setEnabled(table.frameCount() > 1);
    m_earlier->setEnabled(m_frame > 0);
    m_later->setEnabled(m_frame + 1 < table.frameCount());
    layoutToolbar();
  }

  float quantize(double value) const
  {
    value = std::clamp(value, -1., 1.);
    const int levels = m_model->amplitudeLevels();
    if(levels >= 2)
      value = 2. * std::round((value + 1.) * .5 * (levels - 1)) / (levels - 1) - 1.;
    return float(value);
  }

  std::pair<std::size_t, float> sampleAt(QPointF point) const
  {
    const auto area = drawingRect();
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
    update(drawingRect());
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
    update(drawingRect());
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
      std::vector<float> frame(size, 0.f);
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
    updateControls();
    update();
  }

  void resizeFrames()
  {
    if(!m_model || m_model->frameSizeConstraint())
      return;
    const int size = m_resolution->value();
    finish(true);
    const auto& current = m_model->wavetable();
    if(std::size_t(size) == current.frameSize
       || std::size_t(size) > Process::WavetableData::maxSamples / current.frameCount())
      return;
    submit(resample(current, size));
  }

  QPointer<WaveformInlet> m_model;
  CommandDispatcher<> m_dispatcher;
  QRectF m_rect{0., 0., 240., 120.};
  std::size_t m_frame{};
  std::pair<std::size_t, float> m_previous{};
  std::vector<float> m_preview;
  std::optional<Process::WavetableData> m_execution;
  bool m_dragging{};
  bool m_submitting{};
  QWidget* m_toolbar{};
  QWidget* m_bankTools{};
  QGraphicsProxyWidget* m_proxy{};
  QComboBox* m_selector{};
  QSpinBox* m_resolution{};
  QToolButton* m_add{};
  QToolButton* m_duplicate{};
  QToolButton* m_remove{};
  QToolButton* m_earlier{};
  QToolButton* m_later{};
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
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setMinimumSize(220, 120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setRenderHint(QPainter::Antialiasing);
  }
  QSize sizeHint() const override { return {320, 180}; }

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
}

void setWaveformItemModel(QGraphicsItem& item, WaveformInlet& model)
{
  auto* waveform = dynamic_cast<WaveformItem*>(&item);
  SCORE_ASSERT(waveform);
  waveform->setModel(model);
}

void setWaveformItemRect(QGraphicsItem& item, const QRectF& rect)
{
  auto* waveform = dynamic_cast<WaveformItem*>(&item);
  SCORE_ASSERT(waveform);
  waveform->setRect(rect);
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
