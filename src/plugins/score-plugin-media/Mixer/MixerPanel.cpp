#include "MixerPanel.hpp"

#include <Audio/AudioDevice.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Process/Commands/EditPort.hpp>
#include <Process/Dataflow/AudioPortComboBox.hpp>

#include <score/model/ComponentUtils.hpp>
#include <score/model/Skin.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/ClearLayout.hpp>
#include <score/widgets/DoubleSlider.hpp>
#include <score/widgets/MarginLess.hpp>

#include <ossia/audio/audio_parameter.hpp>
#include <ossia/audio/audio_protocol.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/parameter.hpp>

#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QComboBox>
#include <QTabWidget>
#include <QToolButton>
#include <qlabel.h>

#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

namespace Mixer
{

class AudioSliderWidget : public score::DoubleSlider
{
public:
  AudioSliderWidget(QWidget* widg)
      : score::DoubleSlider{widg}
  {
    setOrientation(Qt::Vertical);
    setMinimumSize(20, 50);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setBorderWidth(0);
  }
  ~AudioSliderWidget() override { }
};

class PanSliderWidget : public score::DoubleSlider
{
public:
  PanSliderWidget(QWidget* widg)
      : score::DoubleSlider{widg}
  {
    setOrientation(Qt::Horizontal);
    setMinimumSize(20, 18);
  }
  ~PanSliderWidget() override { }

