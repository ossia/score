#pragma once
#include <score/command/Dispatchers/SettingsCommandDispatcher.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <ossia/audio/jack_protocol.hpp>

#include <QDebug>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QTimer>
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
private:
  std::weak_ptr<ossia::jack_client> m_client{};

public:
  ~JackFactory() override { }

  bool available() const noexcept override
  {
#if USE_WEAK_JACK
    auto wj = WeakJack::instance();
    return wj.available();
#else
    return true;
#endif
  }

  QString prettyName() const override { return QObject::tr("JACK"); }
  std::unique_ptr<ossia::audio_engine>
  make_engine(const Audio::Settings::Model& set, const score::ApplicationContext& ctx) override
  {
    static_assert(std::is_base_of_v<ossia::audio_engine, ossia::jack_engine>);
    auto clt = m_client.lock();
    if (!clt)
    {
      m_client = (clt = std::make_shared<ossia::jack_client>("ossia score"));
    }
    return std::make_unique<ossia::jack_engine>(clt, set.getDefaultIn(), set.getDefaultOut());
  }

  void setupSettingsWidget(
      QWidget* w,
      QFormLayout* lay,
      Audio::Settings::Model& m,
      Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp)
  {
    using Model = Audio::Settings::Model;

    auto in_count = new QSpinBox{w};
    auto out_count = new QSpinBox{w};

#if defined(_WIN32)
    {
      if (!ossia::has_jackd_process())
      {
        qDebug() << "JACK server not running?";
        throw std::runtime_error("Audio error: no JACK server");
      }
    }
#endif

    qDebug() << "JACK: " << WeakJack::instance().available();
    std::shared_ptr<ossia::jack_client> client = m_client.lock();
    if (!client)
    {
      m_client = (client = std::make_shared<ossia::jack_client>("ossia score"));
      qDebug("Creating a jack client");
    }

    {
      auto rate = jack_get_sample_rate(*client);
      auto rate_label = new QLabel{QString::number(rate)};
      rate_label->setObjectName("Rate");
      lay->addRow(QObject::tr("Rate"), rate_label);
      m.setRate(rate);
    }
    {
      auto bs = jack_get_buffer_size(*client);
      auto bs_label = new QLabel{QString::number(bs)};
      bs_label->setObjectName("BufferSize");
      lay->addRow(QObject::tr("Buffer size"), bs_label);
      m.setBufferSize(bs);
    }

    {
      in_count->setRange(0, 1024);
      lay->addRow(QObject::tr("Inputs"), in_count);
      QObject::connect(
          in_count, SignalUtils::QSpinBox_valueChanged_int(), w, [=, &m, &m_disp](int i) {
            m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultIn>(m, i);
          });

      in_count->setValue(m.getDefaultIn());
    }

    {
      out_count->setRange(0, 1024);
      lay->addRow(QObject::tr("Outputs"), out_count);
      QObject::connect(
          out_count, SignalUtils::QSpinBox_valueChanged_int(), w, [=, &m, &m_disp](int i) {
            m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultOut>(m, i);
          });

      out_count->setValue(m.getDefaultIn());
    }

    con(m, &Model::changed, w, [=, &m] {
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
  }

  QWidget* make_settings(
      Audio::Settings::Model& m,
      Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp,
      QWidget* parent) override
  {
    auto w = new QWidget{parent};
    auto lay = new QFormLayout{w};

    QTimer::singleShot(1000, [=, &m, &v, &m_disp] {
      try
      {
        setupSettingsWidget(w, lay, m, v, m_disp);
      }
      catch (...)
      {
        qDebug("Could not set up JACK !");
      }
    });

    return w;
  }
};
#endif
}
