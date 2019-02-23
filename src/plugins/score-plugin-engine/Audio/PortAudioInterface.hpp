#pragma once
#include <score/widgets/SignalUtils.hpp>

#include <ossia/audio/portaudio_protocol.hpp>

#include <QApplication>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QWidget>
#include <QWindow>

#include <Audio/AudioInterface.hpp>
#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/View.hpp>

#if __has_include(<pa_asio.h>)
#include <pa_asio.h>
#endif

namespace Audio
{
#if defined(OSSIA_AUDIO_PORTAUDIO)

struct PortAudioScope
{
  PortAudioScope() { Pa_Initialize(); }

  ~PortAudioScope() { Pa_Terminate(); }
};

struct PortAudioCard
{
  QString api;
  QString raw_name;
  QString pretty_name;
  PaDeviceIndex dev_idx{};

  int inputChan{};
  int outputChan{};

  PaHostApiTypeId hostapi{};

  double rate{};

  int in_index{-1};
  int out_index{-1};
};
class PortAudioFactory final : public AudioFactory
{
  SCORE_CONCRETE("e7543875-3b22-457c-bf41-75504637686f")
public:
  std::vector<PortAudioCard> devices;

  PortAudioFactory()
  {
    // Important note : for this to work, no audio engine must have been
    // created e.g. Pa_Initialize will crash if there's already a jack thread
    // running
    rescan();
  }

  ~PortAudioFactory() override {}

  void rescan()
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