  void setPan(const ossia::pan_weight& p)
  {
    if (p.size() != 2)
    {
      setValue(0.5);
      return;
    }

    // setValue(asin(p[1]) / ossia::half_pi);
    setValue((1. + -p[0] + p[1]) / 2.);
  }

protected:
  void paintEvent(QPaintEvent*) override
  {
    auto& skin = score::Skin::instance();
    QPainter p{this};

    double ratio = 2. * value() - 1.;

    p.setPen(skin.TransparentPen);
    p.setBrush(skin.SliderBrush);
    p.drawRect(rect());

    p.setPen(skin.TransparentPen);
    p.setBrush(skin.SliderInteriorBrush);

    const double y = 0;
    const double h = height();
    const double hw = width() / 2.;
    const double w = hw * std::abs(ratio);

    if (ratio < 0)
    {
      p.drawRect(QRectF{hw - w, y, w, h});
      p.setPen(skin.SliderLine);
      p.drawLine(QPointF{hw - w, h - 1}, QPointF{hw - 1, h - 1});

      p.setFont(skin.SansFontSmall);
      p.drawText(rect(), "  L", Qt::AlignLeft | Qt::AlignVCenter);
      p.setPen(skin.LightGray.main.pen0);
      p.drawText(rect(), "R  ", Qt::AlignRight | Qt::AlignVCenter);
    }
    else if(ratio > 0)
    {
      p.drawRect(QRectF{hw, y, w, h});
      p.setPen(skin.SliderLine);
      p.drawLine(QPointF{hw, h - 1}, QPointF{hw + w - 1, h - 1});

      p.setFont(skin.SansFontSmall);
      p.setPen(skin.LightGray.main.pen0);
      p.drawText(rect(), "  L", Qt::AlignLeft | Qt::AlignVCenter);
      p.setPen(skin.SliderLine);
      p.drawText(rect(), "R  ", Qt::AlignRight | Qt::AlignVCenter);
    }
    else if(ratio == 0)
    {
      p.drawRect(QRectF{hw, y, w, h});

      p.setFont(skin.SansFontSmall);
      p.setPen(skin.LightGray.main.pen0);
      p.drawText(rect(), "  L", Qt::AlignLeft | Qt::AlignVCenter);
      p.drawText(rect(), "R  ", Qt::AlignRight | Qt::AlignVCenter);
    }
  }
};

// TODO MOVEME
class AudioDeviceSlider
    : public QWidget
    , public Nano::Observer
{
public:
  score::MarginLess<QVBoxLayout> lay;
  QLabel label;
  AudioSliderWidget slider;
  ossia::audio_parameter* p{};
  ossia::audio_parameter::callback_index idx;
  AudioDeviceSlider(ossia::audio_parameter& param, QWidget* parent)
      : QWidget{parent}
      , lay{this}
      , label{this}
      , slider{this}
      , p{&param}
  {
    slider.setOrientation(Qt::Vertical);
    setMinimumSize(80, 130);

    lay.addWidget(&label);
    lay.addWidget(&slider);
    lay.setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    auto addr = QString::fromStdString(param.get_node().osc_address());
    label.setText(addr);

    slider.setValue(*param.value().target<float>());
    con(slider, &AudioSliderWidget::valueChanged, this, [&](double d) {
      param.push_value(d);
    });
    idx = param.add_callback([=](const ossia::value& v) {
      slider.setValue(ossia::convert<float>(v));
    });
    param.get_node()
        .about_to_be_deleted.connect<&AudioDeviceSlider::onParamRemoved>(
            *this);
  }

  void onParamRemoved(const ossia::net::node_base& n) { p = nullptr; }

  ~AudioDeviceSlider() override
  {
    if (p)
      p->remove_callback(idx);
  }
};

class AudioBusWidget : public QWidget
{
public:
  AudioBusWidget(
      const Scenario::IntervalModel* param,
      const Dataflow::AudioDevice* dev,
      const score::DocumentContext& ctx,
      QWidget* parent)
      : QWidget{parent}
      , m_context{ctx}
      , m_lay{this}
      , m_title{this}
      , m_gainSlider{this}
      , m_panSlider{this}
      , m_model{param}
  {
    SCORE_ASSERT(!param->graphal());
    setMinimumSize(60, 140);
    setMaximumSize(100, 400);

    m_title.setFlat(true);

    m_title.setText(m_model->metadata().getName());
    m_gainSlider.setWhatsThis("Gain control");
    m_gainSlider.setToolTip(m_gainSlider.whatsThis());
    m_gainSlider.setAttribute(Qt::WA_WState_OwnSizePolicy, true);

    m_mute.setWhatsThis("Mute");
    m_mute.setToolTip(m_mute.whatsThis());
    m_mute.setCheckable(true);
    m_upmix.setWhatsThis("Upmix mono -> stereo");
    m_upmix.setToolTip(m_upmix.whatsThis());
    m_upmix.setCheckable(true);
    m_propagate.setWhatsThis("Propagate automatically sound to parent");
    m_propagate.setToolTip(m_propagate.whatsThis());
    m_propagate.setCheckable(true);
    m_panSlider.setToolTip("Pan control");
    m_panSlider.setWhatsThis(m_panSlider.whatsThis());

    if(dev)
    {
      auto root = State::Address{"audio", {"out"}};
      const auto& node = dev->getNode(root);
      m_audioSelector = Process::makeAddressCombo(root, node, *param->outlet, ctx, parent);

    }

    m_lay.addWidget(&m_title, 0, 0, 1, 2, Qt::AlignLeft);
    m_lay.addWidget(&m_gainSlider, 1, 0, 3, 1);
    m_lay.addWidget(&m_mute, 1, 1, 1, 1);
    m_lay.addWidget(&m_upmix, 2, 1, 1, 1);
    m_lay.addWidget(&m_propagate, 3, 1, 1, 1);
    m_lay.addWidget(&m_panSlider, 7, 0, 1, 2);
    if(dev)
    {
      m_lay.addWidget(m_audioSelector, 8, 0, 1, 3);
    }
    m_lay.setContentsMargins(3, 3, 3, 3);
    m_lay.setSpacing(4);
    m_lay.setRowStretch(m_lay.rowCount(), 1);

    con(m_title, &QPushButton::clicked, this, [this] {
      score::SelectionDispatcher{m_context.selectionStack}.select(*m_model);
    });

    m_gainSlider.setValue(param->outlet->gain());
    con(m_gainSlider,
        &AudioSliderWidget::valueChanged,
        this,
        [this](double d) {
          m_context.dispatcher.submit<Process::SetGain>(*m_model->outlet, d);
        });
    con(m_gainSlider, &AudioSliderWidget::sliderReleased, this, [this] {
      m_context.dispatcher.commit();
    });

    m_mute.setChecked(param->muted());
    con(m_mute, &QPushButton::toggled, this, [this] {
      const_cast<Scenario::IntervalModel*>(m_model)->setMuted(
          m_mute.isChecked());
    });

    m_upmix.setChecked(false);
    con(m_upmix, &QPushButton::toggled, this, [] {
      // TODO
    });

    m_propagate.setChecked(m_model->outlet->propagate());
    con(m_propagate, &QPushButton::toggled, this, [this] {
      m_context.dispatcher.submit<Process::SetPropagate>(
          *m_model->outlet, !m_model->outlet->propagate());
      m_context.dispatcher.commit();
    });

    m_panSlider.setPan(param->outlet->pan());
    con(m_panSlider, &PanSliderWidget::valueChanged, this, [this](double d) {
      double l = sin((1. - d) * ossia::half_pi);
      double r = sin(d * ossia::half_pi);
      m_context.dispatcher.submit<Process::SetPan>(
          *m_model->outlet, ossia::pan_weight{l, r});
    });
    con(m_panSlider, &PanSliderWidget::sliderReleased, this, [this] {
      m_context.dispatcher.commit();
    });
  }

  ~AudioBusWidget() override { }

private:
  const score::DocumentContext& m_context;

