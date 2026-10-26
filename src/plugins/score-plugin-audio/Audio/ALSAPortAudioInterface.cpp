#include <Audio/ALSAPortAudioInterface.hpp>
#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/View.hpp>

#include <score/command/Dispatchers/SettingsCommandDispatcher.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTimer>

#if __has_include(<pa_linux_alsa.h>)
#include <pa_linux_alsa.h>
#endif

#include <map>
#include <optional>
#include <tuple>

namespace Audio
{
#if defined(OSSIA_AUDIO_PORTAUDIO) && __has_include(<pa_linux_alsa.h>)

class ALSAPortAudioWidget : public QWidget
{
  // Initializing PortAudio enumerates every ALSA device: once per panel, and
  // again on Rescan. PortAudio keeps its device list while any scope or
  // PortAudio engine is alive, so Rescan only sees new devices when the
  // engine runs another driver.
  std::optional<PortAudioScope> m_portaudio{std::in_place};
  ALSAPortAudioFactory& m_factory;
  Audio::Settings::Model& m_model;
  Audio::Settings::View& m_view;
  score::SettingsCommandDispatcher& m_disp;

  QComboBox* m_out{};
  QComboBox* m_in{};
  QSpinBox* m_outCount{};
  QSpinBox* m_inCount{};
  QComboBox* m_rate{};
  QLabel* m_info{};

  // Pending values: the model only changes when the dialog is accepted.
  QString m_cardOut;
  QString m_cardIn;

  // Probing a rate opens the device: each combination is probed once.
  std::map<std::tuple<QString, QString, int, int>, std::vector<int>> m_rates;

public:
  ALSAPortAudioWidget(
      ALSAPortAudioFactory& fact, Audio::Settings::Model& m, Audio::Settings::View& v,
      score::SettingsCommandDispatcher& disp, QWidget* parent = nullptr)
      : QWidget{parent}
      , m_factory{fact}
      , m_model{m}
      , m_view{v}
      , m_disp{disp}
      , m_cardOut{m.getCardOut()}
      , m_cardIn{m.getCardIn()}
  {
    auto lay = new QFormLayout{this};
    m_out = new QComboBox{this};
    m_in = new QComboBox{this};
    m_outCount = new QSpinBox{this};
    m_inCount = new QSpinBox{this};
    lay->addRow(tr("Output device"), m_out);
    lay->addRow(tr("Input device"), m_in);
    lay->addRow(tr("Output channels"), m_outCount);
    lay->addRow(tr("Input channels"), m_inCount);
    m_rate = fact.addSampleRateWidget(*this, m, v);
    fact.addBufferSizeWidget(*this, m, v);
    auto rescan = new QPushButton{tr("Rescan"), this};
    lay->addRow(rescan);
    m_info = new QLabel{this};
    m_info->setWordWrap(true);
    lay->addRow(m_info);

    m_outCount->setValue(m.getDefaultOut());
    m_inCount->setValue(m.getDefaultIn());

    if(qEnvironmentVariableIsEmpty("SCORE_DISABLE_ALSA"))
      fillDevices();

    connect(rescan, &QPushButton::clicked, this, [this] {
      m_portaudio.reset();
      m_portaudio.emplace();
      m_factory.rescan();
      m_rates.clear();
      fillDevices();
    });
    connect(m_out, SignalUtils::QComboBox_currentIndexChanged_int(), this, [this](int) {
      m_cardOut = m_out->currentData().toString();
      m_disp.submitDeferredCommand<Audio::Settings::SetModelCardOut>(m_model, m_cardOut);
      updateControls();
    });
    connect(m_in, SignalUtils::QComboBox_currentIndexChanged_int(), this, [this](int) {
      m_cardIn = m_in->currentData().toString();
      m_disp.submitDeferredCommand<Audio::Settings::SetModelCardIn>(m_model, m_cardIn);
      updateControls();
    });
    connect(m_outCount, SignalUtils::QSpinBox_valueChanged_int(), this, [this](int i) {
      m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultOut>(m_model, i);
      updateRates();
    });
    connect(m_inCount, SignalUtils::QSpinBox_valueChanged_int(), this, [this](int i) {
      m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultIn>(m_model, i);
      updateRates();
    });
  }

private:
  void fillDevices()
  {
    QSignalBlocker b1{m_out}, b2{m_in};
    m_out->clear();
    m_in->clear();
    m_out->addItem(tr("No device"), QString{});
    m_in->addItem(tr("No device"), QString{});
    for(const auto& dev : m_factory.devices)
    {
      if(dev.raw_name.isEmpty())
        continue;
      if(dev.outputChan > 0)
        m_out->addItem(dev.raw_name, dev.raw_name);
      if(dev.inputChan > 0)
        m_in->addItem(dev.raw_name, dev.raw_name);
    }
    select(m_out, m_cardOut, false);
    select(m_in, m_cardIn, true);
    updateControls();
  }

