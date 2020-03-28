#include "MixerPanel.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Audio/AudioDevice.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalRawPtrExecution.hpp>
#include <score/widgets/DoubleSlider.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/model/ComponentUtils.hpp>
#include <score/widgets/ClearLayout.hpp>
#include <score/tools/Bind.hpp>
#include <score/model/Skin.hpp>

#include <Process/Commands/EditPort.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <ossia/audio/audio_parameter.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/parameter.hpp>

#include <QHBoxLayout>
#include <QPushButton>
#include <QToolButton>
#include <QPainter>
#include <QScrollArea>
#include <QTabWidget>
#include <qlabel.h>

namespace Mixer
{


class AudioSliderWidget : public score::DoubleSlider
{
public:
  AudioSliderWidget(QWidget* widg) : score::DoubleSlider{widg}
  {
    setOrientation(Qt::Vertical);
    setStyle(score::AbsoluteSliderStyle::instance());
    setMinimumSize(20, 100);
  }
  ~AudioSliderWidget() override { }

protected:
  void paintEvent(QPaintEvent*) override {

    QPainter p{this};
    auto& skin = score::Skin::instance();
    double min = QSlider::minimum();
    double max = QSlider::maximum();
    double val = QSlider::value();

    double ratio = 1. - (max - val) / (max - min);

    static constexpr auto round = 1.5;
    p.setPen(Qt::transparent);
    p.setBrush(skin.SliderBrush);
    p.drawRoundedRect(rect(), round, round);

    p.setPen(skin.LightGray.main.pen0);
    p.setBrush(skin.SliderExtBrush);

    double h = ratio * (height() - 2);
    double y = 1. + ((height() - 2) - h);
    p.drawRect(QRect{1, int(y), (width() - 2), int(h)});
  }
};

class PanSliderWidget : public score::DoubleSlider
{
public:
  PanSliderWidget(QWidget* widg) : score::DoubleSlider{widg}
  {
    setOrientation(Qt::Horizontal);
    setStyle(score::AbsoluteSliderStyle::instance());
    setMinimumSize(20, 10);
  }
  ~PanSliderWidget() override { }

