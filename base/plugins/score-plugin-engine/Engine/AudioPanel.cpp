#include "AudioPanel.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <QHBoxLayout>
#include <qlabel.h>

#include <score/widgets/DoubleSlider.hpp>

#include <Engine/Protocols/Audio/AudioDevice.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/audio/audio_parameter.hpp>
#include <score/widgets/MarginLess.hpp>
#include <QScrollArea>
namespace Audio
{
class AudioSlider : public QWidget
{
  public:
    score::MarginLess<QVBoxLayout> lay;
    QLabel label;
    score::DoubleSlider slider;
    ossia::audio_parameter* p{};
    ossia::audio_parameter::callback_index idx;
    AudioSlider(ossia::audio_parameter& param, QWidget* parent):
      QWidget{parent}
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
      con(slider, &score::DoubleSlider::valueChanged,
              this, [&] (double d) {
        param.push_value(d);
      });
      idx = param.add_callback([=] (const ossia::value& v) {
        slider.setValue(ossia::convert<float>(v));
      });
      param.get_node().about_to_be_deleted.connect<AudioSlider, &AudioSlider::onParamRemoved>(*this);
    }

    void onParamRemoved(const ossia::net::node_base& n)
    {
      p = nullptr;
    }

    ~AudioSlider() override
    {
      if(p)
        p->remove_callback(idx);
    }
};
class AudioPanel: public QScrollArea
{
  public:
    QWidget widg;
    score::MarginLess<QHBoxLayout> lay;
    AudioPanel(const score::DocumentContext& ctx, QWidget* parent):
      QScrollArea { parent }
    , widg{this}
    , lay{&widg}
    {
      this->setWidget(&widg);
      auto& plug = ctx.plugin<Explorer::DeviceDocumentPlugin>();
      int i = 0;
      if(auto audio = plug.list().audioDevice())
      {
        auto dev = static_cast<Dataflow::AudioDevice*>(audio);
        auto& proto = static_cast<ossia::audio_protocol&>(dev->getDevice()->get_protocol());

        for(auto& out : proto.virtaudio)
        {
          auto w = new AudioSlider{*out, &widg};
          lay.addWidget(w);
          i++;
        }
        for(auto& out : proto.out_mappings)
        {
          auto w = new AudioSlider{*out, &widg};
          lay.addWidget(w);
          i++;
        }

        for(auto& out : proto.audio_outs)
        {
          auto w = new AudioSlider{*out, &widg};
          lay.addWidget(w);
          i++;
        }
      }

      widg.setMinimumSize(i * 100, 135);
    }
};
PanelDelegate::PanelDelegate(const score::GUIApplicationContext& ctx)
  : score::PanelDelegate{ctx}
  , m_widget{new QWidget}
{
  m_widget->setLayout(new score::MarginLess<QHBoxLayout>);
}

QWidget*PanelDelegate::widget()
{
  return m_widget;
}

const score::PanelStatus&PanelDelegate::defaultPanelStatus() const
{
  static const score::PanelStatus status{true, Qt::BottomDockWidgetArea, 10,
        QObject::tr("Audio"),
        QObject::tr("Ctrl+Shift+A")};

  return status;
}

void PanelDelegate::on_modelChanged(score::MaybeDocument oldm, score::MaybeDocument newm)
{
  delete m_cur;
  m_cur = nullptr;

  if (newm)
  {
    m_cur = new AudioPanel{*newm, m_widget};
    m_widget->layout()->addWidget(m_cur);
  }
}

std::unique_ptr<score::PanelDelegate>
PanelDelegateFactory::make(const score::GUIApplicationContext& ctx)
{
  return std::make_unique<PanelDelegate>(ctx);
}
}
