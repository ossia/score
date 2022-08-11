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
  Audio::Settings::View& m_view;

  score::SettingsCommandDispatcher& m_disp;
  QComboBox *card_list{}, *buffer_size{}, *rate{};
  QSpinBox* out_count{};
  QLabel* informations{};

  void setInfos(const AlsaCard& dev)
  {
    QString txt;
    txt += "Device: " + dev.pretty_name + "\n";
    txt += QString("Outputs: [%1 -> %2]\n")
               .arg(dev.outputRange.min)
               .arg(dev.outputRange.max);

    informations->setText(txt);
  }

  void updateDevice(const AlsaCard& dev)
  {
    auto& m = m_model;
    if(dev.raw_name != m.getCardOut())
    {
      using namespace Audio::Settings;
      m_disp.submitDeferredCommand<SetModelCardIn>(m, dev.raw_name);
      m_disp.submitDeferredCommand<SetModelCardOut>(m, dev.raw_name);
      m_disp.submitDeferredCommand<SetModelDefaultIn>(m, 0);
      m_disp.submitDeferredCommand<SetModelDefaultOut>(m, this->out_count->value());
      m_disp.submitDeferredCommand<SetModelRate>(m, this->rate->currentText().toInt());
      m_disp.submitDeferredCommand<SetModelBufferSize>(
          m, this->buffer_size->currentText().toInt());

      setInfos(dev);
    }
  }

  void rescanUI()
  {
    QObject::disconnect(
        card_list, SignalUtils::QComboBox_currentIndexChanged_int(), this,
        &ALSAWidget::on_deviceIndexChanged);

    card_list->clear();
    m_factory.rescan();

    auto& devices = m_factory.devices;

    card_list->addItem(devices.front().pretty_name, 0);

    // Normal devices
    for(std::size_t i = 1; i < devices.size(); i++)
    {
      auto& card = devices[i];
      card_list->addItem(card.pretty_name, (int)i);
    }

    if(m_model.getCardOut().isEmpty())
    {
      if(!devices.empty())
      {
        setCard(card_list, devices.front().raw_name);
      }
    }
    else
    {
      setCard(card_list, m_model.getCardOut());
    }

    QObject::connect(
        card_list, SignalUtils::QComboBox_currentIndexChanged_int(), this,
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
      ALSAFactory& fact, Audio::Settings::Model& m, Audio::Settings::View& v,
      score::SettingsCommandDispatcher& disp, QWidget* parent = nullptr)
      : QWidget{parent}
      , m_factory{fact}
      , m_model{m}
      , m_view{v}
      , m_disp{disp}
  {
    auto lay = new QFormLayout{this};
    auto rescan = new QPushButton{tr("Rescan"), this};

    card_list = new QComboBox{this};
    informations = new QLabel{this};
    out_count = new QSpinBox{this};
    {
      lay->addRow(QObject::tr("Output channels"), out_count);
      out_count->setRange(0, 1024);
      QObject::connect(
          out_count, SignalUtils::QSpinBox_valueChanged_int(), this, [this, &m](int i) {
            m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultOut>(m, i);
          });

      out_count->setValue(m.getDefaultOut());
    }

    fact.addBufferSizeWidget(*this, m, v);
    fact.addSampleRateWidget(*this, m, v);

    this->buffer_size = this->findChild<QComboBox*>("BufferSize");
    this->rate = this->findChild<QComboBox*>("Rate");
    using Model = Audio::Settings::Model;

    {
      lay->addRow(QObject::tr("Device"), card_list);
      lay->addRow(rescan);
      lay->addWidget(informations);
    }

    // Disabled case
    if(QString res = qgetenv("SCORE_DISABLE_ALSA"); res.isEmpty())
    {
      rescanUI();
    }
    connect(rescan, &QPushButton::clicked, this, &ALSAWidget::rescanUI);

    connect(
        card_list, qOverload<int>(&QComboBox::currentIndexChanged), this,
        [this, &m](int idx) {
      if(ossia::valid_index(idx, m_factory.devices))
      {
        auto& dev = m_factory.devices[idx];
        m_disp.submitDeferredCommand<Audio::Settings::SetModelCardOut>(m, dev.raw_name);
        updateCombos(dev);
      }
        });

    con(m, &Model::changed, this, [=, &m] { setCard(card_list, m.getCardOut()); });
  }

  void updateCombos(const AlsaCard& card)
  {
    {
      auto bs = this->buffer_size;
      int prev_bs = bs->currentText().toInt();
      bs->clear();

      {
        QSignalBlocker block{bs};
        for(int b : card.buffer_sizes)
        {
          bs->addItem(QString::number(b));
        }
      }

      if(int idx = bs->findText(QString::number(prev_bs)); idx >= 0)
        bs->setCurrentIndex(idx);
      else if(int idx = bs->findText(QString::number(m_model.getBufferSize())); idx >= 0)
        bs->setCurrentIndex(idx);
      this->m_view.BufferSizeChanged(bs->currentText().toInt());
    }

    {
      int prev_rate = rate->currentText().toInt();
      rate->clear();

      {
        QSignalBlocker block{rate};
        for(int b : card.rates)
        {
          rate->addItem(QString::number(b));
        }
      }

      if(int idx = rate->findText(QString::number(prev_rate)); idx >= 0)
        rate->setCurrentIndex(idx);
      else if(int idx = rate->findText(QString::number(m_model.getRate())); idx >= 0)
        rate->setCurrentIndex(idx);
      this->m_view.RateChanged(rate->currentText().toInt());
    }

    {
      int prev_out_count = out_count->value();
      out_count->setRange(card.outputRange.min, card.outputRange.max);
      out_count->setValue(
          ossia::clamp(prev_out_count, card.outputRange.min, card.outputRange.max));
    }

    setInfos(card);
  }

  void setCard(QComboBox* combo, QString val)
  {
    auto dev_it = ossia::find_if(
        m_factory.devices, [&](const AlsaCard& d) { return d.raw_name == val; });
    if(dev_it != m_factory.devices.end())
    {
      const int idx = std::distance(m_factory.devices.begin(), dev_it);
      combo->setCurrentIndex(idx);

      updateCombos(*dev_it);
    }
  }
};

