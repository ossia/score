#include <score/tools/Bind.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QComboBox>
#include <QDebug>
#include <QFormLayout>
#include <QWidget>

#include <Audio/AudioInterface.hpp>
#include <Audio/GenericPortAudioInterface.hpp>
#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/View.hpp>

namespace Audio
{

#if defined(OSSIA_AUDIO_PORTAUDIO)

PortAudioFactory::PortAudioFactory()
{
  // Important note : for this to work, no audio engine must have been
  // created e.g. Pa_Initialize will crash if there's already a jack thread
  // running
  rescan();
}

PortAudioFactory::~PortAudioFactory() { }

void PortAudioFactory::rescan()
{
  devices.clear();

  PortAudioScope portaudio;

  for (int i = 0; i < Pa_GetHostApiCount(); i++)
  {
    auto hostapi = Pa_GetHostApiInfo(i);
    QString api_text;
    switch (hostapi->type)
    {
      case PaHostApiTypeId::paInDevelopment:
        continue;
      case PaHostApiTypeId::paDirectSound:
        api_text = "DirectSound";
        break;
      case PaHostApiTypeId::paMME:
        api_text = "MME";
        break;
      case PaHostApiTypeId::paASIO:
        api_text = "ASIO";
        break;
      case PaHostApiTypeId::paSoundManager:
        api_text = "SoundManager";
        break;
      case PaHostApiTypeId::paCoreAudio:
        api_text = "CoreAudio";
        break;
      case PaHostApiTypeId::paOSS:
        api_text = "OSS";
        break;
      case PaHostApiTypeId::paALSA:
        api_text = "ALSA";
        break;
      case PaHostApiTypeId::paAL:
        api_text = "OpenAL";
        break;
      case PaHostApiTypeId::paBeOS:
        api_text = "BeOS";
        break;
      case PaHostApiTypeId::paWDMKS:
        api_text = "WDMKS";
        break;
      case PaHostApiTypeId::paJACK:
        api_text = "Jack";
        break;
      case PaHostApiTypeId::paWASAPI:
        api_text = "WASAPI";
        break;
      case PaHostApiTypeId::paAudioScienceHPI:
        api_text = "HPI";
        break;
    }

    devices.push_back(PortAudioCard{{}, {}, QObject::tr("Disabled"), -1, 0, 0, {}});
    for (int card = 0; card < hostapi->deviceCount; card++)
    {
      auto dev_idx = Pa_HostApiDeviceIndexToDeviceIndex(i, card);
      auto dev = Pa_GetDeviceInfo(dev_idx);
      auto raw_name = QString::fromLocal8Bit(Pa_GetDeviceInfo(dev_idx)->name);

      devices.push_back(PortAudioCard{
          api_text,
          raw_name,
          "(" + api_text + ") " + raw_name,
          dev_idx,
          dev->maxInputChannels,
          dev->maxOutputChannels,
          hostapi->type,
          dev->defaultSampleRate});
    }
  }
}

QString PortAudioFactory::prettyName() const
{
  return QObject::tr("PortAudio");
}

std::unique_ptr<ossia::audio_engine>
PortAudioFactory::make_engine(const Settings::Model& set, const score::ApplicationContext& ctx)
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

void PortAudioFactory::setCardIn(QComboBox* combo, QString val)
{
  auto dev_it = ossia::find_if(devices, [&](const PortAudioCard& d) { return d.raw_name == val; });
  if (dev_it != devices.end())
  {
    combo->setCurrentIndex(dev_it->in_index);
  }
}

void PortAudioFactory::setCardOut(QComboBox* combo, QString val)
{
  auto dev_it = ossia::find_if(devices, [&](const PortAudioCard& d) { return d.raw_name == val; });
  if (dev_it != devices.end())
  {
    combo->setCurrentIndex(dev_it->out_index);
  }
}

void PortAudioFactory::updateSampleRates(
    QComboBox* rate,
    const PortAudioCard& input,
    const PortAudioCard& output)
{
  PortAudioScope scope;
  rate->clear();
  for (int sr : {22050, 32000, 44100, 48000, 88200, 96000, 192000})
  {
    /*
      PaStreamParameters iParams{};
      iParams.device = input.dev_idx;
      iParams.channelCount = input.inputChan;
      iParams.sampleFormat = paFloat32;
      iParams.suggestedLatency = 0.02;
      */
    PaStreamParameters oParams{};
    oParams.device = output.dev_idx;
    oParams.channelCount = output.outputChan;
    oParams.sampleFormat = paFloat32;
    oParams.suggestedLatency = 0.02;

    if (auto err = Pa_IsFormatSupported(nullptr, /*&iParams, */ &oParams, sr);
        err == paFormatIsSupported)
    {
      rate->addItem(QString::number(sr));
    }
    else
    {
      qDebug() << err << Pa_GetErrorText(err);
    }
  }
}

QWidget* PortAudioFactory::make_settings(
    Settings::Model& m,
    Settings::View& v,
    score::SettingsCommandDispatcher& m_disp,
    QWidget* parent)
{
  auto w = new QWidget{parent};
  auto lay = new QFormLayout{w};

  auto card_in = new QComboBox{w};
  auto card_out = new QComboBox{w};

  addBufferSizeWidget(*w, m, v);
  addSampleRateWidget(*w, m, v);
  auto rate = w->findChild<QComboBox*>("Rate");

  auto updateRates = [=] {
    updateSampleRates(
        rate,
        devices[card_in->itemData(card_in->currentIndex()).toInt()],
        devices[card_out->itemData(card_in->currentIndex()).toInt()]);
  };

  // Disabled case
  card_in->addItem(devices.front().pretty_name, 0);
  card_out->addItem(devices.front().pretty_name, 0);
  devices.front().in_index = 0;
  devices.front().out_index = 0;

  // Normal devices
  for (std::size_t i = 1; i < devices.size(); i++)
  {
    auto& card = devices[i];
    if (card.inputChan > 0)
    {
      card_in->addItem(card.pretty_name, (int)i);
      card.in_index = card_in->count() - 1;
    }
    if (card.outputChan > 0)
    {
      card_out->addItem(card.pretty_name, (int)i);
      card.out_index = card_out->count() - 1;
    }
  }

  using Model = Audio::Settings::Model;

  {
    lay->addRow(QObject::tr("Input device"), card_in);

    auto update_dev = [=, &m, &m_disp](const PortAudioCard& dev) {
      if (dev.raw_name != m.getCardIn())
      {
        m_disp.submitDeferredCommand<Audio::Settings::SetModelCardIn>(m, dev.raw_name);
        m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultIn>(m, dev.inputChan);
        if (dev.hostapi != PaHostApiTypeId::paMME)
        {
          if (dev.out_index != -1 && dev.out_index != card_out->currentIndex())
            card_out->setCurrentIndex(dev.out_index);
        }
      }
    };

    QObject::connect(card_in, SignalUtils::QComboBox_currentIndexChanged_int(), &v, [=](int i) {
      update_dev(devices[card_in->itemData(i).toInt()]);
      updateRates();
    });

    if (m.getCardIn().isEmpty())
    {
      auto default_in = Pa_GetDefaultInputDevice();

      for (auto& v : devices)
      {
        if (v.dev_idx == default_in)
        {
          update_dev(v);
          break;
        }
      }
    }
    else
    {
      setCardIn(card_in, m.getCardIn());
    }

    updateRates();
  }

  {
    lay->addRow(QObject::tr("Output device"), card_out);

    auto update_dev = [=, &m, &m_disp](const PortAudioCard& dev) {
      if (dev.raw_name != m.getCardOut())
      {
        m_disp.submitDeferredCommand<Audio::Settings::SetModelCardOut>(m, dev.raw_name);
        m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultOut>(m, dev.outputChan);
        if (dev.hostapi != PaHostApiTypeId::paMME)
        {
          if (dev.in_index != -1 && dev.in_index != card_in->currentIndex())
            card_in->setCurrentIndex(dev.in_index);
        }
      }
    };

    QObject::connect(card_out, SignalUtils::QComboBox_currentIndexChanged_int(), &v, [=](int i) {
      update_dev(devices[card_out->itemData(i).toInt()]);
      updateRates();
    });

    if (m.getCardOut().isEmpty())
    {
      auto default_out = Pa_GetDefaultOutputDevice();
      for (auto& v : devices)
      {
        if (v.dev_idx == default_out)
        {
          update_dev(v);
          break;
        }
      }
    }
    else
    {
      setCardOut(card_out, m.getCardOut());
    }

    updateRates();
  }
  addBufferSizeWidget(*w, m, v);

  con(m, &Model::changed, &v, [=, &m] {
    setCardIn(card_in, m.getCardIn());
    setCardOut(card_out, m.getCardOut());
  });
  return w;
}

#endif
}