  void select(QComboBox* combo, const QString& name, bool input)
  {
    // The saved name may carry an older card index: show the device it is.
    QString shown = name;
    if(auto dev = m_factory.find(name))
    {
      // The device may be saved for a direction it does not have, or under
      // another card index: store what is shown.
      auto& card = input ? m_cardIn : m_cardOut;
      const bool usable = (input ? dev->inputChan : dev->outputChan) > 0;
      card = usable ? dev->raw_name : QString{};
      if(card != name)
      {
        if(input)
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardIn>(m_model, card);
        else
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardOut>(m_model, card);
      }
      if(!usable)
      {
        combo->setCurrentIndex(0);
        return;
      }
      shown = dev->raw_name;
    }
    int idx = combo->findData(shown);
    if(idx < 0 && !name.isEmpty())
    {
      combo->addItem(tr("%1 (not found)").arg(name), name);
      idx = combo->count() - 1;
    }
    combo->setCurrentIndex(std::max(idx, 0));
  }

  void updateControls()
  {
    auto channels = [](QSpinBox* sb, const PortAudioCard* dev, bool used) {
      QSignalBlocker block{sb};
      const int prev = sb->value();
      sb->setRange(0, !used ? 0 : dev ? std::max(dev->inputChan, dev->outputChan) : 64);
      sb->setValue(std::clamp(prev, sb->minimum(), sb->maximum()));
    };
    auto out = m_factory.find(m_cardOut);
    auto in = m_factory.find(m_cardIn);
    channels(m_outCount, out, !m_cardOut.isEmpty());
    if(out)
      m_outCount->setMaximum(out->outputChan);
    channels(m_inCount, in, !m_cardIn.isEmpty());
    if(in)
      m_inCount->setMaximum(in->inputChan);
    if(m_outCount->value() == 0 && out)
      m_outCount->setValue(std::min(2, out->outputChan));
    m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultOut>(
        m_model, m_outCount->value());
    m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultIn>(
        m_model, m_inCount->value());
    updateRates();
  }