      devices.push_back(
          PortAudioCard{{}, {}, QObject::tr("Disabled"), -1, 0, 0, {}});
      for (int card = 0; card < hostapi->deviceCount; card++)
      {
        auto dev_idx = Pa_HostApiDeviceIndexToDeviceIndex(i, card);
        auto dev = Pa_GetDeviceInfo(dev_idx);
        auto raw_name
            = QString::fromLocal8Bit(Pa_GetDeviceInfo(dev_idx)->name);

        devices.push_back(PortAudioCard{api_text,
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

  void setCardIn(QComboBox* combo, QString val)
  {
    auto dev_it = ossia::find_if(
        devices, [&](const PortAudioCard& d) { return d.raw_name == val; });
    if (dev_it != devices.end())
    {
      combo->setCurrentIndex(dev_it->in_index);
    }
  }
  void setCardOut(QComboBox* combo, QString val)
  {
    auto dev_it = ossia::find_if(
        devices, [&](const PortAudioCard& d) { return d.raw_name == val; });
    if (dev_it != devices.end())
    {
      combo->setCurrentIndex(dev_it->out_index);
    }
  }

  void updateSampleRates(
      QComboBox* rate,
      const PortAudioCard& input,
      const PortAudioCard& output)
  {
    PortAudioScope scope;
    rate->clear();
    for (int sr : {22050, 32000, 44100, 48000, 88200, 96000, 192000})
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

      if (auto err
          = Pa_IsFormatSupported(nullptr, /*&iParams, */ &oParams, sr);
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
    auto buffersize = new QComboBox{w};

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
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardIn>(
              m, dev.raw_name);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultIn>(
              m, dev.inputChan);
          if (dev.hostapi != PaHostApiTypeId::paMME)
          {
            if (dev.out_index != -1
                && dev.out_index != card_out->currentIndex())
              card_out->setCurrentIndex(dev.out_index);
          }
        }
      };

      QObject::connect(
          card_in,
          SignalUtils::QComboBox_currentIndexChanged_int(),
          &v,
          [=](int i) {
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
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardOut>(
              m, dev.raw_name);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultOut>(
              m, dev.outputChan);
          if (dev.hostapi != PaHostApiTypeId::paMME)
          {
            if (dev.in_index != -1 && dev.in_index != card_in->currentIndex())
              card_in->setCurrentIndex(dev.in_index);
          }
        }
      };

      QObject::connect(
          card_out,
          SignalUtils::QComboBox_currentIndexChanged_int(),
          &v,
          [=](int i) {
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

    {
      lay->addRow(QObject::tr("Buffer size"), buffersize);

      updateRates();
    }
    con(m, &Model::changed, &v, [=, &m] {
      setCardIn(card_in, m.getCardIn());
      setCardOut(card_out, m.getCardOut());
    });
    return w;
  }
};

#if __has_include(<pa_asio.h>)
class ASIOFactory final : public QObject, public AudioFactory
{
  SCORE_CONCRETE("6b34c6dd-8201-448f-859c-d014f8d01448")
public:
  std::vector<PortAudioCard> devices;

  ASIOFactory() { rescan(); }

  ~ASIOFactory() override {}

  void rescan()
  {
    devices.clear();
    PortAudioScope portaudio;

    devices.push_back(
        PortAudioCard{{}, {}, QObject::tr("No device"), -1, 0, 0, {}});
    for (int i = 0; i < Pa_GetHostApiCount(); i++)
    {
      auto hostapi = Pa_GetHostApiInfo(i);
      if (hostapi->type == PaHostApiTypeId::paASIO)
      {
        for (int card = 0; card < hostapi->deviceCount; card++)
        {
          auto dev_idx = Pa_HostApiDeviceIndexToDeviceIndex(i, card);
          auto dev = Pa_GetDeviceInfo(dev_idx);
          auto raw_name = QString::fromUtf8(Pa_GetDeviceInfo(dev_idx)->name);

          devices.push_back(PortAudioCard{"ASIO",
                                          raw_name,
                                          raw_name,
                                          dev_idx,
                                          dev->maxInputChannels,
                                          dev->maxOutputChannels,
                                          hostapi->type});
        }
      }
    }
  }

  QString prettyName() const override { return QObject::tr("ASIO"); }
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

  void setCard(QComboBox* combo, QString val)
  {
    auto dev_it = ossia::find_if(
        devices, [&](const PortAudioCard& d) { return d.raw_name == val; });
    if (dev_it != devices.end())
    {
      combo->setCurrentIndex(dev_it->out_index);
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

    auto card_list = new QComboBox{w};
    auto show_ui = new QPushButton{tr("Show Control Panel"), w};

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
      lay->addWidget(show_ui);
      connect(show_ui, &QPushButton::clicked, this, [=] {
        auto& dev
            = devices[card_list->itemData(card_list->currentIndex()).toInt()];
        PortAudioScope portaudio;
        PaAsio_ShowControlPanel(dev.dev_idx, GetActiveWindow());
      });
    }

    con(m, &Model::changed, &v, [=, &m] {
      setCard(card_list, m.getCardOut());
    });
    return w;
  }
};
#endif

#if __has_include(<pa_win_wdmks.h>)
class WDMKSFactory final : public QObject, public AudioFactory
{
  SCORE_CONCRETE("d98fca36-4e50-4802-a825-2fa213f95265")
public:
  std::vector<PortAudioCard> devices;

  WDMKSFactory() { rescan(); }

  ~WDMKSFactory() override {}

  void rescan()
  {
    devices.clear();
    PortAudioScope portaudio;

    devices.push_back(
        PortAudioCard{{}, {}, QObject::tr("No device"), -1, 0, 0, {}});
    for (int i = 0; i < Pa_GetHostApiCount(); i++)
    {
      auto hostapi = Pa_GetHostApiInfo(i);
      if (hostapi->type == PaHostApiTypeId::paWDMKS)
      {
        for (int card = 0; card < hostapi->deviceCount; card++)
        {
          auto dev_idx = Pa_HostApiDeviceIndexToDeviceIndex(i, card);
          auto dev = Pa_GetDeviceInfo(dev_idx);
          auto raw_name = QString::fromUtf8(Pa_GetDeviceInfo(dev_idx)->name);

          devices.push_back(PortAudioCard{"WDMKS",
                                          raw_name,
                                          raw_name,
                                          dev_idx,
                                          dev->maxInputChannels,
                                          dev->maxOutputChannels,
                                          hostapi->type});
        }
      }
    }
  }

  QString prettyName() const override { return QObject::tr("WDMKS"); }
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

  void setCard(QComboBox* combo, QString val)
  {
    auto dev_it = ossia::find_if(
        devices, [&](const PortAudioCard& d) { return d.raw_name == val; });
    if (dev_it != devices.end())
    {
      combo->setCurrentIndex(dev_it->out_index);
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

    auto card_list = new QComboBox{w};

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

    con(m, &Model::changed, &v, [=, &m] {
      setCard(card_list, m.getCardOut());
    });
    return w;
  }
};
#endif

#if __has_include(<pa_win_wasapi.h>)
class WASAPIFactory final : public QObject, public AudioFactory
{
  SCORE_CONCRETE("afcd9c64-0367-4fa1-b2bb-ee65b1c5e5a7")
public:
  std::vector<PortAudioCard> devices;

  WASAPIFactory() { rescan(); }

  ~WASAPIFactory() override {}

  void rescan()
  {
    devices.clear();
    PortAudioScope portaudio;

    devices.push_back(
        PortAudioCard{{}, {}, QObject::tr("No device"), -1, 0, 0, {}});
    for (int i = 0; i < Pa_GetHostApiCount(); i++)
    {
      auto hostapi = Pa_GetHostApiInfo(i);
      if (hostapi->type == PaHostApiTypeId::paWASAPI)
      {
        for (int card = 0; card < hostapi->deviceCount; card++)
        {
          auto dev_idx = Pa_HostApiDeviceIndexToDeviceIndex(i, card);
          auto dev = Pa_GetDeviceInfo(dev_idx);
          auto raw_name = QString::fromUtf8(Pa_GetDeviceInfo(dev_idx)->name);

          devices.push_back(PortAudioCard{"WASAPI",
                                          raw_name,
                                          raw_name,
                                          dev_idx,
                                          dev->maxInputChannels,
                                          dev->maxOutputChannels,
                                          hostapi->type});
        }
      }
    }
  }

  QString prettyName() const override { return QObject::tr("WASAPI"); }
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

  void setCard(QComboBox* combo, QString val)
  {
    auto dev_it = ossia::find_if(
        devices, [&](const PortAudioCard& d) { return d.raw_name == val; });
    if (dev_it != devices.end())
    {
      combo->setCurrentIndex(dev_it->out_index);
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

    auto card_list = new QComboBox{w};

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

    con(m, &Model::changed, &v, [=, &m] {
      setCard(card_list, m.getCardOut());
    });
    return w;
  }
};
#endif

#if __has_include(<pa_win_wmme.h>)

class MMEFactory final : public AudioFactory
{
  SCORE_CONCRETE("f5950e60-dac3-4254-bfb7-b94c96c679aa")
public:
  std::vector<PortAudioCard> devices;

  MMEFactory() { rescan(); }

  ~MMEFactory() override {}

  void rescan()
  {
    devices.clear();

    PortAudioScope portaudio;

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

          devices.push_back(PortAudioCard{"MME",
                                          raw_name,
                                          raw_name,
                                          dev_idx,
                                          dev->maxInputChannels,
                                          dev->maxOutputChannels,
                                          hostapi->type});
        }
      }
    }
  }

  QString prettyName() const override { return QObject::tr("MME"); }
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

  void setCardIn(QComboBox* combo, QString val)
  {
    auto dev_it = ossia::find_if(
        devices, [&](const PortAudioCard& d) { return d.raw_name == val; });
    if (dev_it != devices.end())
    {
      combo->setCurrentIndex(dev_it->in_index);
    }
  }
  void setCardOut(QComboBox* combo, QString val)
  {
    auto dev_it = ossia::find_if(
        devices, [&](const PortAudioCard& d) { return d.raw_name == val; });
    if (dev_it != devices.end())
    {
      combo->setCurrentIndex(dev_it->out_index);
    }
  }

  void updateSampleRates(
      QComboBox* rate,
      const PortAudioCard& input,
      const PortAudioCard& output)
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

      if (auto err
          = Pa_IsFormatSupported(nullptr, /*&iParams, */ &oParams, sr);
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
    auto buffersize = new QComboBox{w};

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
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardIn>(
              m, dev.raw_name);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultIn>(
              m, dev.inputChan);
          if (dev.hostapi != PaHostApiTypeId::paMME)
          {
            if (dev.out_index != -1
                && dev.out_index != card_out->currentIndex())
              card_out->setCurrentIndex(dev.out_index);
          }
        }
      };

      QObject::connect(
          card_in,
          SignalUtils::QComboBox_currentIndexChanged_int(),
          &v,
          [=](int i) {
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
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardOut>(
              m, dev.raw_name);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultOut>(
              m, dev.outputChan);
          if (dev.hostapi != PaHostApiTypeId::paMME)
          {
            if (dev.in_index != -1 && dev.in_index != card_in->currentIndex())
              card_in->setCurrentIndex(dev.in_index);
          }
        }
      };

      QObject::connect(
          card_out,
          SignalUtils::QComboBox_currentIndexChanged_int(),
          &v,
          [=](int i) {
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

    {
      lay->addRow(QObject::tr("Buffer size"), buffersize);

      updateRates();
    }
    con(m, &Model::changed, &v, [=, &m] {
      setCardIn(card_in, m.getCardIn());
      setCardOut(card_out, m.getCardOut());
    });
    return w;
  }
};
#endif

#if __has_include(<pa_linux_alsa.h>)
class ALSAFactory final : public QObject, public AudioFactory
{
  SCORE_CONCRETE("3533ee88-9a8d-486c-b20b-6c966cf4eaa0")
public:
  std::vector<PortAudioCard> devices;

  ALSAFactory() { rescan(); }

  ~ALSAFactory() override {}

  void rescan()
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

  QString prettyName() const override { return QObject::tr("ALSA"); }
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

  void setCard(QComboBox* combo, QString val)
  {
    auto dev_it = ossia::find_if(
        devices, [&](const PortAudioCard& d) { return d.raw_name == val; });
    if (dev_it != devices.end())
    {
      combo->setCurrentIndex(dev_it->out_index);
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

    auto card_list = new QComboBox{w};

    auto informations = new QLabel{w};

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
    con(m, &Model::changed, &v, [=, &m] {
      setCard(card_list, m.getCardOut());
    });
    return w;
  }
};
#endif

#endif
}
