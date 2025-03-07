#pragma once
#include <ossia/detail/config.hpp>

#include <version>

#if defined(__APPLE__)
#include <Audio/AudioInterface.hpp>
#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/View.hpp>

#include <score/command/Dispatchers/SettingsCommandDispatcher.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <ossia/audio/miniaudio_protocol.hpp>

#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#if OSSIA_ENABLE_MINIAUDIO
#include <ossia/audio/miniaudio_protocol.hpp>

#include <magic_enum/magic_enum.hpp>
namespace Audio
{
struct MiniAudioCard
{
  MiniAudioCard() = default;
  MiniAudioCard(const MiniAudioCard&) = default;
  MiniAudioCard& operator=(const MiniAudioCard&) = default;
  MiniAudioCard(MiniAudioCard&&) noexcept = default;
  MiniAudioCard& operator=(MiniAudioCard&&) noexcept = default;
  MiniAudioCard(QString n, ma_device_type t, ma_device_id id)
      : name{n}
      , type{t}
      , id{id}
  {
  }
  QString name;
  ma_device_type type{};
  ma_device_id id{};
  ma_device_info info{};
};

class CoreAudioFactory final
    : public QObject
    , public AudioFactory
{
  SCORE_CONCRETE("85115103-694a-4a3b-9274-76ef47aec5a9")
  std::shared_ptr<ossia::miniaudio_context> m_context;

public:
  std::vector<MiniAudioCard> devices;

  CoreAudioFactory() { rescan(); }

  std::shared_ptr<ossia::miniaudio_context> acquireContext()
  {
    if(!m_context)
    {
      m_context = std::make_shared<ossia::miniaudio_context>();
      auto cfg = ma_context_config_init();
      cfg.threadPriority = ma_thread_priority_realtime;
      cfg.threadStackSize = 8388608;
      ma_context_init(nullptr, 0, &cfg, &m_context->context);
    }

    return m_context;
  }

  ~CoreAudioFactory() override { }
  bool available() const noexcept override { return true; }
  void
  initialize(Audio::Settings::Model& set, const score::ApplicationContext& ctx) override
  {
    acquireContext();
    // set.setDefaultIn(2);
    // set.setDefaultOut(2);
    // set.setRate(48000);
    // set.setBufferSize(256);
    // set.changed();

    const MiniAudioCard* user_in{};
    const MiniAudioCard* user_out{};

    const MiniAudioCard* default_in{};
    const MiniAudioCard* default_out{};

    const MiniAudioCard* first_viable_in{};
    const MiniAudioCard* first_viable_out{};

    // qDebug(Q_FUNC_INFO);
    // qDebug() << "looking for in:" << set.getCardIn();
    // qDebug() << "looking for out:" << set.getCardOut();
    for(auto& d : devices)
    {
      // qDebug() << "has device : " << d.info.name << d.info.nativeDataFormatCount;
      {
        for(int i = 0; i < d.info.nativeDataFormatCount; i++)
        {
          auto fmt = d.info.nativeDataFormats[i];
          // qDebug() << "Format: " << magic_enum::enum_name(fmt.format) << fmt.channels
          //          << fmt.flags << fmt.sampleRate;
        }
      }
      if(d.info.nativeDataFormatCount > 0 && d.info.nativeDataFormats[0].channels > 0)
      {
        // qDebug() << "device id:" << d.id.coreaudio
        //          << d.info.nativeDataFormats[0].channels;

        if(d.type & ma_device_type::ma_device_type_capture)
        {
          if(d.id.coreaudio == set.getCardIn())
            user_in = &d;
         //  qDebug() << "capture" << d.info.isDefault;
          first_viable_in = &d;
          if(d.info.isDefault)
            default_in = &d;
        }
        if(d.type & ma_device_type::ma_device_type_playback)
        {
          if(d.id.coreaudio == set.getCardOut())
            user_out = &d;
          // qDebug() << "playback" << d.info.isDefault;
          first_viable_out = &d;
          if(d.info.isDefault)
            default_out = &d;
        }
      }
    }

    const MiniAudioCard* card_in = user_in;
    if(!card_in)
      card_in = default_in;
    if(!card_in)
      card_in = first_viable_in;
    if(set.getCardIn().startsWith("No device"))
      card_in = nullptr;

    const MiniAudioCard* card_out = user_out;
    if(!card_out)
      card_out = default_out;
    if(!card_out)
      card_out = first_viable_out;
    if(set.getCardOut().startsWith("No device"))
      card_out = nullptr;

    if(card_in)
    {
      // qDebug() << Q_FUNC_INFO << card_in->id.coreaudio;
      set.setCardIn(card_in->id.coreaudio);
      set.setDefaultIn(card_in->info.nativeDataFormats[0].channels);
    }
    else
    {
      // qDebug() << Q_FUNC_INFO << "no input";
      set.setCardIn(devices.front().name);
      set.setDefaultIn(0);
    }

    if(card_out)
    {
      // qDebug() << Q_FUNC_INFO << card_out->id.coreaudio;
      set.setCardOut(card_out->id.coreaudio);
      set.setDefaultOut(card_out->info.nativeDataFormats[0].channels);

      int current_rate = set.getRate();
      if(int fmt_count = card_out->info.nativeDataFormatCount; fmt_count > 0)
      {
        auto* fmts = card_out->info.nativeDataFormats;
        if(std::none_of(fmts, fmts + fmt_count, [current_rate](auto fmt) {
          return fmt.sampleRate == current_rate;
        }))
        {
          set.setRate(card_out->info.nativeDataFormats[0].sampleRate);
        }
      }
    }
    else
    {
      // qDebug() << Q_FUNC_INFO << "no output";
      set.setCardOut(devices.front().name);
      set.setDefaultOut(0);
    }

    set.changed();
  }

  void rescan()
  {
    auto ctx = acquireContext();
    if(!ctx)
      return;
    devices.clear();

    devices.resize(1);
    devices[0].name = "No device";
    memset(&devices[0].id, 0, sizeof(devices[0].id));
    memset(&devices[0].info, 0, sizeof(devices[0].info));

    memcpy(&devices[0].id.coreaudio, "No device", strlen("No device"));

    ma_context_enumerate_devices(
        &m_context->context,
        [](ma_context* ctx, ma_device_type dev_type, const ma_device_info* dev_info,
           void* data) -> ma_bool32 {
      auto& self = *(CoreAudioFactory*)data;
      self.devices.emplace_back(dev_info->name, dev_type, dev_info->id);
      return 1;
    },
        this);

    for(std::size_t i = 1; i < devices.size(); i++)
    {
      auto& dev = devices[i];
      ma_context_get_device_info(&m_context->context, dev.type, &dev.id, &dev.info);
    }
  }

  QString prettyName() const override { return QObject::tr("CoreAudio"); }
  std::shared_ptr<ossia::audio_engine> make_engine(
      const Audio::Settings::Model& set, const score::ApplicationContext& ctx) override
  {
    ma_device_id info_in;
    ma_device_id info_out;
    memset(&info_in, 0, sizeof(info_in));
    memset(&info_out, 0, sizeof(info_out));

    auto card_in = set.getCardIn().toStdString();
    auto card_out = set.getCardOut().toStdString();

    for(auto& dev : this->devices)
    {
      if(dev.id.coreaudio == card_in)
        info_in = dev.id;
      if(dev.id.coreaudio == card_out)
        info_out = dev.id;
    }

    return std::make_shared<ossia::miniaudio_engine>(
        acquireContext(), "ossia score", info_in, info_out, set.getDefaultIn(),
        set.getDefaultOut(), set.getRate(), set.getBufferSize());
  }

  void setCard(QComboBox* combo, ma_device_type tp, QString val)
  {
    //qDebug() << Q_FUNC_INFO << "looking for: " << val << "in " << combo->count();
    auto dev_it
        = ossia::find_if(devices, [&, id = val.toStdString()](const MiniAudioCard& d) {
      // qDebug() << d.id.coreaudio << " == " << id << bool(d.id.coreaudio == id) << " ???";
      return d.id.coreaudio == id && d.type == tp;
    });
    if(dev_it != devices.end())
    {
      int device_index = std::distance(devices.begin(), dev_it);
      for(int i = 0; i < combo->count(); i++)
      {
        // qDebug() << device_index << i << combo->itemText(i) << combo->itemData(i);
        if(combo->itemData(i).toInt() == device_index)
        {
          combo->setCurrentIndex(i);
          return;
        }
      }
    }

    combo->setCurrentIndex(0);
  }

  QWidget* make_settings(
      Audio::Settings::Model& m, Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp, QWidget* parent) override
  {
    acquireContext();
    /* Not useful: hotplug does not seem to work
    struct UpdateWhenVisible : public QWidget
    {
    public:
      using QWidget::QWidget;

      bool event(QEvent* e) override
      {
        if(e->type() == QEvent::Show)
        {
          auto& self = score::AppContext().interfaces<Audio::AudioFactoryList>();
          auto cf = static_cast<CoreAudioFactory*>(
              self.get(CoreAudioFactory::static_concreteKey()));
          cf->rescan();
        }
        return QWidget::event(e);
      }
    };
    */

    auto w = new QWidget{parent};
    auto lay = new QFormLayout{w};

    auto card_list_in = new QComboBox{w};
    auto card_list_out = new QComboBox{w};

    // Disabled case
    card_list_in->addItem(devices.front().name + " (capture)", 0);
    card_list_out->addItem(devices.front().name + " (playback)", 0);

    // qDebug() << Q_FUNC_INFO << "Devices: ";
    for(std::size_t i = 1; i < devices.size(); i++)
    {

      auto& dev = devices[i];
      // qDebug() << i << dev.info.name << dev.id.coreaudio << dev.type;
      if(dev.info.nativeDataFormatCount > 0)
      {
        if(dev.info.nativeDataFormats[0].channels > 0)
        {
          if(dev.type == ma_device_type_capture)
          {
            card_list_in->addItem(dev.name, (int)i);
            // qDebug() << "adding to card list in" << i << dev.name;
          }
          if(dev.type == ma_device_type_playback)
            card_list_out->addItem(dev.name, (int)i);
        }
      }
    }

/*
    for(int i = 0; i < card_list_in->count(); i++)
      qDebug() << Q_FUNC_INFO << "card in " << i << card_list_in->itemText(i)
               << card_list_in->itemData(i);
    for(int i = 0; i < card_list_out->count(); i++)
      qDebug() << Q_FUNC_INFO << "card out " << i << card_list_out->itemText(i)
               << card_list_out->itemData(i);
               */
    using Model = Audio::Settings::Model;

    {
      lay->addRow(QObject::tr("Capture"), card_list_in);

      auto update_dev_in = [=, &m, &m_disp](const MiniAudioCard& dev) {
        if(dev.id.coreaudio != m.getCardIn())
        {
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardIn>(
              m, dev.id.coreaudio);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultIn>(
              m, dev.info.nativeDataFormats[0].channels);
        }
      };

      QObject::connect(
          card_list_in, SignalUtils::QComboBox_currentIndexChanged_int(), &v,
          [=](int i) {
        auto& device = devices[card_list_in->itemData(i).toInt()];
        update_dev_in(device);
      });

      if(m.getCardIn().isEmpty())
      {
        if(!devices.empty())
        {
          update_dev_in(devices.front());
        }
      }
      else
      {
        setCard(card_list_in, ma_device_type_capture, m.getCardIn());
      }
    }

    {
      lay->addRow(QObject::tr("Playback"), card_list_out);

      auto update_dev_out = [=, &m, &m_disp](const MiniAudioCard& dev) {
        if(dev.id.coreaudio != m.getCardOut())
        {
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardOut>(
              m, dev.id.coreaudio);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultOut>(
              m, dev.info.nativeDataFormats[0].channels);
        }
      };

      QObject::connect(
          card_list_out, SignalUtils::QComboBox_currentIndexChanged_int(), &v,
          [=](int i) {
        auto& device = devices[card_list_out->itemData(i).toInt()];
        update_dev_out(device);
      });

      if(m.getCardOut().isEmpty())
      {
        if(!devices.empty())
        {
          update_dev_out(devices.front());
        }
      }
      else
      {
        setCard(card_list_out, ma_device_type_playback, m.getCardOut());
      }
    }

    addBufferSizeWidget(*w, m, v);
    addSampleRateWidget(*w, m, v);

    con(m, &Model::changed, w, [=, &m] {
      setCard(card_list_in, ma_device_type_capture, m.getCardIn());
      setCard(card_list_out, ma_device_type_playback, m.getCardOut());
    });
    return w;
  }
};

}

#endif
#endif
