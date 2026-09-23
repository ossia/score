#include "MixerPanel.hpp"

#include "MixerStrips.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Audio/AudioDevice.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/Telemetry.hpp>

#include <score/selection/Selection.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/HelpInteraction.hpp>
#include <score/widgets/MarginLess.hpp>

#include <ossia/audio/audio_parameter.hpp>
#include <ossia/audio/audio_protocol.hpp>
#include <ossia/network/base/node.hpp>

#include <QFrame>
#include <QAbstractButton>
#include <QHBoxLayout>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
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

//! The side of a folding section: its name, written downwards, under a
//! triangle that points to where the strips go. Checked while unfolded.
class SectionTab final : public QAbstractButton
{
public:
  SectionTab(const QString& text, QWidget* parent)
      : QAbstractButton{parent}
  {
    setText(text);
    setCheckable(true);
    setFocusPolicy(Qt::TabFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  }

  static int thickness(const QFontMetrics& fm) { return fm.height() + 8; }

  QSize sizeHint() const override
  {
    return {thickness(fontMetrics()), fontMetrics().horizontalAdvance(text()) + 28};
  }
  QSize minimumSizeHint() const override { return sizeHint(); }

protected:
  void paintEvent(QPaintEvent*) override
  {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    const auto& pal = palette();

    p.fillRect(rect(), underMouse() ? pal.color(QPalette::Midlight) : pal.color(QPalette::Button));
    if(hasFocus())
    {
      p.setPen(pal.color(QPalette::Highlight));
      p.drawRect(rect().adjusted(0, 0, -1, -1));
    }

    // Unfolded, the triangle points to the strips; folded, along the tab.
    constexpr double a = 6.;
    const double x = (width() - a) / 2.;
    constexpr double y = 8.;
    QPolygonF arrow;
    if(isChecked())
      arrow << QPointF{x, y} << QPointF{x + a, y + a / 2.} << QPointF{x, y + a};
    else
      arrow << QPointF{x, y} << QPointF{x + a, y} << QPointF{x + a / 2., y + a};
    p.setPen(Qt::NoPen);
    p.setBrush(pal.color(QPalette::WindowText));
    p.drawPolygon(arrow);

    p.setPen(pal.color(QPalette::WindowText));
    p.translate(width(), y + a + 6.);
    p.rotate(90.);
    p.drawText(
        QRectF{0., 0., double(height()) - (y + a + 6.), double(width())},
        Qt::AlignLeft | Qt::AlignVCenter, text());
  }

  void enterEvent(QEnterEvent* e) override
  {
    QAbstractButton::enterEvent(e);
    update();
  }
  void leaveEvent(QEvent* e) override
  {
    QAbstractButton::leaveEvent(e);
    update();
  }
};
}