  score::MarginLess<QGridLayout> m_lay;
  QPushButton m_title;
  AudioSliderWidget m_gainSlider;
  PanSliderWidget m_panSlider;
  QPushButton m_mute{"M"};
  QPushButton m_upmix{"U"};
  QPushButton m_propagate{"P"};
  QWidget* m_audioSelector{};
  const Scenario::IntervalModel* m_model{};
};

class MixerPanel final : public QTabWidget
{
public:
  const score::DocumentContext& ctx;
  QTabWidget m_tabs;

  QScrollArea m_physicalInArea;
  QWidget m_physicalInWidget;
  score::MarginLess<QHBoxLayout> m_physicalInLayout;
  std::vector<QWidget*> m_physicalInWidgets;

  QScrollArea m_physicalOutArea;
  QWidget m_physicalOutWidget;
  score::MarginLess<QHBoxLayout> m_physicalOutLayout;
  std::vector<QWidget*> m_physicalOutWidgets;

  QScrollArea m_mappingInArea;
  QWidget m_mappingInWidget;
  score::MarginLess<QHBoxLayout> m_mappingInLayout;
  std::vector<QWidget*> m_mappingInWidgets;

  QScrollArea m_mappingOutArea;
  QWidget m_mappingOutWidget;
  score::MarginLess<QHBoxLayout> m_mappingOutLayout;
  std::vector<QWidget*> m_mappingOutWidgets;

  QScrollArea m_virtualArea;
  QWidget m_virtualWidget;
  score::MarginLess<QHBoxLayout> m_virtualLayout;
  std::vector<QWidget*> m_virtualWidgets;

  QScrollArea m_busArea;
  QWidget m_busWidget;
  score::MarginLess<QHBoxLayout> m_busLayout;
  std::vector<QWidget*> m_busWidgets;

  Dataflow::AudioDevice* m_currentDevice{};

  MixerPanel(const score::DocumentContext& ctx, QWidget* parent)
      : QTabWidget{parent}
      , ctx{ctx}
      , m_physicalInArea{this}
      , m_physicalInWidget{&m_physicalInArea}
      , m_physicalInLayout{&m_physicalInWidget}

      , m_physicalOutArea{this}
      , m_physicalOutWidget{&m_physicalOutArea}
      , m_physicalOutLayout{&m_physicalOutWidget}

      , m_mappingInArea{this}
      , m_mappingInWidget{&m_mappingInArea}
      , m_mappingInLayout{&m_mappingInWidget}

      , m_mappingOutArea{this}
      , m_mappingOutWidget{&m_mappingOutArea}
      , m_mappingOutLayout{&m_mappingOutWidget}

      , m_virtualArea{this}
      , m_virtualWidget{&m_virtualArea}
      , m_virtualLayout{&m_virtualWidget}

      , m_busArea{this}
      , m_busWidget{&m_busArea}
      , m_busLayout{&m_busWidget}
  {
    auto setup_tab = [](auto& tab) {
      tab.setMinimumHeight(150);
      tab.setMinimumWidth(150);
      tab.setWidgetResizable(true);
    };
    this->addTab(&m_busArea, "Buses");
    m_busArea.setWidget(&m_busWidget);
    setup_tab(m_busArea);

    this->addTab(&m_physicalInArea, "Ins");
    m_physicalInArea.setWidget(&m_physicalInWidget);
    setup_tab(m_physicalInArea);
    this->addTab(&m_physicalOutArea, "Outs");
    m_physicalOutArea.setWidget(&m_physicalOutWidget);
    setup_tab(m_physicalOutArea);

    this->addTab(&m_mappingInArea, "Map ins");
    m_mappingInArea.setWidget(&m_mappingInWidget);
    setup_tab(m_mappingInArea);
    this->addTab(&m_mappingOutArea, "Map outs");
    m_mappingOutArea.setWidget(&m_mappingOutWidget);
    setup_tab(m_mappingOutArea);

    this->addTab(&m_virtualArea, "Virtual ports");
    m_virtualArea.setWidget(&m_virtualWidget);
    setup_tab(m_virtualArea);

    auto& aplug = ctx.plugin<Explorer::DeviceDocumentPlugin>();
    if (auto audio = aplug.list().audioDevice())
    {
      auto dev = static_cast<Dataflow::AudioDevice*>(audio);
      connect(dev, &Dataflow::AudioDevice::changed, this, [=] {
        setupDevice(dev);
        setupBuses();
      });
      setupDevice(dev);
    }

    auto& plug = ctx.model<Scenario::ScenarioDocumentModel>();
    con(plug,
        &Scenario::ScenarioDocumentModel::busesChanged,
        this,
        &MixerPanel::setupBuses);
    setupBuses();
  }

