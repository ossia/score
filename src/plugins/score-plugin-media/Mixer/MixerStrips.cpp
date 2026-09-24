#include "MixerStrips.hpp"

#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Explorer/Commands/Add/AddAddress.hpp>
#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/Commands/Remove.hpp>
#include <Explorer/Commands/Update/UpdateAddressSettings.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Process/Commands/EditPort.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortAddressComboBox.hpp>
#include <Process/Process.hpp>

#include <Scenario/Commands/Interval/MakeBus.hpp>
#include <Scenario/Commands/Interval/SetMuteSolo.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Audio/AudioDevice.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
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

Device::Node*
explorerNode(Explorer::DeviceDocumentPlugin& plug, const Device::DeviceInterface& dev)
{
  for(auto& n : plug.rootNode())
    if(n.is<Device::DeviceSettings>()
       && n.get<Device::DeviceSettings>().name == dev.settings().name)
      return &n;
  return nullptr;
}

Device::ProtocolFactory*
protocolOf(const score::DocumentContext& ctx, const Device::DeviceInterface& dev)
{
  return ctx.app.interfaces<Device::ProtocolFactoryList>().get(dev.settings().protocol);
}

QString dbText(double gain)
{
  if(gain <= 0.)
    return QStringLiteral("-inf dB");
  return QString::number(20. * std::log10(gain), 'f', 1) + QStringLiteral(" dB");
}

// Every row keeps its place when empty, so that strips line up.
void keepPlace(QWidget* w)
{
  auto sp = w->sizePolicy();
  sp.setRetainSizeWhenHidden(true);
  w->setSizePolicy(sp);
}

// Height of the controls under the meters of buses.
constexpr int toggle_h = 20;
constexpr int pan_h = 14;
constexpr int route_h = 22;
constexpr int row_gap = 3;
constexpr int controls_h = toggle_h + row_gap + pan_h + row_gap + route_h;

// A toggle whose checked state stands out in its own colour.
QToolButton* makeToggle(
    const QString& text, const QString& help, const QColor& on, QWidget* parent)
{
  auto b = new QToolButton{parent};
  b->setText(text);
  b->setCheckable(true);
  b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  b->setFixedHeight(toggle_h);
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
  const double w = width();
  const double usable = std::max(1., double(height() - cap_h));
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
      this, QObject::tr("Balance between the first two channels. A mono bus is "
                        "panned once upmixed to two channels (right-click the "
                        "strip). Double-click to centre."));
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
  auto& skin = score::Skin::instance();
  m_layout = new QVBoxLayout{this};
  m_layout->setContentsMargins(3, 3, 3, 3);
  m_layout->setSpacing(row_gap);

  m_title = new QPushButton{this};
  m_title->setFlat(true);
  m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  m_title->setFixedHeight(m_title->fontMetrics().height() + 6);
  setTitleColor(Qt::transparent);
  m_layout->addWidget(m_title);

  m_badge = new QLabel{this};
  m_badge->setAlignment(Qt::AlignCenter);
  m_badge->setFont(skin.SansFontSmall);
  m_badge->setFixedHeight(m_badge->fontMetrics().height());
  keepPlace(m_badge);
  score::setHelp(m_badge, tr("Channels the signal carries."));
  m_layout->addWidget(m_badge);

  // The meter spans the travel of the fader's cap, so that both have the
  // same top and bottom.
  auto row = new QHBoxLayout;
  row->setContentsMargins(0, 0, 0, 0);
  row->setSpacing(2);
  auto meter_col = new QVBoxLayout;
  meter_col->setContentsMargins(0, GainFader::cap_h / 2, 0, GainFader::cap_h / 2);
  m_meter = new score::LevelMeter{this};
  m_meter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  keepPlace(m_meter);
  meter_col->addWidget(m_meter);
  m_fader = new GainFader{this};
  row->addLayout(meter_col, 1);
  row->addWidget(m_fader);
  m_layout->addLayout(row, 1);

  m_readout = new QLabel{this};
  m_readout->setAlignment(Qt::AlignCenter);
  m_readout->setFont(skin.SansFontSmall);
  m_readout->setFixedHeight(m_readout->fontMetrics().height());
  keepPlace(m_readout);
  m_layout->addWidget(m_readout);

  m_controls = new QWidget{this};
  m_controls->setFixedHeight(controls_h);
  auto controls = new QVBoxLayout{m_controls};
  controls->setContentsMargins(0, 0, 0, 0);
  controls->setSpacing(row_gap);
  m_buttons = new QWidget{m_controls};
  auto buttons = new QHBoxLayout{m_buttons};
  buttons->setContentsMargins(0, 0, 0, 0);
  buttons->setSpacing(1);
  controls->addWidget(m_buttons);
  controls->addStretch(1);
  m_layout->addWidget(m_controls);

  m_meter->hide();
  m_badge->hide();
  setStripWidth(StripWidth::Normal);
}

