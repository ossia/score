#pragma once
#include <Audio/AudioInterface.hpp>

namespace Audio
{
#if __has_include(<pa_win_wmme.h>)

class MMEFactory final : public AudioFactory
{
  SCORE_CONCRETE("f5950e60-dac3-4254-bfb7-b94c96c679aa")
public:
  std::vector<PortAudioCard> devices;

  MMEFactory() { rescan(); }

  ~MMEFactory() override { }
  bool available() const noexcept override { return true; }

  void rescan()
  {
    devices.clear();

    PortAudioScope portaudio;

    devices.push_back(PortAudioCard{{}, {}, QObject::tr("No device"), -1, 0, 0, {}});
    for (int i = 0; i < Pa_GetHostApiCount(); i++)
    {
      auto hostapi = Pa_GetHostApiInfo(i);
      if (hostapi->type == PaHostApiTypeId::paMME)
      {
        for (int card = 0; card < hostapi->deviceCount; card++)
        {
          auto dev_idx = Pa_HostApiDeviceIndexToDeviceIndex(i, card);
          auto dev = Pa_GetDeviceInfo(dev_idx);
          auto raw_name = QString::fromUtf8(Pa_GetDeviceInfo(dev_idx)->name);

          devices.push_back(PortAudioCard{
              "MME",
              raw_name,
              raw_name,
              dev_idx,
              dev->maxInputChannels,
              dev->maxOutputChannels,
              hostapi->type});
        }
        break;
      }
    }
  }

  QString prettyName() const override { return QObject::tr("MME"); }
  std::unique_ptr<ossia::audio_engine>
  make_engine(const Audio::Settings::Model& set, const score::ApplicationContext& ctx) override
  {
    return std::make_unique<ossia::portaudio_engine>(
        "ossia score",
        set.getCardIn().toStdString(),
        set.getCardOut().toStdString(),
        set.getDefaultIn(),
        set.getDefaultOut(),
        set.getRate(),
        1024,
        paMME);
  }

  void setCardIn(QComboBox* combo, QString val)
  {
    auto dev_it
        = ossia::find_if(devices, [&](const PortAudioCard& d) {
      return d.raw_name == val && d.inputChan > 0;
    });
    if (dev_it != devices.end())
    {
      combo->setCurrentIndex(dev_it->in_index);
    }
  }
  void setCardOut(QComboBox* combo, QString val)
  {
    auto dev_it
        = ossia::find_if(devices, [&](const PortAudioCard& d) {
      return d.raw_name == val && d.outputChan > 0;
    });
    if (dev_it != devices.end())
    {
      combo->setCurrentIndex(dev_it->out_index);
    }
  }

  void updateSampleRates(QComboBox* rate, const PortAudioCard& input, const PortAudioCard& output)
  {
    PortAudioScope scope;
    rate->clear();
    for (int sr : {44100, 48000, 88200, 96000, 192000})
    {
      PaStreamParameters iParams{}, oParams{};
      iParams.device = input.dev_idx;
      iParams.channelCount = input.inputChan;
      iParams.sampleFormat = paFloat32;
      iParams.suggestedLatency = 0.02;

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
        qDebug() << "MME: samplerate errpr " << err << Pa_GetErrorText(err);
      }
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

    auto card_in = new QComboBox{w};
    auto card_out = new QComboBox{w};

    auto rate = new QComboBox{w};
   // auto buffersize = new QComboBox{w};

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

    {
      lay->addRow(QObject::tr("Sample rate"), rate);

      updateRates();
    }
/*
    {
      lay->addRow(QObject::tr("Buffer size"), buffersize);

      updateRates();
    }
*/
    con(m, &Model::changed, w, [=, &m] {
      setCardIn(card_in, m.getCardIn());
      setCardOut(card_out, m.getCardOut());
    });
    return w;
  }
};
#endif
}
