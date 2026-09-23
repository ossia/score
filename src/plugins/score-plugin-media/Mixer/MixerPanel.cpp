#include "MixerPanel.hpp"

#include "MixerStrips.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Scenario/Commands/Interval/MakeBus.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Audio/AudioDevice.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/Telemetry.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/selection/Selection.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/HelpInteraction.hpp>
#include <score/widgets/MarginLess.hpp>

#include <ossia/audio/audio_parameter.hpp>
#include <ossia/audio/audio_protocol.hpp>
#include <ossia/network/base/node.hpp>

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QScrollArea>
#include <QSettings>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace Mixer
{
namespace
{
enum class Section
{
  Buses,
  Inputs,
  Outputs,
  Mapped,
  Virtual,
};
constexpr int section_count = 5;

QString sectionName(Section s)
{
  switch(s)
  {
    case Section::Buses:
      return QObject::tr("Buses");
    case Section::Inputs:
      return QObject::tr("Inputs");
    case Section::Outputs:
      return QObject::tr("Outputs");
    case Section::Mapped:
      return QObject::tr("Mapped");
    case Section::Virtual:
      return QObject::tr("Virtual");
  }
  return {};
}

QString sectionHelp(Section s)
{
  switch(s)
  {
    case Section::Buses:
      return QObject::tr("Intervals marked as buses in their inspector.");
    case Section::Inputs:
      return QObject::tr("The sound card's inputs, one per channel.");
    case Section::Outputs:
      return QObject::tr("The sound card's outputs, one per channel.");
    case Section::Mapped:
      return QObject::tr(
          "The main input, and the ports mapped onto some channels of the card.");
    case Section::Virtual:
      return QObject::tr("Virtual ports, which carry sound between processes.");
  }
  return {};
}

const QString settings_sections = QStringLiteral("Mixer/HiddenSections");
const QString settings_width = QStringLiteral("Mixer/StripWidth");
}

class MixerPanel final : public QWidget
{
public:
  MixerPanel(const score::DocumentContext& ctx, QWidget* parent)
      : QWidget{parent}
      , m_context{ctx}
  {
    auto lay = new score::MarginLess<QVBoxLayout>{this};

    // Which sections show, and how wide the strips are.
    auto bar = new QWidget{this};
    auto bar_lay = new score::MarginLess<QHBoxLayout>{bar};
    bar_lay->setSpacing(2);
    QSettings s;
    const int hidden = s.value(settings_sections, 0).toInt();
    for(int i = 0; i < section_count; i++)
    {
      auto sec = Section(i);
      auto b = new QToolButton{bar};
      b->setText(sectionName(sec));
      b->setCheckable(true);
      b->setChecked(!(hidden & (1 << i)));
      b->setAutoRaise(true);
      score::setHelp(b, sectionHelp(sec));
      bar_lay->addWidget(b);
      connect(b, &QToolButton::toggled, this, [this, i](bool shown) {
        m_sections[i]->setVisible(shown);
        QSettings s;
        int hidden = s.value(settings_sections, 0).toInt();
        hidden = shown ? (hidden & ~(1 << i)) : (hidden | (1 << i));
        s.setValue(settings_sections, hidden);
      });
    }
    bar_lay->addStretch(1);

    m_widthCombo = new QComboBox{bar};
    m_widthCombo->addItems({tr("Narrow"), tr("Normal"), tr("Wide")});
    m_widthCombo->setCurrentIndex(
        std::clamp(s.value(settings_width, int(StripWidth::Normal)).toInt(), 0, 2));
    score::setHelp(m_widthCombo, tr("Width of every strip"));
    bar_lay->addWidget(m_widthCombo);
    connect(m_widthCombo, &QComboBox::currentIndexChanged, this, [this](int w) {
      QSettings{}.setValue(settings_width, w);
      forEachStrip([w](Strip& s) { s.setStripWidth(StripWidth(w)); });
    });
    lay->addWidget(bar);

    // The sections scroll; the master stays on the right.
    auto body = new QWidget{this};
    auto body_lay = new score::MarginLess<QHBoxLayout>{body};
    m_scroll = new QScrollArea{body};
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto content = new QWidget{m_scroll};
    auto content_lay = new score::MarginLess<QHBoxLayout>{content};
    content_lay->setSpacing(6);
    for(int i = 0; i < section_count; i++)
    {
      auto sec = new QFrame{content};
      auto sec_lay = new score::MarginLess<QVBoxLayout>{sec};
      auto header = new score::MarginLess<QHBoxLayout>;
      auto title = new QLabel{sectionName(Section(i)), sec};
      title->setAlignment(Qt::AlignLeft);
      header->addWidget(title);
      if(Section(i) == Section::Mapped || Section(i) == Section::Virtual)
      {
        auto add = new QToolButton{sec};
        add->setText(QStringLiteral("+"));
        add->setAutoRaise(true);
        score::setHelp(
            add, tr("Add a port to the audio device: some channels of the sound "
                    "card, or a virtual port."));
        connect(add, &QToolButton::clicked, this, [this] {
          if(m_device)
            addAudioPort(m_context, *m_device, this);
        });
        header->addWidget(add);
      }
      header->addStretch(1);
      sec_lay->addLayout(header);
      auto strips = new QWidget{sec};
      auto strips_lay = new score::MarginLess<QHBoxLayout>{strips};
      strips_lay->setSpacing(1);
      strips_lay->setAlignment(Qt::AlignLeft);
      sec_lay->addWidget(strips, 1);
      content_lay->addWidget(sec);
      sec->setVisible(!(hidden & (1 << i)));
      m_sections[i] = sec;
      m_strips[i] = strips;
    }
    content_lay->addStretch(1);
    m_scroll->setWidget(content);
    body_lay->addWidget(m_scroll, 1);

    m_master = new QWidget{body};
    new score::MarginLess<QHBoxLayout>{m_master};
    body_lay->addWidget(m_master);
    lay->addWidget(body, 1);

    // An empty Buses section says how to fill it.
    m_emptyBuses = new QWidget{m_strips[int(Section::Buses)]};
    {
      auto l = new QVBoxLayout{m_emptyBuses};
      auto label = new QLabel{
          tr("No bus yet.\nMark an interval as a bus in its inspector,\nor select "
             "intervals and add them:"),
          m_emptyBuses};
      label->setWordWrap(true);
      l->addWidget(label);
      auto add = new QToolButton{m_emptyBuses};
      add->setText(tr("Add the selected intervals"));
      l->addWidget(add);
      l->addStretch(1);
      connect(add, &QToolButton::clicked, this, &MixerPanel::addSelectedAsBuses);
    }

    auto& devices = ctx.plugin<Explorer::DeviceDocumentPlugin>();
    if(auto audio = devices.list().audioDevice())
      watchDevice(static_cast<Dataflow::AudioDevice*>(audio));
    else
      con(devices.list(), &Device::DeviceList::deviceAdded, this,
          [this](Device::DeviceInterface* d) {
        if(auto audio = qobject_cast<Dataflow::AudioDevice*>(d); audio && !m_device)
          watchDevice(audio);
      });

    auto& doc = ctx.model<Scenario::ScenarioDocumentModel>();
    con(doc, &Scenario::ScenarioDocumentModel::busesChanged, this,
        &MixerPanel::setupBuses);
    setupBuses();

    auto& telemetry = ctx.plugin<Execution::DocumentPlugin>().telemetry();
    con(telemetry, &Execution::Telemetry::updated, this, [this, &telemetry] {
      forEachStrip([&](Strip& s) { s.updateMeter(telemetry); });
    });
    con(ctx.coarseUpdateTimer, &QTimer::timeout, this,
        [this] { forEachStrip([](Strip& s) { s.poll(); }); });

    setSelection(ctx.selectionStack.currentSelection());
  }

  void setSelection(const Selection& sel)
  {
    for(auto s : m_busStrips)
      s->setHighlighted(sel.contains(&s->interval()));
  }

private:
  StripWidth width() const noexcept { return StripWidth(m_widthCombo->currentIndex()); }

  template <typename F>
  void forEachStrip(F&& f)
  {
    for(auto s : m_busStrips)
      f(*s);
    for(auto s : m_portStrips)
      f(*s);
  }

  QHBoxLayout* stripsLayout(Section s) const
  {
    return static_cast<QHBoxLayout*>(m_strips[int(s)]->layout());
  }

  void watchDevice(Dataflow::AudioDevice* dev)
  {
    m_device = dev;
    // A reconnection replaces every parameter; a port edit, some of them.
    connect(dev, &Dataflow::AudioDevice::changed, this, &MixerPanel::requestDevice);
    connect(dev, &Dataflow::AudioDevice::portsChanged, this, &MixerPanel::requestDevice);
    setupDevice();
  }

  // A reconnection announces itself more than once: rebuild once.
  void requestDevice()
  {
    if(m_devicePending)
      return;
    m_devicePending = true;
    QTimer::singleShot(0, this, [this] {
      m_devicePending = false;
      setupDevice();
    });
  }

  void setupBuses()
  {
    qDeleteAll(m_busStrips);
    m_busStrips.clear();

    auto& doc = m_context.model<Scenario::ScenarioDocumentModel>();
    auto lay = stripsLayout(Section::Buses);
    for(auto itv : doc.busIntervals)
    {
      if(itv->graphal())
        continue;
      auto s = new BusStrip{*itv, m_context, m_strips[int(Section::Buses)]};
      s->setStripWidth(width());
      lay->addWidget(s);
      m_busStrips.push_back(s);
    }
    lay->removeWidget(m_emptyBuses);
    lay->addWidget(m_emptyBuses);
    m_emptyBuses->setVisible(m_busStrips.empty());
    setSelection(m_context.selectionStack.currentSelection());
  }

  void setupDevice()
  {
    qDeleteAll(m_portStrips);
    m_portStrips.clear();

    auto dev = m_device.data();
    if(!dev || !dev->getProtocol())
      return;
    auto& proto = *dev->getProtocol();

    auto add = [this](
                   Section sec, ossia::audio_parameter* p, PortStrip::Meter m,
                   std::vector<int> channels = {}) {
      if(!p)
        return;
      auto parent = m_strips[int(sec)];
      auto s = new PortStrip{*p, m, std::move(channels), m_context, parent};
      s->setStripWidth(width());
      stripsLayout(sec)->addWidget(s);
      m_portStrips.push_back(s);
    };

    int i = 0;
    for(auto p : proto.audio_ins)
      add(Section::Inputs, p, PortStrip::Meter::HardwareInputs, {i++});
    i = 0;
    for(auto p : proto.audio_outs)
      add(Section::Outputs, p, PortStrip::Meter::HardwareOutputs, {i++});

    // A mapped port is a set of hardware channels: its meter is theirs.
    auto mapping = [](const ossia::mapped_audio_parameter& p) {
      return std::vector<int>(p.mapping.begin(), p.mapping.end());
    };
    add(Section::Mapped, proto.main_audio_in, PortStrip::Meter::HardwareInputs);
    for(auto p : proto.in_mappings)
      add(Section::Mapped, p, PortStrip::Meter::HardwareInputs, mapping(*p));
    for(auto p : proto.out_mappings)
      add(Section::Mapped, p, PortStrip::Meter::HardwareOutputs, mapping(*p));
    for(auto p : proto.virtaudio)
      add(Section::Virtual, p, PortStrip::Meter::Virtual);

    // The master: every output, after the master gain.
    if(auto p = proto.main_audio_out)
    {
      auto s = new PortStrip{*p, PortStrip::Meter::HardwareOutputs, {}, m_context, m_master};
      s->setStripWidth(StripWidth::Wide);
      score::setHelp(s, tr("The master: its gain applies to every output."));
      m_master->layout()->addWidget(s);
      m_portStrips.push_back(s);
    }
  }

  void addSelectedAsBuses()
  {
    auto& doc = m_context.model<Scenario::ScenarioDocumentModel>();
    CommandDispatcher<> disp{m_context.commandStack};
    for(auto& obj : m_context.selectionStack.currentSelection())
      if(auto itv = qobject_cast<const Scenario::IntervalModel*>(obj.data());
         itv && !itv->graphal() && !ossia::contains(doc.busIntervals, itv))
        disp.submit<Scenario::Command::SetBus>(doc, *itv, true);
  }

  const score::DocumentContext& m_context;
  QPointer<Dataflow::AudioDevice> m_device;
  QScrollArea* m_scroll{};
  QComboBox* m_widthCombo{};
  std::array<QWidget*, section_count> m_sections{};
  std::array<QWidget*, section_count> m_strips{};
  QWidget* m_master{};
  QWidget* m_emptyBuses{};
  std::vector<BusStrip*> m_busStrips;
  std::vector<PortStrip*> m_portStrips;
  bool m_devicePending{};
};

PanelDelegate::PanelDelegate(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}
    , m_widget{new QWidget}
{
  m_widget->setLayout(new score::MarginLess<QHBoxLayout>);
  score::setHelp(
      m_widget,
      QObject::tr("The audio mixer: the buses of the score, and the ports of the audio "
                  "device.\nRight-click a strip to change its width."));
}

QWidget* PanelDelegate::widget()
{
  return m_widget;
}

const score::PanelStatus& PanelDelegate::defaultPanelStatus() const
{
  static const score::PanelStatus status{
      false,
      false,
      Qt::BottomDockWidgetArea,
      10,
      QObject::tr("Audio"),
      "audio",
      QObject::tr("Ctrl+Shift+M")};

  return status;
}

void PanelDelegate::on_modelChanged(score::MaybeDocument oldm, score::MaybeDocument newm)
{
  delete m_cur;
  m_cur = nullptr;

  if(newm)
  {
    m_cur = new MixerPanel{*newm, m_widget};
    m_widget->layout()->addWidget(m_cur);
  }
}

void PanelDelegate::setNewSelection(const Selection& s)
{
  if(m_cur)
    m_cur->setSelection(s);
}

std::unique_ptr<score::PanelDelegate>
PanelDelegateFactory::make(const score::GUIApplicationContext& ctx)
{
  return std::make_unique<PanelDelegate>(ctx);
}
}
