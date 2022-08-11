#include <Audio/PipeWireInterface.hpp>
#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/View.hpp>

#include <score/tools/Bind.hpp>
#include <score/widgets/AddRemoveList.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <ossia/audio/pipewire_protocol.hpp>

#include <QCheckBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QTimer>

#if defined(OSSIA_AUDIO_PIPEWIRE)
#include <QSocketNotifier>

namespace Audio
{
PipeWireAudioFactory::PipeWireAudioFactory()
    : m_client{std::make_shared<ossia::pipewire_context>()}
{
  if(int fd = m_client->get_fd(); fd != -1)
  {
    m_fd = new QSocketNotifier{fd, QSocketNotifier::Read};
    connect(m_fd, &QSocketNotifier::activated, this, [clt = m_client] {
      if(auto lp = clt->lp)
      {
        int result = pw_loop_iterate(lp, 0);
        if(result < 0)
          qDebug() << "pw_loop_iterate: " << spa_strerror(result);
      }
    });
    m_fd->setEnabled(true);
  }
}

PipeWireAudioFactory::~PipeWireAudioFactory()
{
  m_client->synchronize();
  delete m_fd;
  m_client.reset();
}

QString PipeWireAudioFactory::prettyName() const
{
  return QObject::tr("PipeWire");
}

bool PipeWireAudioFactory::available() const noexcept
{
  return ossia::libpipewire::instance().init;
}

void PipeWireAudioFactory::initialize(
    Audio::Settings::Model& set, const score::ApplicationContext& ctx)
{
}

std::shared_ptr<ossia::audio_engine> PipeWireAudioFactory::make_engine(
    const Audio::Settings::Model& set, const score::ApplicationContext& ctx)
{
  static_assert(std::is_base_of_v<ossia::audio_engine, ossia::pipewire_audio_protocol>);

  if(!this->m_client)
    return {};

  if(!this->m_client->lp)
    return {};

  ossia::audio_setup settings;
  settings.name = "ossia score";
  settings.buffer_size = set.getBufferSize();
  settings.rate = set.getRate();
  settings.card_in = "";
  settings.card_out = "";

  for(auto& name : set.getInputNames())
    settings.inputs.push_back(name.toStdString());
  for(auto& name : set.getOutputNames())
    settings.outputs.push_back(name.toStdString());

  while(int64_t(settings.inputs.size()) < set.getDefaultIn())
  {
    settings.inputs.push_back("in_" + std::to_string(settings.inputs.size()));
  }
  while(int64_t(settings.outputs.size()) < set.getDefaultOut())
  {
    settings.outputs.push_back("out_" + std::to_string(settings.outputs.size()));
  }

  auto eng = std::make_shared<ossia::pipewire_audio_protocol>(this->m_client, settings);

  if(set.getAutoConnect())
  {
    eng->autoconnect();
  }

  //ossia::libpipewire::instance().main_loop_run(this->m_client->loop);
  return eng;
}

void PipeWireAudioFactory::setupSettingsWidget(
    QWidget* w, QFormLayout* lay, Audio::Settings::Model& m, Audio::Settings::View& v,
    score::SettingsCommandDispatcher& m_disp)
{
  using Model = Audio::Settings::Model;

#if defined(_WIN32)
  {
    if(!ossia::has_jackd_process())
    {
      qDebug() << "JACK server not running?";
      throw std::runtime_error("Audio error: no JACK server");
    }
  }
#endif

  auto on_noPipeWire = [&] {
    auto label = new QLabel{QObject::tr("PipeWire does not seem to be running.")};
    lay->addWidget(label);
  };

  if(!available())
  {
    on_noPipeWire();
    return;
  }

  if(!m_client)
  {
    m_client = std::make_shared<ossia::pipewire_context>();
  }

  auto loop = m_client->main_loop;
  if(!loop)
  {
    on_noPipeWire();
    return;
  }

  /*
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
  */

  auto autoconnect = new QCheckBox{w};
  {
    lay->addRow(QObject::tr("Auto-connect ports"), autoconnect);
    QObject::connect(autoconnect, &QCheckBox::toggled, w, [=, &m, &m_disp](bool c) {
      m_disp.submitDeferredCommand<Audio::Settings::SetModelAutoConnect>(m, c);
    });
    autoconnect->setChecked(m.getAutoConnect());
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
    QObject::connect(out_ports, &score::AddRemoveList::changed, w, [=, &m, &m_disp]() {
      score::AddRemoveList::sanitize(out_ports, in_ports);
      m_disp.submitDeferredCommand<Audio::Settings::SetModelOutputNames>(
          m, out_ports->content());
    });
  }

  auto in_count = new QSpinBox{w};
  {
    in_count->setRange(0, 1024);
    QObject::connect(
        in_count, SignalUtils::QSpinBox_valueChanged_int(), w, [=, &m, &m_disp](int i) {
          in_ports->setCount(i);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultIn>(m, i);
        });

    in_count->setValue(m.getDefaultIn());
  }

  auto out_count = new QSpinBox{w};
  {
    out_count->setRange(0, 1024);
    QObject::connect(
        out_count, SignalUtils::QSpinBox_valueChanged_int(), w, [=, &m, &m_disp](int i) {
          out_ports->setCount(i);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultOut>(m, i);
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
      auto val = m.getAutoConnect();
      if(val != autoconnect->isChecked())
        autoconnect->setChecked(val);
    }
    {
      auto ports = m.getInputNames();
      if(!in_ports->sameContent(ports))
        in_ports->replaceContent(ports);
    }
    {
      auto ports = m.getOutputNames();
      if(!out_ports->sameContent(ports))
        out_ports->replaceContent(ports);
    }
    {
      auto val = m.getDefaultIn();
      if(val != in_count->value())
        in_count->setValue(val);
    }
    {
      auto val = m.getDefaultOut();
      if(val != out_count->value())
        out_count->setValue(val);
    }
  });
}

QWidget* PipeWireAudioFactory::make_settings(
    Audio::Settings::Model& m, Audio::Settings::View& v,
    score::SettingsCommandDispatcher& m_disp, QWidget* parent)
{
  auto w = new QWidget{parent};
  auto lay = new QFormLayout{w};

  setupSettingsWidget(w, lay, m, v, m_disp);
  return w;
}
}
#endif