  void setPan(const ossia::pan_weight& p)
  {
      if(p.size() != 2)
      {
          setValue(0.5);
          return;
      }

      //setValue(asin(p[1]) / ossia::half_pi);
      setValue((1. + -p[0] + p[1]) / 2.);
  }

protected:
  void paintEvent(QPaintEvent*) override
  {
    auto& skin = score::Skin::instance();
    QPainter p{this};

    double min = QSlider::minimum();
    double max = QSlider::maximum();
    double val = QSlider::value();

    double ratio = 1. - 2. * (max - val) / (max - min);

    static constexpr auto round = 1.5;
    p.setPen(skin.TransparentPen);
    p.setBrush(skin.SliderBrush);
    p.drawRoundedRect(rect(), round, round);

    //p.setPen(Qt::white);
    p.setPen(skin.LightGray.main.pen0);
    p.setBrush(skin.SliderExtBrush);

    const int y = 1;
    const int h = (height() - 2);
    const double hw = width() / 2.;
    const int w = hw * std::abs(ratio);
    if(ratio <= 0)
    {
      p.drawRect(QRect{int(hw - w + 1), y, std::max(2, w - 2), h});
    }
    else
    {
      p.drawRect(QRect{int(hw + 1), y, std::max(2, w - 2), h});
    }

    p.setFont(skin.SansFontSmall);
    p.drawText(rect(), "  L", Qt::AlignLeft | Qt::AlignVCenter);
    p.drawText(rect(), "R  ", Qt::AlignRight | Qt::AlignVCenter);
  }
};

// TODO MOVEME
class AudioDeviceSlider : public QWidget, public Nano::Observer
{
public:
  score::MarginLess<QVBoxLayout> lay;
  QLabel label;
  AudioSliderWidget slider;
  ossia::audio_parameter* p{};
  ossia::audio_parameter::callback_index idx;
  AudioDeviceSlider(ossia::audio_parameter& param, QWidget* parent)
      : QWidget{parent}, lay{this}, label{this}, slider{this}, p{&param}
  {
    slider.setOrientation(Qt::Vertical);
    setMinimumSize(80, 130);

    lay.addWidget(&label);
    lay.addWidget(&slider);
    lay.setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    auto addr = QString::fromStdString(param.get_node().osc_address());
    label.setText(addr);

    slider.setValue(*param.value().target<float>());
    con(slider, &AudioSliderWidget::doubleValueChanged, this, [&](double d) {
      param.push_value(d);
    });
    idx = param.add_callback([=](const ossia::value& v) {
      slider.setValue(ossia::convert<float>(v));
    });
    param.get_node().about_to_be_deleted.connect<&AudioDeviceSlider::onParamRemoved>(
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
  AudioBusWidget(const Scenario::IntervalModel* param, const score::DocumentContext& ctx, QWidget* parent)
    : QWidget{parent}, m_context{ctx}, m_lay{this}, m_title{this}, m_gainSlider{this}, m_panSlider{this}, m_model{param}
  {
    setStyleSheet("QWidget { font: 7pt \"Ubuntu\"; }");
    setMinimumSize(60, 130);
    setMaximumSize(60, 130);

    m_title.setFlat(true);
    m_title.setStyleSheet("QPushButton { border: none }");
    m_title.setText(m_model->metadata().getName());
    m_gainSlider.setWhatsThis("Gain control");
    m_gainSlider.setToolTip(m_gainSlider.whatsThis());
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

    m_lay.addWidget(&m_title,      0, 0, 1, 2, Qt::AlignLeft);
    m_lay.addWidget(&m_gainSlider, 1, 0, 6, 1);
    m_lay.addWidget(&m_mute,       1, 1, 1, 1);
    m_lay.addWidget(&m_upmix,      2, 1, 1, 1);
    m_lay.addWidget(&m_propagate,  3, 1, 1, 1);
    m_lay.addWidget(&m_panSlider,  7, 0, 1, 2);
    m_lay.setMargin(1);
    m_lay.setSpacing(2);

    con(m_title, &QPushButton::clicked, this, [this] {
        m_context.selectionStack.pushNewSelection({m_model});
    });

    m_gainSlider.setValue(param->outlet->gain());
    con(m_gainSlider, &AudioSliderWidget::doubleValueChanged, this, [this](double d) {
      m_context.dispatcher.submit<Process::SetGain>(*m_model->outlet, d);
    });
    con(m_gainSlider, &AudioSliderWidget::sliderReleased, this, [this] {
        m_context.dispatcher.commit();
    });

    m_mute.setChecked(param->muted());
    con(m_mute, &QPushButton::toggled, this, [this] {
        const_cast<Scenario::IntervalModel*>(m_model)->setMuted(m_mute.isChecked());
    });

    m_upmix.setChecked(false);
    con(m_upmix, &QPushButton::toggled, this, [this] {
        // TODO
    });

    m_propagate.setChecked(m_model->outlet->propagate());
    con(m_propagate, &QPushButton::toggled, this, [this] {
        m_context.dispatcher.submit<Process::SetPropagate>(*m_model->outlet, !m_model->outlet->propagate());
        m_context.dispatcher.commit();
    });

    m_panSlider.setPan(param->outlet->pan());
    con(m_panSlider, &PanSliderWidget::doubleValueChanged, this, [this] (double d) {
        double l = sin((1. - d) * ossia::half_pi);
        double r = sin(d * ossia::half_pi);
        m_context.dispatcher.submit<Process::SetPan>(*m_model->outlet, ossia::pan_weight{l, r});
    });
    con(m_panSlider, &PanSliderWidget::sliderReleased, this, [this] {
        m_context.dispatcher.commit();
    });
  }

  ~AudioBusWidget() override
  {
  }

private:
  const score::DocumentContext& m_context;

  score::MarginLess<QGridLayout> m_lay;
  QPushButton m_title;
  AudioSliderWidget m_gainSlider;
  PanSliderWidget m_panSlider;
  QPushButton m_mute{"M"};
  QPushButton m_upmix{"U"};
  QPushButton m_propagate{"P"};
  const Scenario::IntervalModel* m_model{};
};



class MixerPanel final : public QTabWidget
{
public:
  const score::DocumentContext& ctx;
  QTabWidget m_tabs;
  QScrollArea m_deviceArea;
  QWidget m_deviceWidget;
  score::MarginLess<QHBoxLayout> m_deviceLayout;
  QScrollArea m_busArea;
  QWidget m_busWidget;
  score::MarginLess<QHBoxLayout> m_busLayout;

  MixerPanel(const score::DocumentContext& ctx, QWidget* parent)
      : QTabWidget{parent}
      , ctx{ctx}
      , m_deviceArea{this}
      , m_deviceWidget{&m_deviceArea}
      , m_deviceLayout{&m_deviceWidget}
      , m_busArea{this}
      , m_busWidget{&m_busArea}
      , m_busLayout{&m_busWidget}
  {
    this->addTab(&m_busArea, "Buses");
    m_busArea.setWidget(&m_busWidget);
    m_busArea.setMinimumHeight(150);
    m_busArea.setMinimumWidth(150);
    m_busArea.setWidgetResizable(true);

    this->addTab(&m_deviceArea, "Devices");
    m_deviceArea.setWidget(&m_deviceWidget);
    m_deviceArea.setMinimumHeight(150);
    m_deviceArea.setMinimumWidth(150);
    m_deviceArea.setWidgetResizable(true);

    setupDevice();

    auto& plug = ctx.model<Scenario::ScenarioDocumentModel>();
    con(plug, &Scenario::ScenarioDocumentModel::busesChanged,
        this, &MixerPanel::setupBuses);
    setupBuses();

  }

  void setupDevice()
  {
    auto& plug = ctx.plugin<Explorer::DeviceDocumentPlugin>();
    int i = 0;
    if (auto audio = plug.list().audioDevice())
    {
      auto dev = static_cast<Dataflow::AudioDevice*>(audio);
      auto& proto = static_cast<ossia::audio_protocol&>(
            dev->getDevice()->get_protocol());

      for (auto& out : proto.virtaudio)
      {
        auto w = new AudioDeviceSlider{*out, &m_deviceWidget};
        m_deviceLayout.addWidget(w);
        i++;
      }
      for (auto& out : proto.out_mappings)
      {
        auto w = new AudioDeviceSlider{*out, &m_deviceWidget};
        m_deviceLayout.addWidget(w);
        i++;
      }

      for (auto& out : proto.audio_outs)
      {
        auto w = new AudioDeviceSlider{*out, &m_deviceWidget};
        m_deviceLayout.addWidget(w);
        i++;
      }
    }

    m_deviceWidget.setMinimumSize(i * 100, 150);
    m_deviceLayout.addStretch(1);

  }
  void setupBuses()
  {
    auto& plug = ctx.model<Scenario::ScenarioDocumentModel>();
    score::clearLayout(&m_busLayout);

    int i = 0;
    for(auto bus : plug.busIntervals)
    {
      auto w = new AudioBusWidget{bus, ctx, &m_busWidget};
      m_busLayout.addWidget(w);
    }

    m_busWidget.setMinimumSize(i * 100, 150);
    m_busLayout.addStretch(1);

  }
};
PanelDelegate::PanelDelegate(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}, m_widget{new QWidget}
{
  m_widget->setLayout(new score::MarginLess<QHBoxLayout>);
}

QWidget* PanelDelegate::widget()
{
  return m_widget;
}

const score::PanelStatus& PanelDelegate::defaultPanelStatus() const
{
  static const score::PanelStatus status{false, false,
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