void Strip::setTitle(const QString& t)
{
  m_titleText = t;
  elideTitle();
}

void Strip::elideTitle()
{
  // The left border and the padding take 8 pixels.
  m_title->setText(m_title->fontMetrics().elidedText(
      m_titleText, Qt::ElideRight, std::max(0, m_title->width() - 8)));
}

void Strip::resizeEvent(QResizeEvent* e)
{
  QWidget::resizeEvent(e);
  elideTitle();
}

void Strip::setTitleColor(const QColor& c)
{
  // The same box for every strip; a bus shows its interval's colour on the left.
  m_title->setStyleSheet(
      QStringLiteral("QPushButton { border: none; border-left: 4px solid %1; "
                     "padding: 0px 4px 0px 0px; text-align: center; }")
          .arg(c.alpha() == 0 ? QStringLiteral("transparent") : c.name()));
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
  const float scale = float(meterGain());
  m_levels.clear();
  auto add = [&](int src) {
    if(src >= 0 && src < total)
      m_levels.push_back(
          {levels->peak[src] * scale, levels->rms(src) * scale,
           levels->is_clipped(src)});
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

  auto controls = static_cast<QVBoxLayout*>(m_controls->layout());
  m_pan = new PanSlider{m_controls};
  m_pan->setFixedHeight(pan_h);
  controls->insertWidget(1, m_pan);

  auto combo = Process::makePortAddressCombo(outlet, ctx, m_controls);
  combo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  combo->setFixedHeight(route_h);
  controls->insertWidget(2, combo);

  connect(m_fader, &GainFader::valueChanged, this, [this](double p) {
    const double g = GainFader::positionToGain(p);
    m_context.dispatcher.submit<Process::SetGain>(*m_model.outlet, g);
    setGainReadout(g);
  });
  connect(m_fader, &GainFader::sliderReleased, this, [this] {
    m_context.dispatcher.commit();
  });
  connect(m_pan, &PanSlider::valueChanged, this, [this](double p) {
    // The balance only moves the first two weights: the others are the
    // channel gains of a wider bus.
    auto [l, r] = PanSlider::weights(p);
    auto w = m_model.outlet->pan();
    while(w.size() < 2)
      w.push_back(1.);
    w[0] = l;
    w[1] = r;
    m_context.dispatcher.submit<Process::SetPan>(*m_model.outlet, w);
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

  // Each change updates only what shows it: a fader drag changes the gain at
  // every move.
  con(outlet, &Process::AudioOutlet::gainChanged, this, [this] { syncGain(); });
  con(outlet, &Process::AudioOutlet::panChanged, this, [this] { syncPan(); });
  auto sync_buttons = [this] { syncButtons(); };
  con(outlet, &Process::AudioOutlet::propagateChanged, this, sync_buttons);
  con(itv, &Scenario::IntervalModel::mutedChanged, this, sync_buttons);
  con(itv, &Scenario::IntervalModel::soloedChanged, this, sync_buttons);
  auto title = [this] { syncTitle(); };
  con(itv, &Scenario::IntervalModel::soloMutedChanged, this, title);
  con(itv.metadata(), &score::ModelMetadata::NameChanged, this, title);
  con(itv.metadata(), &score::ModelMetadata::ColorChanged, this, title);

  syncFromModel();
}

BusStrip::~BusStrip() = default;

void BusStrip::syncFromModel()
{
  syncGain();
  syncPan();
  syncButtons();
  syncTitle();
}

void BusStrip::syncGain()
{
  const double g = m_model.outlet->gain();
  {
    QSignalBlocker b{m_fader};
    m_fader->setValue(GainFader::gainToPosition(g));
  }
  setGainReadout(g);
}

void BusStrip::syncPan()
{
  QSignalBlocker b{m_pan};
  const auto& pan = m_model.outlet->pan();
  m_pan->setValue(pan.size() >= 2 ? PanSlider::position(pan[0], pan[1]) : 0.5);
}

void BusStrip::syncButtons()
{
  for(auto [button, state] :
      {std::pair{m_mute, m_model.muted()}, std::pair{m_solo, m_model.soloed()},
       std::pair{m_propagate, m_model.outlet->propagate()}})
  {
    QSignalBlocker b{button};
    button->setChecked(state);
  }
}

void BusStrip::syncTitle()
{
  const auto& name = m_model.metadata().getName();
  setTitle(name);
  setTitleColor(m_model.metadata().getColor().getBrush().color());

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

  // Every process under the bus, looked up again now and then rather than at
  // each update: a whole score may be under it.
  if(!m_processesAge.isValid() || m_processesAge.elapsed() > 1000)
  {
    m_processes.clear();
    for(auto proc : m_model.findChildren<Process::ProcessModel*>())
      m_processes.emplace_back(proc);
    m_processesAge.start();
  }

  double load = -1.;
  for(const auto& proc : m_processes)
    if(proc)
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

namespace
{
//! One fader per channel of a bus, on its pan weights.
class ChannelGains final : public QWidget
{
public:
  ChannelGains(
      const Process::AudioOutlet& outlet, int channels,
      const score::DocumentContext& ctx)
      : QWidget{nullptr, Qt::Popup}
      , m_outlet{outlet}
      , m_context{ctx}
  {
    setAttribute(Qt::WA_DeleteOnClose);
    setAutoFillBackground(true);
    auto pal = palette();
    pal.setColor(QPalette::Window, score::Skin::instance().Background2.color());
    setPalette(pal);
    auto lay = new QHBoxLayout{this};
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(2);

    const auto& pan = outlet.pan();
    for(int c = 0; c < channels; c++)
    {
      auto col = new QVBoxLayout;
      auto fader = new GainFader{this};
      fader->setFixedHeight(120);
      {
        QSignalBlocker b{fader};
        fader->setValue(
            GainFader::gainToPosition(c < int(pan.size()) ? pan[c] : 1.));
      }
      connect(fader, &GainFader::valueChanged, this, [this, c](double p) {
        auto w = m_outlet.pan();
        while(int(w.size()) <= c)
          w.push_back(1.);
        w[c] = GainFader::positionToGain(p);
        m_context.dispatcher.submit<Process::SetPan>(m_outlet, w);
      });
      connect(fader, &GainFader::sliderReleased, this, [this] {
        m_context.dispatcher.commit();
      });
      auto label = new QLabel{QString::number(c + 1), this};
      label->setAlignment(Qt::AlignCenter);
      label->setFont(score::Skin::instance().SansFontSmall);
      col->addWidget(fader, 0, Qt::AlignHCenter);
      col->addWidget(label);
      lay->addLayout(col);
    }
  }

private:
  const Process::AudioOutlet& m_outlet;
  const score::DocumentContext& m_context;
};
}

void BusStrip::fillContextMenu(QMenu& menu)
{
  auto gains = menu.addAction(tr("Channel gains..."));
  connect(gains, &QAction::triggered, this, [this] {
    const int channels = std::max(
        {m_meter->channelCount(), int(m_model.outlet->pan().size()), 2});
    auto popup = new ChannelGains{*m_model.outlet, channels, m_context};
    popup->move(mapToGlobal(rect().topRight()));
    popup->show();
  });

  // How a narrower signal is widened before the gain and pan.
  auto& outlet = *m_model.outlet;
  auto upmix = menu.addMenu(tr("Upmix"));
  auto set_upmix = [this](int mode, int channels) {
    auto& out = *m_model.outlet;
    MacroCommandDispatcher<Process::SetUpmix> disp{m_context.commandStack};
    disp.submit(new Process::SetUpmixMode{out, mode});
    disp.submit(new Process::SetUpmixChannels{out, channels});
    disp.commit();
  };
  {
    auto off = upmix->addAction(tr("Off"));
    off->setCheckable(true);
    off->setChecked(outlet.upmixMode() == 0);
    connect(off, &QAction::triggered, this, [set_upmix] { set_upmix(0, 0); });
  }
  for(auto [mode, title] :
      {std::pair{1, tr("Repeat the channels up to")},
       std::pair{2, tr("Add silent channels up to")}})
  {
    auto sub = upmix->addMenu(title);
    for(int n : {2, 4, 6, 8, 16, 32, 64})
    {
      auto act = sub->addAction(tr("%n channels", nullptr, n));
      act->setCheckable(true);
      act->setChecked(outlet.upmixMode() == mode && outlet.upmixChannels() == n);
      connect(act, &QAction::triggered, this, [set_upmix, mode = mode, n] {
        set_upmix(mode, n);
      });
    }
  }

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
  setTitle(address);
  m_title->setToolTip(tr("audio:%1").arg(address));
  // No controls: the meter and the fader take their room.
  m_controls->hide();

  m_meterKind = meter;
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
      case Meter::Virtual:
        if(auto v = dynamic_cast<ossia::virtual_audio_parameter*>(&param))
          setMeter(m_telemetry->meterVirtualPort(*v));
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

double PortStrip::meterGain() const noexcept
{
  // The inputs are metered as the driver gives them: what the graph reads is
  // scaled by the port's gain, and by /in/main's.
  if(m_meterKind != Meter::HardwareInputs || !m_param)
    return 1.;
  double g = m_param->gain();
  if(m_param->upstream)
    g *= m_param->upstream->gain();
  return g;
}

void PortStrip::poll()
{
  if(!m_param || m_dragging)
    return;
  // Called often: the widgets only change, and repaint, when the value does.
  const double g = m_param->gain();
  if(g == m_shownGain)
    return;
  m_shownGain = g;
  QSignalBlocker b{m_fader};
  m_fader->setValue(GainFader::gainToPosition(g));
  setGainReadout(g);
}

void PortStrip::onNodeRemoved(const ossia::net::node_base&)
{
  m_param = nullptr;
  setEnabled(false);
}

void addAudioPort(
    const score::DocumentContext& ctx, Dataflow::AudioDevice& dev, QWidget* parent,
    const std::string& kind)
{
  auto proto = protocolOf(ctx, dev);
  if(!proto)
    return;
  Device::AddressSettings proposed;
  proposed.extendedAttributes["audio-kind"] = kind;
  proposed.extendedAttributes["audio-channels"] = 2;
  std::unique_ptr<Device::AddressDialog> dial{
      proto->makeEditAddressDialog(proposed, dev, ctx, parent)};
  if(!dial || dial->exec() != QDialog::Accepted)
    return;

  auto stgs = dial->getSettings();
  stgs.name = stgs.name.trimmed();
  while(stgs.name.startsWith('/'))
    stgs.name.remove(0, 1);
  if(stgs.name.isEmpty())
    return;

  auto& plug = ctx.plugin<Explorer::DeviceDocumentPlugin>();
  RedoMacroCommandDispatcher<Explorer::Command::AddAddresses> disp{ctx.commandStack};
  if(!explorerNode(plug, dev))
    disp.submit(new Explorer::Command::LoadDevice{plug, dev.settings()});
  if(auto node = explorerNode(plug, dev))
    disp.submit(new Explorer::Command::AddAddress{
        plug, Device::NodePath{*node}, InsertMode::AsChild, stgs});
  disp.commit();
}

void PortStrip::fillContextMenu(QMenu& menu)
{
  // Only the ports the user made can be edited.
  if(!m_param || !(dynamic_cast<ossia::mapped_audio_parameter*>(m_param)
                   || dynamic_cast<ossia::virtual_audio_parameter*>(m_param)))
    return;

  auto& plug = m_context.plugin<Explorer::DeviceDocumentPlugin>();
  auto dev = plug.list().audioDevice();
  if(!dev)
    return;
  auto address = State::Address::fromString(
      "audio:" + QString::fromStdString(m_param->get_node().osc_address()));
  if(!address)
    return;

  auto edit = menu.addAction(tr("Edit the port..."));
  connect(edit, &QAction::triggered, this, [this, dev, addr = *address] {
    // The strips are rebuilt when the ports change, which may happen while the
    // dialog is open: past this point, nothing of this strip is used.
    auto& ctx = m_context;
    QWidget* window = this->window();
    auto& plug = ctx.plugin<Explorer::DeviceDocumentPlugin>();
    auto node = Device::try_getNodeFromAddress(plug.rootNode(), addr);
    auto proto = protocolOf(ctx, *dev);
    if(!node || !node->is<Device::AddressSettings>() || !proto)
      return;
    std::unique_ptr<Device::AddressDialog> dial{proto->makeEditAddressDialog(
        node->get<Device::AddressSettings>(), *dev, ctx, window)};
    if(!dial || dial->exec() != QDialog::Accepted)
      return;
    // The node may be gone with the dialog open.
    node = Device::try_getNodeFromAddress(plug.rootNode(), addr);
    if(!node)
      return;
    CommandDispatcher<>{ctx.commandStack}.submit<Explorer::Command::UpdateAddressSettings>(
        plug, Device::NodePath{*node}, dial->getSettings());
  });

  auto remove = menu.addAction(tr("Remove the port"));
  connect(remove, &QAction::triggered, this, [this, addr = *address] {
    auto& plug = m_context.plugin<Explorer::DeviceDocumentPlugin>();
    if(auto node = Device::try_getNodeFromAddress(plug.rootNode(), addr))
      CommandDispatcher<>{m_context.commandStack}.submit(
          new Explorer::Command::Remove{plug, Device::NodePath{*node}});
  });
}
}
