#include <Audio/ALSAPortAudioInterface.hpp>
#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/View.hpp>
#include <score/widgets/SignalUtils.hpp>
#include <score/tools/Bind.hpp>
#include <score/command/Dispatchers/SettingsCommandDispatcher.hpp>
#include <QComboBox>
#include <QLabel>
#include <QFormLayout>

namespace Audio
{
#if defined(OSSIA_AUDIO_PORTAUDIO) && __has_include(<pa_linux_alsa.h>)

class ALSAWidget : public QWidget
{

  ALSAFactory& m_factory;
public:
  ALSAWidget(
      ALSAFactory& fact,
      Audio::Settings::Model& m,
      Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp,
      QWidget* parent = nullptr)
    : QWidget{parent}
    , m_factory{fact}
  {
    auto& devices = fact.devices;
    auto lay = new QFormLayout{this};

    auto card_list = new QComboBox{this};

    auto informations = new QLabel{this};

    auto set_informations = [=](const PortAudioCard& dev) {
      informations->setText(tr("Inputs:\t%1\nOutputs:\t%2\nRate:\t%3")
                                .arg(dev.inputChan)
                                .arg(dev.outputChan)
                                .arg(dev.rate));
    };

    // Disabled case
    card_list->addItem(devices.front().pretty_name, 0);
    devices.front().out_index = 0;

    // Normal devices
    for (std::size_t i = 1; i < devices.size(); i++)
    {
      auto& card = devices[i];
      card_list->addItem(card.pretty_name, (int)i);
      card.out_index = card_list->count() - 1;
    }

    fact.addBufferSizeWidget(*this, m, v);
    fact.addSampleRateWidget(*this, m, v);

    using Model = Audio::Settings::Model;

    {
      lay->addRow(QObject::tr("Device"), card_list);

      auto update_dev = [=, &m, &m_disp](const PortAudioCard& dev) {
        if (dev.raw_name != m.getCardOut())
        {
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardIn>(
              m, dev.raw_name);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardOut>(
              m, dev.raw_name);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultIn>(
              m, dev.inputChan);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultOut>(
              m, dev.outputChan);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelRate>(
              m, dev.rate);

          set_informations(dev);
        }
      };

      QObject::connect(
          card_list,
          SignalUtils::QComboBox_currentIndexChanged_int(),
          &v,
          [=](int i) {
            auto& device = devices[card_list->itemData(i).toInt()];
            update_dev(device);
          });

      if (m.getCardOut().isEmpty())
      {
        if (!devices.empty())
        {
          update_dev(devices.front());
        }
      }
      else
      {
        setCard(card_list, m.getCardOut());
      }
    }

    {
      lay->addWidget(informations);
      set_informations(
          devices[card_list->itemData(card_list->currentIndex()).toInt()]);
    }
    con(m, &Model::changed, this, [=, &m] {
      setCard(card_list, m.getCardOut());
    });

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

ALSAFactory::ALSAFactory()
{
  QString res = qgetenv("SCORE_DISABLE_ALSA");
  if(res.isEmpty())
    rescan();
}

ALSAFactory::~ALSAFactory()
{

}

void ALSAFactory::rescan()
{
  devices.clear();
  PortAudioScope portaudio;

  devices.push_back(
      PortAudioCard{{}, {}, QObject::tr("No device"), -1, 0, 0, {}});
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

        devices.push_back(PortAudioCard{"ALSA",
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

std::unique_ptr<ossia::audio_engine> ALSAFactory::make_engine(
    const Audio::Settings::Model& set,
    const score::ApplicationContext& ctx)
{
  return std::make_unique<ossia::portaudio_engine>(
      "ossia score",
      set.getCardIn().toStdString(),
      set.getCardOut().toStdString(),
      set.getDefaultIn(),
      set.getDefaultOut(),
      set.getRate(),
      set.getBufferSize());
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
