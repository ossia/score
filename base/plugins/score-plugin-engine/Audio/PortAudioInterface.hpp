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
class PortAudioFactory final
    : public AudioFactory
{
  SCORE_CONCRETE("e7543875-3b22-457c-bf41-75504637686f")
public:
  ~PortAudioFactory() override
  {

  }

  QString prettyName() const override { return QObject::tr("PortAudio"); };
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
    int idx = m_CardIn->findData(QVariant::fromValue(val));
    if (idx != -1 && idx != m_CardIn->currentIndex())
      m_CardIn->setCurrentIndex(idx);
    else
    {
      idx = m_CardIn->findText(val);
      if (idx != -1 && idx != m_CardIn->currentIndex())
        m_CardIn->setCurrentIndex(idx);
    }
  }
  static void setCardOut(QComboBox* m_CardOut, QString val)
  {
    int idx = m_CardOut->findData(QVariant::fromValue(val));
    if (idx != -1 && idx != m_CardOut->currentIndex())
      m_CardOut->setCurrentIndex(idx);
    else
    {
      idx = m_CardOut->findText(val);
      if (idx != -1 && idx != m_CardOut->currentIndex())
        m_CardOut->setCurrentIndex(idx);
    }
  }
  QWidget* make_settings(
      Audio::Settings::Model& m,
      Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp,
      QWidget* parent) override
  {
    auto w = new QWidget{parent};
    auto lay = new QFormLayout{w};

    QStringList raw_cards_in, cards_in;
    ossia::int_vector indices_in;

    QStringList raw_cards_out, cards_out;
    ossia::int_vector indices_out;

    Pa_Initialize();

    for (int i = 0; i < Pa_GetHostApiCount(); i++)
    {
      auto dev = Pa_GetHostApiInfo(i);
      QString dev_text;
      switch (dev->type)
      {
        case PaHostApiTypeId::paInDevelopment:
          continue;
        case PaHostApiTypeId::paDirectSound:
          dev_text = "DirectSound";
          break;
        case PaHostApiTypeId::paMME:
          dev_text = "MME";
          break;
        case PaHostApiTypeId::paASIO:
          dev_text = "ASIO";
          break;
        case PaHostApiTypeId::paSoundManager:
          dev_text = "SoundManager";
          break;
        case PaHostApiTypeId::paCoreAudio:
          dev_text = "CoreAudio";
          break;
        case PaHostApiTypeId::paOSS:
          dev_text = "OSS";
          break;
        case PaHostApiTypeId::paALSA:
          dev_text = "ALSA";
          break;
        case PaHostApiTypeId::paAL:
          dev_text = "OpenAL";
          break;
        case PaHostApiTypeId::paBeOS:
          dev_text = "BeOS";
          break;
        case PaHostApiTypeId::paWDMKS:
          dev_text = "WDMKS";
          break;
        case PaHostApiTypeId::paJACK:
          dev_text = "Jack";
          break;
        case PaHostApiTypeId::paWASAPI:
          dev_text = "WASAPI";
          break;
        case PaHostApiTypeId::paAudioScienceHPI:
          dev_text = "HPI";
          break;
      }

      for (int card = 0; card < dev->deviceCount; card++)
      {
        auto dev_idx = Pa_HostApiDeviceIndexToDeviceIndex(i, card);
        auto dev = Pa_GetDeviceInfo(dev_idx);
        auto raw_name = QString::fromLocal8Bit(Pa_GetDeviceInfo(dev_idx)->name);
        if (dev->maxInputChannels > 0)
        {
          cards_in.push_back("(" + dev_text + ") " + raw_name);
          raw_cards_in.push_back(raw_name);
          indices_in.push_back(dev_idx);
        }
        if (dev->maxOutputChannels > 0)
        {
          cards_out.push_back("(" + dev_text + ") " + raw_name);
          raw_cards_out.push_back(raw_name);
          indices_out.push_back(dev_idx);
        }
      }
    }

    Pa_Terminate();

    using Model = Audio::Settings::Model;
    using View = Audio::Settings::View;

    auto card_in = new QComboBox{w};
    auto card_out = new QComboBox{w};
    {
      for (int i = 0; i < cards_in.size(); i++)
        card_in->addItem(cards_in[i], indices_in[i]);
      lay->addRow(QObject::tr("Input device"), card_in);
      QObject::connect(
          card_in, SignalUtils::QComboBox_currentIndexChanged_int(), &v,
          [=,&m,&m_disp](int i) {
        m_disp.submitDeferredCommand<Audio::Settings::SetModelCardIn>(m, raw_cards_in[i]);
      });

      setCardIn(card_in, m.getCardIn());
    }

    {
      for (int i = 0; i < cards_in.size(); i++)
        card_out->addItem(cards_in[i], indices_in[i]);
      lay->addRow(QObject::tr("Output device"), card_out);
      QObject::connect(
          card_out, SignalUtils::QComboBox_currentIndexChanged_int(), &v,
          [=,&m,&m_disp](int i) {
        m_disp.submitDeferredCommand<Audio::Settings::SetModelCardOut>(m, raw_cards_out[i]);
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
