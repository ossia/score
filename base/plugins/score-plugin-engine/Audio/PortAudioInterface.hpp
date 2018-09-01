#pragma once
#include <Audio/AudioInterface.hpp>
#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/View.hpp>
#include <score/widgets/SignalUtils.hpp>
#include <ossia/audio/portaudio_protocol.hpp>
#include <QFormLayout>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QWidget>
#include <QFormLayout>
#include <QComboBox>

namespace Audio
{
#if defined(OSSIA_AUDIO_PORTAUDIO)

struct PortAudioCard
{
  QString api;
  QString raw_name;

  int inputChan{};
  int outputChan{};

  PaHostApiTypeId hostapi{};

  int in_index{-1};
  int out_index{-1};
};
class PortAudioFactory final
    : public AudioFactory
{
  SCORE_CONCRETE("e7543875-3b22-457c-bf41-75504637686f")
public:
  std::vector<PortAudioCard> devices;
  PortAudioFactory()
  {
    // Important note : for this to work, no audio engine must have been created
    // e.g. Pa_Initialize will crash if there's already a jack thread running
    rescan();
  }

  ~PortAudioFactory() override
  {
  }

  void rescan()
  {
    devices.clear();
    Pa_Initialize();

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

      for (int card = 0; card < hostapi->deviceCount; card++)
      {
        auto dev_idx = Pa_HostApiDeviceIndexToDeviceIndex(i, card);
        auto dev = Pa_GetDeviceInfo(dev_idx);
        auto raw_name = QString::fromLocal8Bit(Pa_GetDeviceInfo(dev_idx)->name);
        devices.push_back(PortAudioCard{api_text, raw_name, dev->maxInputChannels, dev->maxOutputChannels, hostapi->type});
      }
    }

    Pa_Terminate();
  }

  QString prettyName() const override { return QObject::tr("PortAudio"); }
  std::unique_ptr<ossia::audio_engine> make_engine(
      const Audio::Settings::Model& set,
      const score::ApplicationContext& ctx) override
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

  static void setCardIn(QComboBox* m_CardIn, QString val)
  {
    int idx = m_CardIn->findText(val);
    if (idx != -1 && idx != m_CardIn->currentIndex())
      m_CardIn->setCurrentIndex(idx);
  }
  static void setCardOut(QComboBox* m_CardOut, QString val)
  {
    int idx = m_CardOut->findText(val);
    if (idx != -1 && idx != m_CardOut->currentIndex())
      m_CardOut->setCurrentIndex(idx);
  }

  QWidget* make_settings(
      Audio::Settings::Model& m,
      Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp,
      QWidget* parent) override
  {

    auto w = new QWidget{parent};
    auto lay = new QFormLayout{w};

    auto card_in = new QComboBox{w};
    auto card_out = new QComboBox{w};

    for(std::size_t i = 0; i < devices.size(); i++)
    {
      auto& card = devices[i];
      if (card.inputChan > 0)
      {
        card_in->addItem("(" + card.api + ") " + card.raw_name, (int)i);
        card.in_index = card_in->count() - 1;
      }
      if (card.outputChan > 0)
      {
        card_out->addItem("(" + card.api + ") " + card.raw_name, (int)i);
        card.out_index = card_out->count() - 1;
      }
    }

    using Model = Audio::Settings::Model;
    using View = Audio::Settings::View;

    {
      lay->addRow(QObject::tr("Input device"), card_in);
      QObject::connect(
          card_in, SignalUtils::QComboBox_currentIndexChanged_int(), &v,
          [=,&m,&m_disp](int i) {
        auto& dev = devices[card_in->itemData(i).toInt()];
        if(dev.raw_name != m.getCardIn())
        {
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardIn>(m, dev.raw_name);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultIn>(m, dev.inputChan);
          if(dev.hostapi != PaHostApiTypeId::paMME)
          {
            if(dev.out_index != -1 && dev.out_index != card_out->currentIndex())
              card_out->setCurrentIndex(dev.out_index);
          }
        }
      });

      setCardIn(card_in, m.getCardIn());
    }

    {
      lay->addRow(QObject::tr("Output device"), card_out);
      QObject::connect(
          card_out, SignalUtils::QComboBox_currentIndexChanged_int(), &v,
          [=,&m,&m_disp](int i) {
        auto& dev = devices[card_out->itemData(i).toInt()];
        if(dev.raw_name != m.getCardOut())
        {
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardOut>(m, dev.raw_name);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultOut>(m, dev.outputChan);
          if(dev.hostapi != PaHostApiTypeId::paMME)
          {
            if(dev.in_index != -1 && dev.in_index != card_in->currentIndex())
              card_in->setCurrentIndex(dev.in_index);
          }
        }
      });

      setCardOut(card_out, m.getCardOut());
    }

    con(m, &Model::changed, &v, [=,&m] {
      setCardIn(card_in, m.getCardIn());
      setCardOut(card_in, m.getCardOut());
    });
    return w;
  }
};
#endif
}
