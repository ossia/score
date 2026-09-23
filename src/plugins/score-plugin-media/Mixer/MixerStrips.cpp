#include "MixerStrips.hpp"

#include <Process/Commands/EditPort.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortAddressComboBox.hpp>
#include <Process/Process.hpp>

#include <Scenario/Commands/Interval/MakeBus.hpp>
#include <Scenario/Commands/Interval/SetMuteSolo.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Execution/DocumentPlugin.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/Skin.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/HelpInteraction.hpp>

#include <ossia/audio/audio_parameter.hpp>
#include <ossia/dataflow/telemetry.hpp>
#include <ossia/network/base/node.hpp>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

namespace Mixer
{
namespace
{
constexpr double floor_db = -96.;

QString dbText(double gain)
{
  if(gain <= 0.)
    return QStringLiteral("-inf dB");
  return QString::number(20. * std::log10(gain), 'f', 1) + QStringLiteral(" dB");
}

// A toggle whose checked state stands out in its own colour.
QToolButton* makeToggle(
    const QString& text, const QString& help, const QColor& on, QWidget* parent)
{
  auto b = new QToolButton{parent};
  b->setText(text);
  b->setCheckable(true);
  b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  b->setStyleSheet(
      QStringLiteral(
          "QToolButton { border: 1px solid %2; border-radius: 2px; padding: 1px; }"
          "QToolButton:checked { background-color: %1; border-color: %1; color: black; }")
          .arg(on.name(), score::Skin::instance().Gray.color().name()));
  score::setHelp(b, help);
  return b;
}
}

GainFader::GainFader(QWidget* parent)
    : score::DoubleSlider{Qt::Vertical, parent}
{
  setRange(floor_db, 0., 0.);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  setMinimumHeight(60);
  score::setHelp(
      this, QObject::tr("Gain. Double-click for 0 dB, right-click to type a value."));
}

double GainFader::positionToGain(double p) noexcept
{
  p = std::clamp(p, 0., 1.);
  return p * p * p;
}

double GainFader::gainToPosition(double g) noexcept
{
  return std::cbrt(std::clamp(g, 0., 1.));
}

double GainFader::map(double position) const
{
  if(position <= 0.)
    return floor_db;
  return std::max(floor_db, 60. * std::log10(position));
}

double GainFader::unmap(double db) const
{
  if(db <= floor_db)
    return 0.;
  return std::pow(10., db / 60.);
}

void GainFader::paintEvent(QPaintEvent*)
{
  auto& skin = score::Skin::instance();
  QPainter p{this};

  // A thin track, lit below a cap that sits at the value.
  constexpr double cap_h = 8.;
  const double w = width();
  const double usable = std::max(1., height() - cap_h);
  const double y = (1. - value()) * usable;
  const double cx = w / 2.;

  p.fillRect(
      QRectF{cx - 1.5, cap_h / 2., 3., usable}, skin.Background1.color().darker(260));
  p.fillRect(QRectF{cx - 1.5, y + cap_h / 2., 3., usable - y}, skin.SliderInteriorBrush);

  p.setPen(skin.Dark.color());
  p.setBrush(skin.SliderInteriorBrush);
  p.drawRect(QRectF{0.5, y + 0.5, w - 1., cap_h - 1.});
  p.setPen(skin.Light.color());
  p.drawLine(QPointF{2., y + cap_h / 2.}, QPointF{w - 2., y + cap_h / 2.});
}

PanSlider::PanSlider(QWidget* parent)
    : score::DoubleSlider{Qt::Horizontal, parent}
{
  setRange(-100., 100., 0.);
  score::setHelp(
      this, QObject::tr("Balance between the first two channels. "
                        "Double-click to centre."));
}

std::pair<double, double> PanSlider::weights(double position) noexcept
{
  position = std::clamp(position, 0., 1.);
  return {std::min(1., 2. * (1. - position)), std::min(1., 2. * position)};
}

double PanSlider::position(double left, double right) noexcept
{
  if(left <= 0. && right <= 0.)
    return 0.5;
  if(left >= right)
    return 0.5 * right / left;
  return 1. - 0.5 * left / right;
}

void PanSlider::paintEvent(QPaintEvent*)
{
  auto& skin = score::Skin::instance();
  QPainter p{this};

  const double ratio = 2. * value() - 1.;
  p.setPen(skin.TransparentPen);
  p.setBrush(skin.SliderBrush);
  p.drawRect(rect());

  const double hw = width() / 2.;
  const double w = hw * std::abs(ratio);
  p.setBrush(skin.SliderInteriorBrush);
  if(ratio < 0)
    p.drawRect(QRectF{hw - w, 0., w, double(height())});
  else if(ratio > 0)
    p.drawRect(QRectF{hw, 0., w, double(height())});

  p.setPen(skin.SliderLine);
  p.drawLine(QPointF{hw, 0.}, QPointF{hw, double(height())});

  p.setFont(skin.SansFontSmall);
  p.setPen(ratio < 0 ? skin.SliderLine : skin.LightGray.main.pen0);
  p.drawText(rect().adjusted(3, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, "L");
  p.setPen(ratio > 0 ? skin.SliderLine : skin.LightGray.main.pen0);
  p.drawText(rect().adjusted(0, 0, -3, 0), Qt::AlignRight | Qt::AlignVCenter, "R");
}

Strip::Strip(const score::DocumentContext& ctx, QWidget* parent)
    : QWidget{parent}
    , m_context{ctx}
    , m_telemetry{&ctx.plugin<Execution::DocumentPlugin>().telemetry()}
{
  m_layout = new QVBoxLayout{this};
  m_layout->setContentsMargins(3, 3, 3, 3);
  m_layout->setSpacing(3);

  m_title = new QPushButton{this};
  m_title->setFlat(true);
  m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  m_layout->addWidget(m_title);

  m_badge = new QLabel{this};
  m_badge->setAlignment(Qt::AlignCenter);
  m_badge->setFont(score::Skin::instance().SansFontSmall);
  score::setHelp(m_badge, tr("Channels the signal carries."));
  m_layout->addWidget(m_badge);

  auto row = new QHBoxLayout;
  row->setContentsMargins(0, 0, 0, 0);
  row->setSpacing(2);
  m_meter = new score::LevelMeter{this};
  m_meter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_fader = new GainFader{this};
  row->addWidget(m_meter, 1);
  row->addWidget(m_fader);
  m_layout->addLayout(row, 1);

  m_readout = new QLabel{this};
  m_readout->setAlignment(Qt::AlignCenter);
  m_readout->setFont(score::Skin::instance().SansFontSmall);
  m_layout->addWidget(m_readout);

  m_buttons = new QWidget{this};
  auto buttons = new QHBoxLayout{m_buttons};
  buttons->setContentsMargins(0, 0, 0, 0);
  buttons->setSpacing(1);
  m_layout->addWidget(m_buttons);

  m_meter->hide();
  m_badge->hide();
  setStripWidth(StripWidth::Normal);
}

Strip::~Strip()
{
  if(m_telemetry && m_meterHandle)
    m_telemetry->release(m_meterHandle);
}

void Strip::setStripWidth(StripWidth w)
{
  m_width = w;
  switch(w)
  {
    case StripWidth::Narrow:
      setFixedWidth(52);
      break;
    case StripWidth::Normal:
      setFixedWidth(84);
      break;
    case StripWidth::Wide:
      setFixedWidth(140);
      break;
  }
  m_meter->setScaleVisible(w != StripWidth::Narrow);
  m_readout->setVisible(w != StripWidth::Narrow);
}

void Strip::setMeter(Execution::Telemetry::Meter m, std::vector<int> channels)
{
  if(m_telemetry && m_meterHandle)
    m_telemetry->release(m_meterHandle);
  m_meterHandle = m;
  m_channels = std::move(channels);
  m_meter->setVisible(bool(m));
  // A single channel needs no count.
  m_badge->setVisible(bool(m) && m_channels.size() != 1);
  m_badge->setText(tr("off"));
}

void Strip::updateMeter(const Execution::Telemetry& t)
{
  if(!m_meterHandle)
    return;

  const auto* levels = t.levels(m_meterHandle);
  if(!levels)
  {
    m_meter->setInactive();
    m_badge->setText(tr("off"));
    return;
  }

  const int total = int(levels->channels);
  m_levels.clear();
  auto add = [&](int src) {
    if(src >= 0 && src < total)
      m_levels.push_back({levels->peak[src], levels->rms(src), levels->is_clipped(src)});
    else
      m_levels.push_back({});
  };
  if(m_channels.empty())
    for(int c = 0; c < total; c++)
      add(c);
  else
    for(int c : m_channels)
      add(c);

  m_meter->setLevels(m_levels);
  const int shown = int(m_levels.size());
  m_badge->setText(shown == 0 ? tr("silent") : tr("%n ch", nullptr, shown));
}

void Strip::setGainReadout(double gain)
{
  m_readout->setText(dbText(gain));
}

void Strip::setHighlighted(bool b)
{
  if(b != m_highlighted)
  {
    m_highlighted = b;
    update();
  }
}

void Strip::paintEvent(QPaintEvent*)
{
  if(!m_highlighted)
    return;
  QPainter p{this};
  p.setPen(QPen{palette().highlight().color(), 2.});
  p.setBrush(Qt::NoBrush);
  p.drawRect(rect().adjusted(1, 1, -1, -1));
}

void Strip::contextMenuEvent(QContextMenuEvent* e)
{
  QMenu menu;
  auto widths = menu.addMenu(tr("Width"));
  auto group = new QActionGroup{&menu};
  for(auto [w, name] :
      {std::pair{StripWidth::Narrow, tr("Narrow")},
       std::pair{StripWidth::Normal, tr("Normal")},
       std::pair{StripWidth::Wide, tr("Wide")}})
  {
    auto act = widths->addAction(name);
    act->setCheckable(true);
    act->setChecked(m_width == w);
    group->addAction(act);
    connect(act, &QAction::triggered, this, [this, w = w] { setStripWidth(w); });
  }
  if(m_meter->isVisible())
  {
    auto clear = menu.addAction(tr("Clear clip marks"));
    connect(clear, &QAction::triggered, m_meter, &score::LevelMeter::resetClips);
  }
  fillContextMenu(menu);
  menu.exec(e->globalPos());
}

BusStrip::BusStrip(
    const Scenario::IntervalModel& itv, const score::DocumentContext& ctx,
    QWidget* parent)
    : Strip{ctx, parent}
    , m_model{itv}
{
  auto& outlet = *itv.outlet;

  connect(m_title, &QPushButton::clicked, this, [this] {
    score::SelectionDispatcher{m_context.selectionStack}.select(m_model);
  });

  if(m_telemetry)
    setMeter(m_telemetry->meterOutlet(outlet));

  auto buttons = static_cast<QHBoxLayout*>(m_buttons->layout());
  m_mute = makeToggle("M", tr("Mute"), QColor{230, 120, 50}, m_buttons);
  m_solo = makeToggle(
      "S", tr("Solo: silence the buses that neither contain this one nor are in it"),
      QColor{226, 196, 64}, m_buttons);
  m_propagate = makeToggle(
      "P",
      tr("Propagate: send the sound to the parent interval rather than to the "
         "output address"),
      QColor{90, 160, 220}, m_buttons);
  buttons->addWidget(m_mute);
  buttons->addWidget(m_solo);
  buttons->addWidget(m_propagate);

  m_pan = new PanSlider{this};
  m_layout->addWidget(m_pan);

  auto combo = Process::makePortAddressCombo(outlet, ctx, this);
  combo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  m_layout->addWidget(combo);

  connect(m_fader, &GainFader::valueChanged, this, [this](double p) {
    const double g = GainFader::positionToGain(p);
    m_context.dispatcher.submit<Process::SetGain>(*m_model.outlet, g);
    setGainReadout(g);
  });
  connect(m_fader, &GainFader::sliderReleased, this, [this] {
    m_context.dispatcher.commit();
  });
  connect(m_pan, &PanSlider::valueChanged, this, [this](double p) {
    auto [l, r] = PanSlider::weights(p);
    m_context.dispatcher.submit<Process::SetPan>(
        *m_model.outlet, Process::pan_weight{{l, r}});
  });
  connect(m_pan, &PanSlider::sliderReleased, this, [this] {
    m_context.dispatcher.commit();
  });
  connect(m_mute, &QToolButton::toggled, this, [this](bool b) {
    CommandDispatcher<>{m_context.commandStack}
        .submit<Scenario::Command::SetIntervalMuted>(m_model, b);
  });
  connect(m_solo, &QToolButton::toggled, this, [this](bool b) {
    CommandDispatcher<>{m_context.commandStack}
        .submit<Scenario::Command::SetIntervalSoloed>(m_model, b);
  });
  connect(m_propagate, &QToolButton::toggled, this, [this](bool b) {
    CommandDispatcher<>{m_context.commandStack}.submit<Process::SetPropagate>(
        *m_model.outlet, b);
  });

  auto sync = [this] { syncFromModel(); };
  con(outlet, &Process::AudioOutlet::gainChanged, this, sync);
  con(outlet, &Process::AudioOutlet::panChanged, this, sync);
  con(outlet, &Process::AudioOutlet::propagateChanged, this, sync);
  con(itv, &Scenario::IntervalModel::mutedChanged, this, sync);
  con(itv, &Scenario::IntervalModel::soloedChanged, this, sync);
  con(itv, &Scenario::IntervalModel::soloMutedChanged, this, sync);
  con(itv.metadata(), &score::ModelMetadata::NameChanged, this, sync);
  con(itv.metadata(), &score::ModelMetadata::ColorChanged, this, sync);

  syncFromModel();
}

BusStrip::~BusStrip() = default;

void BusStrip::syncFromModel()
{
  auto& outlet = *m_model.outlet;
  {
    QSignalBlocker b{m_fader};
    m_fader->setValue(GainFader::gainToPosition(outlet.gain()));
  }
  setGainReadout(outlet.gain());
  {
    QSignalBlocker b{m_pan};
    const auto& pan = outlet.pan();
    m_pan->setValue(
        pan.size() >= 2 ? PanSlider::position(pan[0], pan[1]) : 0.5);
  }
  for(auto [button, state] :
      {std::pair{m_mute, m_model.muted()}, std::pair{m_solo, m_model.soloed()},
       std::pair{m_propagate, outlet.propagate()}})
  {
    QSignalBlocker b{button};
    button->setChecked(state);
  }

  const auto& name = m_model.metadata().getName();
  m_title->setText(name);
  const QColor color = m_model.metadata().getColor().getBrush().color();
  m_title->setStyleSheet(
      QStringLiteral("QPushButton { border-left: 4px solid %1; text-align: left; "
                     "padding-left: 3px; }")
          .arg(color.name()));

  QFont f = m_title->font();
  f.setItalic(m_model.soloMuted());
  m_title->setFont(f);
  m_title->setToolTip(
      m_model.soloMuted() ? tr("%1\nSilenced: another bus is soloed").arg(name)
                          : name);
}

void BusStrip::updateMeter(const Execution::Telemetry& t)
{
  Strip::updateMeter(t);

  double load = -1.;
  for(auto proc : m_model.findChildren<Process::ProcessModel*>())
    if(const double l = t.cpuLoad(*proc); l >= 0.)
      load = std::max(load, 0.) + l;

  if(load >= 0.)
  {
    m_badge->setText(
        m_badge->text() + QStringLiteral(" · ") + QString::number(100. * load, 'f', 1)
        + QStringLiteral("%"));
    m_badge->setToolTip(tr("Channels the bus carries, and the share of the real time "
                           "its processes take to run."));
  }
}

void BusStrip::fillContextMenu(QMenu& menu)
{
  auto remove = menu.addAction(tr("Remove from the buses"));
  connect(remove, &QAction::triggered, this, [this] {
    auto& doc = m_context.model<Scenario::ScenarioDocumentModel>();
    CommandDispatcher<>{m_context.commandStack}.submit<Scenario::Command::SetBus>(
        doc, m_model, false);
  });
}

PortStrip::PortStrip(
    ossia::audio_parameter& param, Meter meter, std::vector<int> channels,
    const score::DocumentContext& ctx, QWidget* parent)
    : Strip{ctx, parent}
    , m_param{&param}
{
  const auto address = QString::fromStdString(param.get_node().osc_address());
  m_title->setText(address);
  m_title->setToolTip(tr("audio:%1").arg(address));
  m_buttons->hide();

  if(m_telemetry)
  {
    switch(meter)
    {
      case Meter::HardwareInputs:
        setMeter(m_telemetry->meterHardwareInputs(), std::move(channels));
        break;
      case Meter::HardwareOutputs:
        setMeter(m_telemetry->meterHardwareOutputs(), std::move(channels));
        break;
      case Meter::None:
        break;
    }
  }

  connect(m_fader, &GainFader::valueChanged, this, [this](double p) {
    const double g = GainFader::positionToGain(p);
    if(m_param)
      m_param->push_value(float(g));
    setGainReadout(g);
  });

  connect(m_fader, &GainFader::sliderMoved, this, [this] { m_dragging = true; });
  connect(m_fader, &GainFader::sliderReleased, this, [this] { m_dragging = false; });

  param.get_node().about_to_be_deleted.connect<&PortStrip::onNodeRemoved>(*this);
  poll();
}

PortStrip::~PortStrip() = default;

void PortStrip::poll()
{
  if(!m_param || m_dragging)
    return;
  const double g = m_param->gain();
  QSignalBlocker b{m_fader};
  m_fader->setValue(GainFader::gainToPosition(g));
  setGainReadout(g);
}

void PortStrip::onNodeRemoved(const ossia::net::node_base&)
{
  m_param = nullptr;
  setEnabled(false);
}
}
