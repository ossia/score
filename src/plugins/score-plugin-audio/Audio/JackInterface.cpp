#include <Audio/AudioTick.hpp>
#include <Audio/JackInterface.hpp>
#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/View.hpp>

#include <score/tools/Bind.hpp>
#include <score/widgets/AddRemoveList.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QCheckBox>
#include <QDebug>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QWidget>

#include <thread>
#include <wobjectimpl.h>

#if defined(OSSIA_AUDIO_JACK)
W_OBJECT_IMPL(Audio::JackFactory)

namespace Audio
{
JackFactory::~JackFactory() { }

bool JackFactory::available() const noexcept
{
#if USE_WEAK_JACK
  const auto& wj = WeakJack::instance();
  return wj.available() == 0;
#else
  return true;
#endif
}

std::shared_ptr<ossia::jack_client> JackFactory::acquireClient()
try
{
  std::shared_ptr<ossia::jack_client> clt = m_client.lock();

  if (!clt)
    m_client = (clt = std::make_shared<ossia::jack_client>("ossia score"));

  return clt;
}
catch (...)
{
  return {};
}

std::shared_ptr<ossia::audio_engine> JackFactory::make_engine(
    const Audio::Settings::Model& set,
    const score::ApplicationContext& ctx)
{
  static_assert(std::is_base_of_v<ossia::audio_engine, ossia::jack_engine>);

  auto clt = acquireClient();

  ossia::jack_settings settings;
  settings.autoconnect = set.getAutoConnect();
  settings.transport
      = static_cast<ossia::transport_mode>(set.getJackTransport());
  for (auto& name : set.getInputNames())
    settings.inputs.push_back(name.toStdString());
  for (auto& name : set.getOutputNames())
    settings.outputs.push_back(name.toStdString());

  while (int64_t(settings.inputs.size()) < set.getDefaultIn())
  {
    settings.inputs.push_back("in_" + std::to_string(settings.inputs.size()));
  }
  while (int64_t(settings.outputs.size()) < set.getDefaultOut())
  {
    settings.outputs.push_back(
        "out_" + std::to_string(settings.outputs.size()));
  }

  // ! Warning ! these functions are executed in the audio thread
  settings.sync_function
      = [this](jack_transport_state_t st, jack_position_t* pos) -> int {
    if (m_prevState != st)
    {
      // warning! sending a queued signal may allocate
      switch (st)
      {
        case jack_transport_state_t::JackTransportStopped:
        {
          transportStateChanged(ossia::transport_status::stopped);
          m_prevState = st;
          return 1;
        }
        case jack_transport_state_t::JackTransportStarting:
        {
          transportStateChanged(ossia::transport_status::starting);
          m_prevState = st;
          return 0;
        }
        case jack_transport_state_t::JackTransportRolling:
        {
          transportStateChanged(ossia::transport_status::playing);
          m_prevState = st;
          return 1;
        }
        case jack_transport_state_t::JackTransportLooping:
          m_prevState = st;
          {
            return 1;
          }
        default:
          // commented as not available in Debian buster / bullseye : case jack_transport_state_t::JackTransportNetStarting:
          {
            m_prevState = st;
            return 1;
          }
      }
    }
    else
    {
      if (m_prevState == jack_transport_state_t::JackTransportStarting)
      {
        return Audio::execution_status.load()
                       == ossia::transport_status::playing
                   ? 1
                   : 0;
      }
    }
    return 1;
  };
  settings.timebase_function = [&info = this->currentTransportInfo](
                                   int frames, jack_position_t& pos) {
    // pos.frame += frames;
    pos.valid = jack_position_bits_t(JackPositionBBT | JackBBTFrameOffset);

    // Duration of a quarter note in flicks
    const double beat_duration = ossia::quarter_duration<double>;
    const double ticks_per_beat
        = 1920.; // apparently some common value, 2*default midi PPQ (960)

    pos.beats_per_bar = info.signature.upper;
    pos.beat_type = info.signature.lower;
    pos.ticks_per_beat = ticks_per_beat;
    pos.beats_per_minute = info.current_tempo;

    // FIXME speed is broken
    pos.bar = (info.date.impl / (4 * beat_duration));
    pos.beat = (info.date.impl - pos.bar * 4 * beat_duration) / beat_duration;
    pos.tick = (info.date.impl - pos.bar * 4 * beat_duration
                - pos.beat * beat_duration)
               / ticks_per_beat;
    pos.bar_start_tick = info.musical_start_last_bar * ticks_per_beat;

    // bar / beat are human-readable values, 1-based
    pos.bar++;
    pos.beat++;

    pos.bbt_offset = 0;
  };

  return std::make_shared<ossia::jack_engine>(
      clt, set.getDefaultIn(), set.getDefaultOut(), settings);
}

void JackFactory::setupSettingsWidget(
    QWidget* w,
    QFormLayout* lay,
    Audio::Settings::Model& m,
    Audio::Settings::View& v,
    score::SettingsCommandDispatcher& m_disp)
{
  using Model = Audio::Settings::Model;

#if defined(_WIN32)
  {
    if (!ossia::has_jackd_process())
    {
      qDebug() << "JACK server not running?";
      throw std::runtime_error("Audio error: no JACK server");
    }
  }
#endif

  auto on_noJack = [&] {
    auto label = new QLabel{
        QObject::tr("JACK does not seem to be running.\nCheck that jackd is "
                    "running and that /usr/lib/libjack.so exists.")};
    lay->addWidget(label);
  };
  if (WeakJack::instance().available() != 0)
  {
    on_noJack();
    return;
  }

  std::shared_ptr<ossia::jack_client> client = m_client.lock();
  if (!client)
  {
    m_client = (client = std::make_shared<ossia::jack_client>("ossia score"));
  }

  auto clt = client->client;
  if(!clt)
  {
    on_noJack();
    return;
  }

  {
    auto rate = jack_get_sample_rate(clt);
    auto rate_label = new QLabel{QString::number(rate)};
    rate_label->setObjectName("Rate");
    lay->addRow(QObject::tr("Rate"), rate_label);
    m.setRate(rate);
  }
  {
    auto bs = jack_get_buffer_size(clt);
    auto bs_label = new QLabel{QString::number(bs)};
    bs_label->setObjectName("BufferSize");
    lay->addRow(QObject::tr("Buffer size"), bs_label);
    m.setBufferSize(bs);
  }

  auto autoconnect = new QCheckBox{w};
  {
    lay->addRow(QObject::tr("Auto-connect ports"), autoconnect);
    QObject::connect(
        autoconnect, &QCheckBox::toggled, w, [=, &m, &m_disp](bool c) {
          m_disp.submitDeferredCommand<Audio::Settings::SetModelAutoConnect>(
              m, c);
        });
    autoconnect->setChecked(m.getAutoConnect());
  }
  auto transport = new QComboBox{w};
  {
    transport->addItems({"None", "Client", "Master"});
    transport->setCurrentIndex((int)m.getJackTransport());
    lay->addRow(QObject::tr("Enable JACK transport"), transport);
    QObject::connect(
        transport,
        qOverload<int>(&QComboBox::currentIndexChanged),
        w,
        [=, &m, &m_disp](int c) {
          m_disp.submitDeferredCommand<Audio::Settings::SetModelJackTransport>(
              m, static_cast<Audio::Settings::ExternalTransport>(c));
        });
  }

  auto in_ports = new score::AddRemoveList{"in_", m.getInputNames(), w};
  auto out_ports = new score::AddRemoveList{"out_", m.getOutputNames(), w};
  {
    QObject::connect(in_ports, &score::AddRemoveList::changed, w, [=, &m, &m_disp]() {
      score::AddRemoveList::sanitize(in_ports, out_ports);
      m_disp.submitDeferredCommand<Audio::Settings::SetModelInputNames>(
          m, in_ports->content());
    });
  }
  {
    QObject::connect(
        out_ports, &score::AddRemoveList::changed, w, [=, &m, &m_disp]() {
          score::AddRemoveList::sanitize(out_ports, in_ports);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelOutputNames>(
              m, out_ports->content());
        });
  }

  auto in_count = new QSpinBox{w};
  {
    in_count->setRange(0, 1024);
    QObject::connect(
        in_count,
        SignalUtils::QSpinBox_valueChanged_int(),
        w,
        [=, &m, &m_disp](int i) {
          in_ports->setCount(i);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultIn>(
              m, i);
        });

    in_count->setValue(m.getDefaultIn());
  }

  auto out_count = new QSpinBox{w};
  {
    out_count->setRange(0, 1024);
    QObject::connect(
        out_count,
        SignalUtils::QSpinBox_valueChanged_int(),
        w,
        [=, &m, &m_disp](int i) {
          out_ports->setCount(i);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultOut>(
              m, i);
        });

    out_count->setValue(m.getDefaultOut());
  }

  {
    auto hlay = new QHBoxLayout;
    auto left_form = new QFormLayout;
    left_form->addRow(QObject::tr("Inputs"), in_count);
    left_form->addRow(in_ports);
    auto right_form = new QFormLayout;
    right_form->addRow(QObject::tr("Outputs"), out_count);
    right_form->addRow(out_ports);
    hlay->addLayout(left_form);
    hlay->addLayout(right_form);
    lay->addRow(hlay);
  }

  con(m, &Model::changed, w, [=, &m] {
    {
      auto val = m.getJackTransport();
      if ((int)val != transport->currentIndex())
        transport->setCurrentIndex((int)val);
    }
    {
      auto val = m.getAutoConnect();
      if (val != autoconnect->isChecked())
        autoconnect->setChecked(val);
    }
    {
      auto ports = m.getInputNames();
      if (!in_ports->sameContent(ports))
        in_ports->replaceContent(ports);
    }
    {
      auto ports = m.getOutputNames();
      if (!out_ports->sameContent(ports))
        out_ports->replaceContent(ports);
    }
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

QWidget* JackFactory::make_settings(
    Audio::Settings::Model& m,
    Audio::Settings::View& v,
    score::SettingsCommandDispatcher& m_disp,
    QWidget* parent)
{
  auto w = new QWidget{parent};
  auto lay = new QFormLayout{w};

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  QTimer::singleShot(
      1000,
      w,
#else
  QTimer::singleShot(
      1000,
#endif
      [=, &m, &v, &m_disp] {
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
}
#endif