  void setupDevice(Dataflow::AudioDevice* dev)
  {
    qDeleteAll(m_physicalInWidgets);
    m_physicalInWidgets.clear();
    qDeleteAll(m_physicalOutWidgets);
    m_physicalOutWidgets.clear();
    qDeleteAll(m_mappingInWidgets);
    m_mappingInWidgets.clear();
    qDeleteAll(m_mappingOutWidgets);
    m_mappingOutWidgets.clear();
    qDeleteAll(m_virtualWidgets);
    m_virtualWidgets.clear();

    // Remove the addStretch thing
    delete m_physicalInLayout.takeAt(0);
    delete m_physicalOutLayout.takeAt(0);
    delete m_mappingInLayout.takeAt(0);
    delete m_mappingOutLayout.takeAt(0);
    delete m_virtualLayout.takeAt(0);

    auto& proto = static_cast<ossia::audio_protocol&>(
        dev->getDevice()->get_protocol());
    {
      if (proto.main_audio_in)
      {
        // Main in
        auto w
            = new AudioDeviceSlider{*proto.main_audio_in, &m_physicalInWidget};
        m_mappingInLayout.addWidget(w);
        m_mappingInWidgets.push_back(w);
      }
      for (auto& in : proto.audio_ins)
      {
        auto w = new AudioDeviceSlider{*in, &m_physicalInWidget};
        m_physicalInLayout.addWidget(w);
        m_physicalInWidgets.push_back(w);
      }
      for (auto& in : proto.in_mappings)
      {
        auto w = new AudioDeviceSlider{*in, &m_mappingInArea};
        m_mappingInLayout.addWidget(w);
        m_mappingInWidgets.push_back(w);
      }

      if (proto.main_audio_out)
      {
        // Main out
        auto w = new AudioDeviceSlider{
            *proto.main_audio_out, &m_physicalOutWidget};
        m_mappingOutLayout.addWidget(w);
        m_mappingOutWidgets.push_back(w);
      }
      for (auto& out : proto.audio_outs)
      {
        auto w = new AudioDeviceSlider{*out, &m_physicalOutWidget};
        m_physicalOutLayout.addWidget(w);
        m_physicalOutWidgets.push_back(w);
      }
      for (auto& out : proto.out_mappings)
      {
        auto w = new AudioDeviceSlider{*out, &m_mappingOutArea};
        m_mappingOutLayout.addWidget(w);
        m_mappingOutWidgets.push_back(w);
      }

      for (auto& out : proto.virtaudio)
      {
        auto w = new AudioDeviceSlider{*out, &m_virtualWidget};
        m_virtualLayout.addWidget(w);
        m_virtualWidgets.push_back(w);
      }
    }

    constexpr double width = 75;

    m_physicalInWidget.setMinimumSize(
        (proto.audio_ins.size() + 1) * width, 150);
    m_physicalInLayout.addStretch(1);

    m_physicalOutWidget.setMinimumSize(
        (proto.audio_outs.size() + 1) * width, 150);
    m_physicalOutLayout.addStretch(1);

    m_mappingInWidget.setMinimumSize((proto.in_mappings.size()) * width, 150);
    m_mappingInLayout.addStretch(1);

    m_mappingOutWidget.setMinimumSize(
        (proto.out_mappings.size()) * width, 150);
    m_mappingOutLayout.addStretch(1);

    m_virtualWidget.setMinimumSize((proto.virtaudio.size()) * width, 150);
    m_virtualLayout.addStretch(1);

    m_currentDevice = dev;
  }

  void setupBuses()
  {
    auto& plug = ctx.model<Scenario::ScenarioDocumentModel>();
    qDeleteAll(m_busWidgets);
    m_busWidgets.clear();
    // Remove the addStretch thing
    delete m_busLayout.takeAt(0);

    int i = 0;
    for (auto bus : plug.busIntervals)
    {
      auto w = new AudioBusWidget{bus, m_currentDevice, ctx, &m_busWidget};
      m_busLayout.addWidget(w);
      m_busWidgets.push_back(w);
    }

    m_busWidget.setMinimumSize(i * 100, 150);
    m_busLayout.addStretch(1);
  }
};
PanelDelegate::PanelDelegate(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}
    , m_widget{new QWidget}
{
  m_widget->setLayout(new score::MarginLess<QHBoxLayout>);
  m_widget->setStatusTip(QObject::tr(
                           "This panel shows the audio mixer\n"
                           "Parameters of audio busses and of the audio device \n "
                           "can be accessed here \n"
                           ));
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

void PanelDelegate::on_modelChanged(
    score::MaybeDocument oldm,
    score::MaybeDocument newm)
{
  delete m_cur;
  m_cur = nullptr;

  if (newm)
  {
    m_cur = new MixerPanel{*newm, m_widget};
    m_widget->layout()->addWidget(m_cur);
  }
}

std::unique_ptr<score::PanelDelegate>
PanelDelegateFactory::make(const score::GUIApplicationContext& ctx)
{
  return std::make_unique<PanelDelegate>(ctx);
}
}
