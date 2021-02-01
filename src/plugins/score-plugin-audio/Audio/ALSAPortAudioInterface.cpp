#include <score/command/Dispatchers/SettingsCommandDispatcher.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

#include <Audio/ALSAPortAudioInterface.hpp>
#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/View.hpp>

namespace Audio
{
#if defined(OSSIA_AUDIO_PORTAUDIO) && __has_include(<pa_linux_alsa.h>)

class ALSAWidget : public QWidget
{
  ALSAFactory& m_factory;
  Audio::Settings::Model& m_model;

  score::SettingsCommandDispatcher& m_disp;
  QComboBox* card_list{};
  QLabel* informations{};

  void setInfos(const PortAudioCard& dev)
  {
    informations->setText(tr("Inputs:\t%1\nOutputs:\t%2\nRate:\t%3")
                              .arg(dev.inputChan)
                              .arg(dev.outputChan)
                              .arg(dev.rate));
  }

  void updateDevice(const PortAudioCard& dev)
  {
    auto& m = m_model;
    if (dev.raw_name != m.getCardOut())
    {
      m_disp.submitDeferredCommand<Audio::Settings::SetModelCardIn>(m, dev.raw_name);
      m_disp.submitDeferredCommand<Audio::Settings::SetModelCardOut>(m, dev.raw_name);
      m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultIn>(m, dev.inputChan);
      m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultOut>(m, dev.outputChan);
      m_disp.submitDeferredCommand<Audio::Settings::SetModelRate>(m, dev.rate);

      setInfos(dev);
    }
  }

  void rescanUI()
  {
    card_list->clear();
    m_factory.rescan();

    auto& devices = m_factory.devices;

    card_list->addItem(devices.front().pretty_name, 0);
    devices.front().out_index = 0;

    // Normal devices
    for (std::size_t i = 1; i < devices.size(); i++)
    {
      auto& card = devices[i];
      card_list->addItem(card.pretty_name, (int)i);
      card.out_index = card_list->count() - 1;
    }
  }

public:
  ALSAWidget(
      ALSAFactory& fact,
      Audio::Settings::Model& m,
      Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp,
      QWidget* parent = nullptr)
      : QWidget{parent}, m_factory{fact}, m_model{m}, m_disp{m_disp}
  {
    auto& devices = fact.devices;
    auto lay = new QFormLayout{this};
    auto rescan = new QPushButton{tr("Rescan"), this};

    card_list = new QComboBox{this};

    informations = new QLabel{this};

    // Disabled case

    QString res = qgetenv("SCORE_DISABLE_ALSA");
    if (res.isEmpty())
    {
      QTimer::singleShot(1000, [this] { rescanUI(); });
    }
    connect(rescan, &QPushButton::clicked, this, &ALSAWidget::rescanUI);

    fact.addBufferSizeWidget(*this, m, v);
    fact.addSampleRateWidget(*this, m, v);

    using Model = Audio::Settings::Model;

    {
      lay->addRow(QObject::tr("Device"), card_list);
      lay->addRow(rescan);

      QObject::connect(
          card_list, SignalUtils::QComboBox_currentIndexChanged_int(), &v, [this](int i) {
            auto& devices = m_factory.devices;
            auto& device = devices[card_list->itemData(i).toInt()];
            updateDevice(device);
          });

      if (m.getCardOut().isEmpty())
      {
        if (!devices.empty())
        {
          updateDevice(devices.front());
        }
      }
      else
      {
        setCard(card_list, m.getCardOut());
      }
    }

    {
      lay->addWidget(informations);
      std::size_t dev_idx = card_list->itemData(card_list->currentIndex()).toInt();
      if (dev_idx < devices.size())
      {
        setInfos(devices[dev_idx]);
      }
    }
    con(m, &Model::changed, this, [=, &m] { setCard(card_list, m.getCardOut()); });
  }
  void setCard(QComboBox* combo, QString val)
  {
    auto dev_it = ossia::find_if(
        m_factory.devices, [&](const PortAudioCard& d) { return d.raw_name == val; });
    if (dev_it != m_factory.devices.end())
    {
      combo->setCurrentIndex(dev_it->out_index);
    }
  }
};

ALSAFactory::ALSAFactory() { }

ALSAFactory::~ALSAFactory() { }

void ALSAFactory::rescan()
{
  devices.clear();
  PortAudioScope portaudio;

  devices.push_back(PortAudioCard{{}, {}, QObject::tr("No device"), -1, 0, 0, {}});
  for (int i = 0; i < Pa_GetHostApiCount(); i++)
  {
    auto hostapi = Pa_GetHostApiInfo(i);
    if (hostapi->type == PaHostApiTypeId::paALSA)
    {
      for (int card = 0; card < hostapi->deviceCount; card++)
      {
        auto dev_idx = Pa_HostApiDeviceIndexToDeviceIndex(i, card);
        auto dev = Pa_GetDeviceInfo(dev_idx);
        auto raw_name = QString::fromUtf8(Pa_GetDeviceInfo(dev_idx)->name);
        auto pretty_name = raw_name;
        if (dev->maxInputChannels == 0)
        {
          pretty_name = tr("(Output) ") + pretty_name;
        }
        else if (dev->maxOutputChannels == 0)
        {
          pretty_name = tr("(Input) ") + pretty_name;
        }

        devices.push_back(PortAudioCard{
            "ALSA",
            raw_name,
            pretty_name,
            dev_idx,
            dev->maxInputChannels,
            dev->maxOutputChannels,
            hostapi->type,
            dev->defaultSampleRate});
      }
    }
  }
}

QString ALSAFactory::prettyName() const
{
  return QObject::tr("ALSA");
}

std::unique_ptr<ossia::audio_engine>
ALSAFactory::make_engine(const Audio::Settings::Model& set, const score::ApplicationContext& ctx)
{
  return std::make_unique<ossia::portaudio_engine>(
      "ossia score",
      set.getCardIn().toStdString(),
      set.getCardOut().toStdString(),
      set.getDefaultIn(),
      set.getDefaultOut(),
      set.getRate(),
      set.getBufferSize(),
      paALSA);
}

QWidget* ALSAFactory::make_settings(
    Audio::Settings::Model& m,
    Audio::Settings::View& v,
    score::SettingsCommandDispatcher& m_disp,
    QWidget* parent)
{
  return new ALSAWidget{*this, m, v, m_disp, parent};
}

#endif

}

namespace Audio
{

#if defined(OSSIA_AUDIO_PULSEAUDIO)
PulseAudioFactory::PulseAudioFactory() { }

PulseAudioFactory::~PulseAudioFactory() { }


bool PulseAudioFactory::available() const noexcept
{
  try {
    ossia::libpulse::instance();
    return true;
  } catch(...) {
    return false;
  }
}

QString PulseAudioFactory::prettyName() const
{
  return QObject::tr("PulseAudio");
}

std::unique_ptr<ossia::audio_engine> PulseAudioFactory::make_engine(
    const Audio::Settings::Model& set,
    const score::ApplicationContext& ctx)
{
  return std::make_unique<ossia::pulseaudio_engine>(
      "ossia score",
      set.getCardIn().toStdString(),
      set.getCardOut().toStdString(),
      set.getDefaultIn(),
      set.getDefaultOut(),
      set.getRate(),
      set.getBufferSize());
}

QWidget* PulseAudioFactory::make_settings(
    Audio::Settings::Model& m,
    Audio::Settings::View& v,
    score::SettingsCommandDispatcher& m_disp,
    QWidget* parent)
{
  return new QWidget;
}
#endif

}
