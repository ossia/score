#pragma once
#include <Audio/AudioInterface.hpp>
#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/View.hpp>

#include <score/command/Dispatchers/SettingsCommandDispatcher.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <ossia/detail/config.hpp>

#include <ossia/audio/miniaudio_protocol.hpp>

#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>

#include <version>
#if OSSIA_ENABLE_MINIAUDIO

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

class GenericMiniAudioFactory
    : public QObject
    , public AudioFactory
{
  std::shared_ptr<ossia::miniaudio_context> m_context;

public:
  std::vector<MiniAudioCard> devices;

  virtual ~GenericMiniAudioFactory() override { }

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

  virtual bool compareDeviceId(const ma_device_id& id, const QString& str) const noexcept
      = 0;
  virtual bool
  compareDeviceId(const ma_device_id& id, std::string_view str) const noexcept
      = 0;
  virtual void setDeviceId(ma_device_id& id, const QString& str) const noexcept = 0;
  virtual QString deviceIdToString(const ma_device_id& id) const noexcept = 0;

  // When true, the factory exposes an extra "Default device" entry mapped to
  // the OS default device. The selection is stored as the "Default" sentinel
  // and make_engine passes a NULL miniaudio device id, so the backend follows
  // the OS default (with automatic rerouting on WASAPI) while running.
  virtual bool followsDefaultDevice() const noexcept { return false; }

  static constexpr int DefaultDeviceItemData = -1;

  int defaultDeviceChannels(ma_device_type tp) const noexcept
  {
    for(const auto& d : devices)
    {
      if(d.type == tp && d.info.isDefault && d.info.nativeDataFormatCount > 0)
        return d.info.nativeDataFormats[0].channels;
    }
    return 2;
  }

  void
  initialize(Audio::Settings::Model& set, const score::ApplicationContext& ctx) override
  {
    acquireContext();
    // set.setDefaultIn(2);
    // set.setDefaultOut(2);
    // set.setRate(48000);
    // set.setBufferSize(256);
    // set.changed();

    // Backends that can follow the OS default (WASAPI) default to that mode when
    // nothing has been configured yet, so playback/capture track the Windows
    // mixer default output/input out of the box.
    if(followsDefaultDevice() && set.getCardOut().isEmpty())
      set.setCardOut("Default");
    if(followsDefaultDevice() && set.getCardIn().isEmpty())
      set.setCardIn("Default");

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
          // auto fmt = d.info.nativeDataFormats[i];
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
          if(compareDeviceId(d.id, set.getCardIn()))
            user_in = &d;
          //  qDebug() << "capture" << d.info.isDefault;
          first_viable_in = &d;
          if(d.info.isDefault)
            default_in = &d;
        }
        if(d.type & ma_device_type::ma_device_type_playback)
        {
          if(compareDeviceId(d.id, set.getCardOut()))
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

    if(followsDefaultDevice() && set.getCardIn() == "Default")
    {
      const MiniAudioCard* ref = default_in ? default_in : first_viable_in;
      set.setDefaultIn(ref ? ref->info.nativeDataFormats[0].channels : 2);
    }
    else if(card_in)
    {
      set.setCardIn(deviceIdToString(card_in->id));
      set.setDefaultIn(card_in->info.nativeDataFormats[0].channels);
    }
    else
    {
      set.setCardIn(devices.front().name);
      set.setDefaultIn(0);
    }

    if(followsDefaultDevice() && set.getCardOut() == "Default")
    {
      // Keep the "Default device" sentinel so make_engine passes a NULL device
      // id and miniaudio follows the OS default output. Derive channels/rate
      // from the current OS default device.
      const MiniAudioCard* ref = default_out ? default_out : first_viable_out;
      if(ref)
      {
        set.setDefaultOut(ref->info.nativeDataFormats[0].channels);
        int current_rate = set.getRate();
        if(int fmt_count = ref->info.nativeDataFormatCount; fmt_count > 0)
        {
          auto* fmts = ref->info.nativeDataFormats;
          if(std::none_of(fmts, fmts + fmt_count, [current_rate](auto fmt) {
            return fmt.sampleRate == current_rate;
          }))
          {
            set.setRate(ref->info.nativeDataFormats[0].sampleRate);
          }
        }
      }
      else
      {
        set.setDefaultOut(2);
      }
    }
    else if(card_out)
    {
      set.setCardOut(deviceIdToString(card_out->id));
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

    setDeviceId(devices[0].id, "No device");

    ma_context_enumerate_devices(
        &m_context->context,
        [](ma_context* ctx, ma_device_type dev_type, const ma_device_info* dev_info,
           void* data) -> ma_bool32 {
      auto& self = *(GenericMiniAudioFactory*)data;
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
      if(compareDeviceId(dev.id, card_in))
        info_in = dev.id;
      if(compareDeviceId(dev.id, card_out))
        info_out = dev.id;
    }

    return std::make_shared<ossia::miniaudio_engine>(
        acquireContext(), "ossia score", info_in, info_out, set.getDefaultIn(),
        set.getDefaultOut(), set.getRate(), set.getBufferSize());
  }

  void setCard(QComboBox* combo, ma_device_type tp, QString val)
  {
    // "Default device" sentinel: select the dedicated combo entry.
    if(followsDefaultDevice() && val == "Default")
    {
      for(int i = 0; i < combo->count(); i++)
      {
        if(combo->itemData(i).toInt() == DefaultDeviceItemData)
        {
          combo->setCurrentIndex(i);
          return;
        }
      }
    }

    auto dev_it = ossia::find_if(devices, [&](const MiniAudioCard& d) {
      return compareDeviceId(d.id, val) && d.type == tp;
    });
    if(dev_it != devices.end())
    {
      int device_index = std::distance(devices.begin(), dev_it);
      for(int i = 0; i < combo->count(); i++)
      {
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

    // Optional "follow the OS default device" entry (e.g. WASAPI).
    if(followsDefaultDevice())
    {
      card_list_in->addItem(
          QObject::tr("Default device (follow system)"), DefaultDeviceItemData);
      card_list_out->addItem(
          QObject::tr("Default device (follow system)"), DefaultDeviceItemData);
    }

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

      auto update_dev_in = [this, &m, &m_disp](const MiniAudioCard& dev) {
        if(!compareDeviceId(dev.id, m.getCardIn()))
        {
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardIn>(
              m, deviceIdToString(dev.id));
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultIn>(
              m, dev.info.nativeDataFormats[0].channels);
        }
      };

      auto update_default_in = [this, &m, &m_disp]() {
        if(m.getCardIn() != "Default")
        {
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardIn>(
              m, QStringLiteral("Default"));
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultIn>(
              m, defaultDeviceChannels(ma_device_type_capture));
        }
      };

      QObject::connect(
          card_list_in, SignalUtils::QComboBox_currentIndexChanged_int(), &v,
          [this, card_list_in, update_dev_in, update_default_in](int i) {
        int data = card_list_in->itemData(i).toInt();
        if(data == DefaultDeviceItemData)
          update_default_in();
        else
          update_dev_in(devices[data]);
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

      auto update_dev_out = [this, &m, &m_disp](const MiniAudioCard& dev) {
        if(!compareDeviceId(dev.id, m.getCardOut()))
        {
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardOut>(
              m, deviceIdToString(dev.id));
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultOut>(
              m, dev.info.nativeDataFormats[0].channels);
        }
      };

      auto update_default_out = [this, &m, &m_disp]() {
        if(m.getCardOut() != "Default")
        {
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardOut>(
              m, QStringLiteral("Default"));
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultOut>(
              m, defaultDeviceChannels(ma_device_type_playback));
        }
      };

      QObject::connect(
          card_list_out, SignalUtils::QComboBox_currentIndexChanged_int(), &v,
          [this, card_list_out, update_dev_out, update_default_out](int i) {
        int data = card_list_out->itemData(i).toInt();
        if(data == DefaultDeviceItemData)
          update_default_out();
        else
          update_dev_out(devices[data]);
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

    con(m, &Model::changed, w, [this, card_list_in, card_list_out, &m] {
      setCard(card_list_in, ma_device_type_capture, m.getCardIn());
      setCard(card_list_out, ma_device_type_playback, m.getCardOut());
    });
    return w;
  }
};

}

#endif
