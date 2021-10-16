#include <Audio/ALSAInterface.hpp>
#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/View.hpp>

#include <score/command/Dispatchers/SettingsCommandDispatcher.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

namespace Audio
{
#if defined(OSSIA_AUDIO_ALSA)

class ALSAWidget : public QWidget
{
  ALSAFactory& m_factory;
  Audio::Settings::Model& m_model;

  score::SettingsCommandDispatcher& m_disp;
  QComboBox* card_list{};
  QLabel* informations{};

  void setInfos(const AlsaCard& dev)
  {
    informations->setText(tr("Inputs:\t%1\nOutputs:\t%2\nRate:\t%3")
                              .arg(dev.inputChan)
                              .arg(dev.outputChan)
                              .arg(dev.rate));
  }

  void updateDevice(const AlsaCard& dev)
  {
    auto& m = m_model;
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
      m_disp.submitDeferredCommand<Audio::Settings::SetModelRate>(m, dev.rate);

      setInfos(dev);
    }
  }

  void rescanUI()
  {

    QObject::disconnect(
        card_list,
        SignalUtils::QComboBox_currentIndexChanged_int(),
        this,
        &ALSAWidget::on_deviceIndexChanged);

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

    if (m_model.getCardOut().isEmpty())
    {
      if (!devices.empty())
      {
        updateDevice(devices.front());
      }
    }
    else
    {
      setCard(card_list, m_model.getCardOut());
    }

    QObject::connect(
        card_list,
        SignalUtils::QComboBox_currentIndexChanged_int(),
        this,
        &ALSAWidget::on_deviceIndexChanged);
  }

  void on_deviceIndexChanged(int i)
  {
    auto& devices = m_factory.devices;
    auto& device = devices[card_list->itemData(i).toInt()];
    updateDevice(device);
  }

public:
  ALSAWidget(
      ALSAFactory& fact,
      Audio::Settings::Model& m,
      Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp,
      QWidget* parent = nullptr)
      : QWidget{parent}
      , m_factory{fact}
      , m_model{m}
      , m_disp{m_disp}
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
      QTimer::singleShot(1000, this, [this] { rescanUI(); });
    }
    connect(rescan, &QPushButton::clicked, this, &ALSAWidget::rescanUI);

    fact.addBufferSizeWidget(*this, m, v);
    fact.addSampleRateWidget(*this, m, v);

    using Model = Audio::Settings::Model;

    {
      lay->addRow(QObject::tr("Device"), card_list);
      lay->addRow(rescan);
      lay->addWidget(informations);
      std::size_t dev_idx
          = card_list->itemData(card_list->currentIndex()).toInt();
      if (dev_idx < devices.size())
      {
        setInfos(devices[dev_idx]);
      }
    }
    con(m, &Model::changed, this, [=, &m] {
      setCard(card_list, m.getCardOut());
    });
  }
  void setCard(QComboBox* combo, QString val)
  {
    auto dev_it
        = ossia::find_if(m_factory.devices, [&](const AlsaCard& d) {
            return d.raw_name == val;
          });
    if (dev_it != m_factory.devices.end())
    {
      combo->setCurrentIndex(dev_it->out_index);
    }
  }
};

ALSAFactory::ALSAFactory()
{
  rescan();
}

ALSAFactory::~ALSAFactory() { }

void ALSAFactory::initialize(
    Audio::Settings::Model& set,
    const score::ApplicationContext& ctx)
{
  auto device = ossia::find_if(devices, [&](const AlsaCard& dev) {
    return dev.raw_name == set.getCardOut();
  });

  if (device == devices.end())
  {
    for (const QString& default_device :
        {"default", "sysdefault", "pipewire", "pulse", "jack"})
    {
      auto default_dev
          = ossia::find_if(devices, [&](const AlsaCard& dev) {
              return dev.raw_name == default_device;
            });
      if (default_dev != devices.end())
      {
        set.setCardIn(default_dev->raw_name);
        set.setCardOut(default_dev->raw_name);
        int num_in_chans = ossia::clamp(set.getDefaultIn(), 0, default_dev->inputChan);
        int min_out_chans = std::min(2, default_dev->outputChan);
        int num_out_chans = ossia::clamp(set.getDefaultOut(), min_out_chans, default_dev->outputChan);
        set.setDefaultIn(num_in_chans);
        set.setDefaultOut(num_out_chans);
        set.setRate(default_dev->rate);
        set.changed();
        return;
      }
    }
  }
}

bool ALSAFactory::available() const noexcept
{
  try
  {
    ossia::libasound::instance();
    return true;
  }
  catch (...)
  {
    return false;
  }
}
void ALSAFactory::rescan()
{
  devices.clear();

  devices.push_back(
      AlsaCard{ {}, QObject::tr("No device"), 0, 0, {}});

  auto& snd = ossia::libasound::instance();

  void** hints{};
  auto err = snd.device_name_hint(-1, "pcm", &hints);
  if (err != 0) {
    return;
  }

  auto n = hints;
  while (*n) {
    if(auto name = snd_device_name_get_hint(*n, "NAME"); strcmp("null", name))
    {
      AlsaCard card;
      card.raw_name = name;
      card.pretty_name = name;

      snd_pcm_t* pcm{};
      if (int err = snd_pcm_open (&pcm, name, SND_PCM_STREAM_PLAYBACK, 0); err >= 0)
      {
        snd_pcm_hw_params_t* hw_params{};
        snd_alloca(&hw_params, snd, pcm_hw_params);
        snd.pcm_hw_params_any(pcm, hw_params);

        // Get min and max number of channels
        unsigned int min{}, max{};
        snd.pcm_hw_params_get_channels_min(hw_params, &min);
        snd.pcm_hw_params_get_channels_max(hw_params, &max);
        card.inputChan = 0;
        card.outputChan = max;

        unsigned int rate{};
        snd.pcm_hw_params_get_rate(hw_params, &rate, 0);
        card.rate = rate;

        snd_pcm_close(pcm);

        devices.push_back(card);
      }

      free(name);
    }
    ++n;
  }

  snd.device_name_free_hint(hints);
}

QString ALSAFactory::prettyName() const
{
  return QObject::tr("ALSA (raw, output only)");
}

std::unique_ptr<ossia::audio_engine> ALSAFactory::make_engine(
    const Audio::Settings::Model& set,
    const score::ApplicationContext& ctx)
{
  return std::make_unique<ossia::alsa_engine>(
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