ALSAFactory::ALSAFactory()
{
  rescan();
}

ALSAFactory::~ALSAFactory() { }

void ALSAFactory::initialize(
    Audio::Settings::Model& set, const score::ApplicationContext& ctx)
{
  auto device = ossia::find_if(
      devices, [&](const AlsaCard& dev) { return dev.raw_name == set.getCardOut(); });

  if(device == devices.end())
  {
    for(const QString& default_device :
        {"default", "sysdefault", "pipewire", "pulse", "jack"})
    {
      auto default_dev = ossia::find_if(
          devices, [&](const AlsaCard& dev) { return dev.raw_name == default_device; });
      if(default_dev != devices.end())
      {
        qDebug() << "ALSA: Default settings not found, trying to set a default device: ";
        set.setCardIn(default_dev->raw_name);
        set.setCardOut(default_dev->raw_name);
        int num_in_chans = ossia::clamp(
            set.getDefaultIn(), default_dev->inputRange.min,
            default_dev->inputRange.max);

        int num_out_chans = ossia::clamp(
            set.getDefaultOut(), default_dev->outputRange.min,
            default_dev->outputRange.max);
        if(num_out_chans > 2)
          num_out_chans = 2;

        set.setDefaultIn(num_in_chans);
        set.setDefaultOut(num_out_chans);

        if(ossia::contains(default_dev->rates, 48000.) || default_dev->rates.empty())
          set.setRate(48000.);
        else
          set.setRate(default_dev->rates.front());

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
  catch(...)
  {
    return false;
  }
}
void ALSAFactory::rescan()
{
  devices.clear();

  devices.push_back(AlsaCard{{}, QObject::tr("No device"), 0, 0, {}});

  auto& snd = ossia::libasound::instance();

  void** hints{};
  auto err = snd.device_name_hint(-1, "pcm", &hints);
  if(err != 0)
  {
    return;
  }

  auto n = hints;
  while(*n)
  {
    if(auto name = snd.device_name_get_hint(*n, "NAME"); strcmp("null", name))
    {
      AlsaCard card;
      card.raw_name = name;
      card.pretty_name = name;

      snd_pcm_t* pcm{};
      if(int err = snd.pcm_open(&pcm, name, SND_PCM_STREAM_PLAYBACK, 0); err >= 0)
      {
        snd_pcm_hw_params_t* hw_params{};
        snd_alloca(&hw_params, snd, pcm_hw_params);
        snd.pcm_hw_params_any(pcm, hw_params);

        // Get min and max of various properties
        // unimplemented for input channel count for now
        card.inputRange = {0, 0};

        // output channel count
        {
          unsigned int min{}, max{};
          snd.pcm_hw_params_get_channels_min(hw_params, &min);
          snd.pcm_hw_params_get_channels_max(hw_params, &max);
          card.outputRange.min = min;
          card.outputRange.max = max;
        }

        // sampling rate range
        {
          unsigned int min{}, max{};
          snd.pcm_hw_params_get_rate_min(hw_params, &min, 0);
          snd.pcm_hw_params_get_rate_max(hw_params, &max, 0);
          for(unsigned int r : {22050, 32000, 44100, 48000, 88200, 96000, 192000})
            if(r >= min && r <= max)
              card.rates.push_back(r);
        }

        // buffer size range
        {
          snd_pcm_uframes_t min{}, max{};
          snd.pcm_hw_params_get_period_size_min(hw_params, &min, 0);
          snd.pcm_hw_params_get_period_size_max(hw_params, &max, 0);
          for(snd_pcm_uframes_t r :
              {16, 32, 48, 64, 128, 256, 512, 1024, 2048, 4096, 8192})
            if(r >= min && r <= max)
              card.buffer_sizes.push_back(r);
        }

        snd.pcm_close(pcm);
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

std::shared_ptr<ossia::audio_engine> ALSAFactory::make_engine(
    const Audio::Settings::Model& set, const score::ApplicationContext& ctx)
{
  return std::make_shared<ossia::alsa_engine>(
      set.getCardIn().toStdString(), set.getCardOut().toStdString(), set.getDefaultIn(),
      set.getDefaultOut(), set.getRate(), set.getBufferSize());
}

QWidget* ALSAFactory::make_settings(
    Audio::Settings::Model& m, Audio::Settings::View& v,
    score::SettingsCommandDispatcher& m_disp, QWidget* parent)
{
  return new ALSAWidget{*this, m, v, m_disp, parent};
}

#endif

}