  //! The rates PortAudio accepts for these devices and channel counts. A
  //! device that is in use, by score or another program, cannot be asked.
  void updateRates()
  {
    auto index_of = [](const QString& name, bool input) -> PaDeviceIndex {
      if(name.isEmpty())
        return paNoDevice;
      const auto api = Pa_HostApiTypeIdToHostApiIndex(paALSA);
      for(int i = 0; i < Pa_GetDeviceCount(); i++)
        if(auto d = Pa_GetDeviceInfo(i); d->hostApi == api && name == d->name
                                         && (input ? d->maxInputChannels
                                                   : d->maxOutputChannels)
                                                > 0)
          return i;
      return paNoDevice;
    };
    PaStreamParameters in{
        index_of(m_cardIn, true), m_inCount->value(), paFloat32 | paNonInterleaved, 0.01,
        nullptr};
    PaStreamParameters out{
        index_of(m_cardOut, false), m_outCount->value(), paFloat32 | paNonInterleaved,
        0.01, nullptr};
    const bool use_in = in.device != paNoDevice && in.channelCount > 0;
    const bool use_out = out.device != paNoDevice && out.channelCount > 0;

    const int current
        = m_rate->currentText().toInt() > 0 ? m_rate->currentText().toInt() : m_model.getRate();
    std::vector<int> rates;
    bool busy = false;
    if(use_in || use_out)
    {
      const auto key = std::make_tuple(
          use_in ? m_cardIn : QString{}, use_out ? m_cardOut : QString{},
          use_in ? in.channelCount : 0, use_out ? out.channelCount : 0);
      if(auto it = m_rates.find(key); it != m_rates.end())
      {
        rates = it->second;
      }
      else
      {
        // Each probe opens the device; a busy one is retried for a second by
        // default.
        PaAlsa_SetRetriesBusy(0);
        for(int r : {22050, 32000, 44100, 48000, 88200, 96000, 176400, 192000})
        {
          auto err
              = Pa_IsFormatSupported(use_in ? &in : nullptr, use_out ? &out : nullptr, r);
          if(err == paFormatIsSupported)
            rates.push_back(r);
          else if(err == paDeviceUnavailable)
          {
            busy = true;
            rates.clear();
            break;
          }
        }
        PaAlsa_SetRetriesBusy(100);
        // A busy device is asked again later.
        if(!busy)
          m_rates[key] = rates;
      }
    }
    if(rates.empty())
      rates.push_back(current);

    {
      QSignalBlocker block{m_rate};
      m_rate->clear();
      for(int r : rates)
        m_rate->addItem(QString::number(r), r);
      int idx = m_rate->findData(current);
      if(idx < 0)
        idx = std::max(0, m_rate->findData(48000));
      m_rate->setCurrentIndex(idx);
    }
    m_view.RateChanged(m_rate->currentData().toInt());

    QStringList lines;
    if(busy)
      lines << tr("A device is in use, by score or another program: its rates "
                  "cannot be listed.");
    if(use_in && use_out && m_cardIn != m_cardOut)
      lines << tr("Input and output are different devices. If they are on different "
                  "cards, their clocks differ: long sessions will click every few "
                  "minutes.");
    m_info->setText(lines.join("\n"));
  }
};

ALSAPortAudioFactory::ALSAPortAudioFactory() { }

const PortAudioCard* ALSAPortAudioFactory::find(const QString& name) const noexcept
{
  if(name.isEmpty())
    return nullptr;
  auto without_index = [](const QString& n) {
    static const QRegularExpression index{R"( \(hw:\d+,\d+\)$)"};
    return QString{n}.remove(index);
  };
  const PortAudioCard* fallback{};
  for(const auto& dev : devices)
  {
    if(dev.raw_name == name)
      return &dev;
    if(!fallback && without_index(dev.raw_name) == without_index(name))
      fallback = &dev;
  }
  return fallback;
}

ALSAPortAudioFactory::~ALSAPortAudioFactory() { }

void ALSAPortAudioFactory::initialize(
    Audio::Settings::Model& set, const score::ApplicationContext& ctx)
{
  if(!m_scanned)
    rescan();

  auto device = find(set.getCardOut());
  if(!device || device->hostapi == paInDevelopment)
  {
    // FIXME check if pipewire / pulse / jack / ... are running
    for(const QString& default_device :
        {"default", "sysdefault", "pipewire", "pulse", "jack"})
    {
      auto default_dev = ossia::find_if(devices, [&](const PortAudioCard& dev) {
        return dev.raw_name == default_device;
      });
      if(default_dev != devices.end())
      {
        set.setCardIn(default_dev->raw_name);
        set.setCardOut(default_dev->raw_name);

        int num_in_chans = ossia::clamp(set.getDefaultIn(), 0, default_dev->inputChan);
        int min_out_chans = std::min(2, default_dev->outputChan);
        int num_out_chans
            = ossia::clamp(set.getDefaultOut(), min_out_chans, default_dev->outputChan);
        set.setDefaultIn(num_in_chans);
        set.setDefaultOut(num_out_chans);
        set.setRate(default_dev->rate);
        set.changed();
        return;
      }
    }
  }
}

void ALSAPortAudioFactory::rescan()
{
  m_scanned = true;
  devices.clear();
  PortAudioScope portaudio;

  devices.push_back(PortAudioCard{{}, {}, QObject::tr("No device"), -1, 0, 0, {}});
  for(int i = 0; i < Pa_GetHostApiCount(); i++)
  {
    auto hostapi = Pa_GetHostApiInfo(i);
    if(hostapi->type == PaHostApiTypeId::paALSA)
    {
      for(int card = 0; card < hostapi->deviceCount; card++)
      {
        auto dev_idx = Pa_HostApiDeviceIndexToDeviceIndex(i, card);
        auto dev = Pa_GetDeviceInfo(dev_idx);
        auto raw_name = QString::fromUtf8(Pa_GetDeviceInfo(dev_idx)->name);
        auto pretty_name = raw_name;
        if(dev->maxInputChannels == 0)
        {
          pretty_name = tr("(Output) ") + pretty_name;
        }
        else if(dev->maxOutputChannels == 0)
        {
          pretty_name = tr("(Input) ") + pretty_name;
        }

        devices.push_back(PortAudioCard{
            "ALSA", raw_name, pretty_name, dev_idx, dev->maxInputChannels,
            dev->maxOutputChannels, hostapi->type, dev->defaultSampleRate});
      }
    }
  }
}

QString ALSAPortAudioFactory::prettyName() const
{
  return QObject::tr("ALSA (PortAudio)");
}

std::shared_ptr<ossia::audio_engine> ALSAPortAudioFactory::make_engine(
    const Audio::Settings::Model& set, const score::ApplicationContext& ctx)
{
  return std::make_shared<ossia::portaudio_engine>(
      "ossia score", set.getCardIn().toStdString(), set.getCardOut().toStdString(),
      set.getDefaultIn(), set.getDefaultOut(), set.getRate(), set.getBufferSize(),
      paALSA);
}

QWidget* ALSAPortAudioFactory::make_settings(
    Audio::Settings::Model& m, Audio::Settings::View& v,
    score::SettingsCommandDispatcher& m_disp, QWidget* parent)
{
  if(!m_scanned)
    rescan();
  return new ALSAPortAudioWidget{*this, m, v, m_disp, parent};
}

#endif
}