class MixerPanel final : public QWidget
{
public:
  MixerPanel(const score::DocumentContext& ctx, QWidget* parent)
      : QWidget{parent}
      , m_context{ctx}
  {
    auto lay = new score::MarginLess<QVBoxLayout>{this};
    QSettings s;
    const int hidden = s.value(settings_sections, 0).toInt();
    m_width = StripWidth(
        std::clamp(s.value(settings_width, int(StripWidth::Normal)).toInt(), 0, 2));

    // The sections scroll; the master stays on the right.
    auto body = new QWidget{this};
    auto body_lay = new score::MarginLess<QHBoxLayout>{body};
    m_scroll = new QScrollArea{body};
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto content = new QWidget{m_scroll};
    auto content_lay = new score::MarginLess<QHBoxLayout>{content};
    content_lay->setSpacing(6);
    const int tab_w = SectionTab::thickness(fontMetrics());
    for(int i = 0; i < section_count; i++)
    {
      // A tab on the left folds the section's strips away.
      auto sec = new QWidget{content};
      auto sec_lay = new score::MarginLess<QHBoxLayout>{sec};
      sec_lay->setSpacing(4);

      auto side = new QWidget{sec};
      auto side_lay = new score::MarginLess<QVBoxLayout>{side};
      side_lay->setSpacing(1);
      auto tab = new SectionTab{sectionName(Section(i)), side};
      tab->setChecked(!(hidden & (1 << i)));
      score::setHelp(tab, sectionHelp(Section(i)));
      side_lay->addWidget(tab, 1);
      if(Section(i) == Section::Mapped || Section(i) == Section::Virtual)
      {
        auto add = new QToolButton{side};
        add->setText(QStringLiteral("+"));
        add->setAutoRaise(true);
        add->setFixedSize(tab_w, tab_w);
        score::setHelp(
            add, tr("Add a port to the audio device: some channels of the sound "
                    "card, or a virtual port."));
        const std::string kind = Section(i) == Section::Virtual ? "virtual" : "in";
        connect(add, &QToolButton::clicked, this, [this, kind] {
          if(m_device)
            addAudioPort(m_context, *m_device, this, kind);
        });
        side_lay->addWidget(add);
      }
      sec_lay->addWidget(side);

      auto strips = new QWidget{sec};
      auto strips_lay = new score::MarginLess<QHBoxLayout>{strips};
      strips_lay->setSpacing(2);
      strips_lay->setAlignment(Qt::AlignLeft);
      strips->setVisible(tab->isChecked());
      sec_lay->addWidget(strips, 1);
      content_lay->addWidget(sec);

      connect(tab, &SectionTab::toggled, this, [strips, i](bool shown) {
        strips->setVisible(shown);
        QSettings s;
        int hidden = s.value(settings_sections, 0).toInt();
        hidden = shown ? (hidden & ~(1 << i)) : (hidden | (1 << i));
        s.setValue(settings_sections, hidden);
      });
      tab->setContextMenuPolicy(Qt::CustomContextMenu);
      connect(tab, &QWidget::customContextMenuRequested, this, [this, tab](QPoint pos) {
        widthMenu(tab->mapToGlobal(pos));
      });

      m_sections[i] = sec;
      m_strips[i] = strips;
    }
    content_lay->addStretch(1);
    m_scroll->setWidget(content);
    body_lay->addWidget(m_scroll, 1);

    m_master = new QWidget{body};
    new score::MarginLess<QVBoxLayout>{m_master};
    body_lay->addSpacing(12);
    body_lay->addWidget(m_master);

    // The strips never get shorter than they can be; while the sections
    // scroll, the master leaves the scroll bar's room too, so that both end
    // at the same height.
    auto bar_h = m_scroll->horizontalScrollBar()->sizeHint().height();
    updateMinimumHeight();
    connect(
        m_scroll->horizontalScrollBar(), &QScrollBar::rangeChanged, this,
        [this, bar_h](int, int max) {
      m_master->layout()->setContentsMargins(0, 0, 0, max > 0 ? bar_h : 0);
    });
    lay->addWidget(body, 1);

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
  StripWidth width() const noexcept { return m_width; }

  //! The strips never get shorter than they can be, with room for the scroll
  //! bar; this changes with the strips shown.
  void updateMinimumHeight()
  {
    // New strips are shown, and counted, once the event loop runs.
    QTimer::singleShot(0, this, [this] {
      const int bar_h = m_scroll->horizontalScrollBar()->sizeHint().height();
      m_scroll->widget()->layout()->activate();
      m_scroll->setMinimumHeight(
          m_scroll->widget()->minimumSizeHint().height() + bar_h);
    });
  }

  //! The width of every strip.
  void widthMenu(QPoint at)
  {
    QMenu menu;
    auto widths = menu.addMenu(tr("Width of every strip"));
    for(auto [w, name] :
        {std::pair{StripWidth::Narrow, tr("Narrow")},
         std::pair{StripWidth::Normal, tr("Normal")},
         std::pair{StripWidth::Wide, tr("Wide")}})
    {
      auto act = widths->addAction(name);
      act->setCheckable(true);
      act->setChecked(m_width == w);
      connect(act, &QAction::triggered, this, [this, w = w] {
        m_width = w;
        QSettings{}.setValue(settings_width, int(w));
        forEachStrip([w](Strip& s) { s.setStripWidth(w); });
      });
    }
    menu.exec(at);
  }

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
    setSelection(m_context.selectionStack.currentSelection());
    updateMinimumHeight();
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
      static_cast<QVBoxLayout*>(m_master->layout())->addWidget(s, 1);
      m_portStrips.push_back(s);
    }
    updateMinimumHeight();
  }

  const score::DocumentContext& m_context;
  QPointer<Dataflow::AudioDevice> m_device;
  QScrollArea* m_scroll{};
  StripWidth m_width{StripWidth::Normal};
  std::array<QWidget*, section_count> m_sections{};
  std::array<QWidget*, section_count> m_strips{};
  QWidget* m_master{};
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
                  "device.\nRight-click a strip, or a section's side, to change the "
                  "width of the strips."));
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
