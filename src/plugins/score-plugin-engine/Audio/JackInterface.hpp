#pragma once
#include <score/widgets/SignalUtils.hpp>

#include <ossia/audio/jack_protocol.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>

#include <Audio/AudioInterface.hpp>
#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/View.hpp>

namespace Audio
{
#if defined(OSSIA_AUDIO_JACK)
class JackFactory final : public AudioFactory
{
  SCORE_CONCRETE("7ff2af00-f2f5-4930-beec-0e2d21eda195")
public:
  ~JackFactory() override {}

  QString prettyName() const override { return QObject::tr("JACK"); }
  std::unique_ptr<ossia::audio_engine> make_engine(
      const Audio::Settings::Model& set,
      const score::ApplicationContext& ctx) override
  {
    static_assert(std::is_base_of_v<ossia::audio_engine, ossia::jack_engine>);
    return std::make_unique<ossia::jack_engine>(
        "ossia score",
        set.getDefaultIn(),
        set.getDefaultOut(),
        set.getRate(),
        set.getBufferSize());
  }

  QWidget* make_settings(
      Audio::Settings::Model& m,
      Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp,
      QWidget* parent) override
  {
    auto w = new QWidget{parent};
    auto lay = new QFormLayout{w};

    using Model = Audio::Settings::Model;

    auto in_count = new QSpinBox{w};
    auto out_count = new QSpinBox{w};

    {
      in_count->setRange(0, 1024);
      lay->addRow(QObject::tr("Inputs"), in_count);
      QObject::connect(
          in_count,
          SignalUtils::QSpinBox_valueChanged_int(),
          &v,
          [=, &m, &m_disp](int i) {
            m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultIn>(
                m, i);
          });

      in_count->setValue(m.getDefaultIn());
    }

    {
      out_count->setRange(0, 1024);
      lay->addRow(QObject::tr("Outputs"), out_count);
      QObject::connect(
          out_count,
          SignalUtils::QSpinBox_valueChanged_int(),
          &v,
          [=, &m, &m_disp](int i) {
            m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultOut>(
                m, i);
          });

      out_count->setValue(m.getDefaultIn());
    }

    con(m, &Model::changed, &v, [=, &m] {
      {
        auto val = m.getDefaultIn();
        if (val != in_count->value())
          in_count->setValue(val);
      }
      {
        auto val = m.getDefaultOut();
        if (val != out_count->value())
          out_count->setValue(val);
      }
    });
    return w;
  }
};
#endif
}
