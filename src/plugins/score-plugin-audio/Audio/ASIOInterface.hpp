#pragma once
#include <ossia/detail/config.hpp>
#include <Audio/AudioInterface.hpp>
#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/View.hpp>

#include <score/command/Dispatchers/SettingsCommandDispatcher.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/MessageBox.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>

#if defined(OSSIA_ENABLE_ASIO)
#include <ossia/audio/asio_protocol.hpp>

#include <exception>
#include <functional>

extern AsioDrivers* asioDrivers;
namespace Audio
{
//! Settings page that refreshes the device list when it actually becomes
//! visible.
//!
//! score builds every settings widget during startup, long before the user
//! opens the dialog, so refreshing from make_settings() itself would put the
//! driver-loading probe straight back onto the startup path -- which is the very
//! cost we are trying to avoid.
struct ASIOSettingsWidget final : QWidget
{
  using QWidget::QWidget;

  std::function<void()> on_show;

  void showEvent(QShowEvent* ev) override
  {
    QWidget::showEvent(ev);
    if(on_show)
      on_show();
  }
};

struct ASIOCard
{
  //! Driver name as the ASIO SDK reports it. This is the identity: it is what
  //! gets stored in the settings and handed to asio_engine, so it must stay
  //! verbatim. Anything user-facing goes through displayName().
  QString name;
  int driver_index{-1};
  int inputChan{};
  int outputChan{};

  //! Empty while the driver initializes fine; otherwise a short reason, shown
  //! in parentheses after the name in the device list.
  QString unavailable;

  //! Whether the driver has been loaded once to read the two fields above.
  //! Until then the channel counts are unknown and no reason is displayed.
  bool probed{};

  QString displayName() const
  {
    if(unavailable.isEmpty())
      return name;
    return QStringLiteral("%1 (%2)").arg(name, unavailable);
  }
};

class NativeASIOFactory final
    : public QObject
    , public AudioFactory
{
  SCORE_CONCRETE("2d21a3aa-f108-4e05-a3a0-8fd6b150bda5")
public:
  std::vector<ASIOCard> devices;

  NativeASIOFactory() { rescan(); }
  ~NativeASIOFactory() override { }

  bool available() const noexcept override { return !devices.empty(); }

  void
  initialize(Audio::Settings::Model& set, const score::ApplicationContext& ctx) override
  {
    rescan();

    if(!set.getCardOut().isEmpty())
    {
      // Check that the saved device still exists
      auto it = std::find_if(
          devices.begin(), devices.end(),
          [&](const ASIOCard& d) { return d.name == set.getCardOut(); });
      if(it != devices.end())
        return;
    }

    // Auto-select first device
    if(devices.size() > 1)
    {
      auto& dev = devices[1]; // [0] is "No device"
      set.setCardIn(dev.name);
      set.setCardOut(dev.name);
      set.setDefaultIn(dev.inputChan);
      set.setDefaultOut(dev.outputChan);
    }
  }

  //! Short, user-facing reason why a driver would not initialize. Shown after
  //! the device name in the list.
  static QString reasonFor(ASIOError err)
  {
    switch(err)
    {
      case ASE_NotPresent:
        // By far the common case: the interface is simply not plugged in.
        return QObject::tr("device not connected");
      case ASE_HWMalfunction:
        return QObject::tr("hardware malfunction");
      case ASE_NoMemory:
        return QObject::tr("out of memory");
      case ASE_NoClock:
        return QObject::tr("no clock");
      case ASE_InvalidMode:
        return QObject::tr("device busy or in a bad mode");
      default:
        return QObject::tr("unavailable");
    }
  }

  //! Lists the installed drivers and probes each one once.
  //!
  //! rescan() is reached three times while score starts up (this factory's
  //! constructor, Model::initDriver and ApplicationPlugin::initialize). Listing
  //! the drivers is a cheap registry walk, but probing them is not: each probe
  //! loads the driver's DLL through COM, which measured ~30 ms per driver here
  //! and costs more once real hardware answers. Doing all of that three times
  //! added roughly a third of a second to startup before the GUI appeared, so
  //! the result is now computed once and reused.
  //!
  //! \param force re-list and re-probe even if we already have results. Used
  //! when the user is looking at the device list, so hardware plugged in since
  //! startup shows up.
  void rescan(bool force = false)
  {
    if(m_scanned && !force)
    {
      // The list is known, but a driver can still be unprobed: probing is
      // skipped while an engine streams, so a scan that happened then left gaps.
      // Filling them now is free once everything has been probed.
      for(auto& dev : devices)
        probe(dev);
      return;
    }

    const std::vector<ASIOCard> previous = std::move(devices);

    devices.clear();
    devices.push_back(ASIOCard{QObject::tr("No device"), -1, 0, 0});

    try
    {
      for(auto& card : ossia::asio_engine::enumerate_drivers())
      {
        ASIOCard dev{QString::fromStdString(card.name), card.driver_index};

        // Always carry over what an earlier scan learned. On a forced rescan we
        // clear `probed` so the driver gets queried again, but keep the old
        // values as a fallback: if an engine turns out to be streaming, probe()
        // declines and we would otherwise be left with a blank list.
        auto it = std::find_if(
            previous.begin(), previous.end(),
            [&](const ASIOCard& d) { return d.name == dev.name; });
        if(it != previous.end())
        {
          dev.inputChan = it->inputChan;
          dev.outputChan = it->outputChan;
          dev.unavailable = it->unavailable;
          dev.probed = it->probed && !force;
        }

        devices.push_back(std::move(dev));
      }

      // Only mark the list as known once enumeration actually succeeded,
      // otherwise a transient failure would be cached forever.
      m_scanned = true;

      for(auto& dev : devices)
        probe(dev);

      ossia::asio_diagnostics::log() << "enumeration finished: " << (devices.size() - 1)
                                     << " driver(s) available\n";
    }
    catch(const std::exception& e)
    {
      ossia::asio_diagnostics::log() << "enumeration aborted: " << e.what() << '\n';
    }
    catch(...)
    {
      ossia::asio_diagnostics::log() << "enumeration aborted: unknown exception\n";
    }
  }

  //! Loads a driver to read its channel counts and find out whether its
  //! hardware answers. This is the expensive half of a scan, so it happens at
  //! most once per driver.
  //!
  //! Never probe while an engine streams: AsioDrivers::loadDriver() releases
  //! whatever driver is currently loaded, which would cut the running engine
  //! off from its hardware.
  void probe(ASIOCard& card)
  {
    if(card.probed || card.driver_index < 0)
      return;

    const std::string active = ossia::asio_engine::active_driver();
    if(!active.empty())
    {
      if(card.name.toStdString() == active)
      {
        // We are streaming through it, so it is plainly usable. Record that
        // much without touching the driver.
        card.unavailable.clear();
        card.probed = true;
      }
      else if(ossia::asio_diagnostics::verbose())
      {
        ossia::asio_diagnostics::log()
            << "\"" << card.name.toStdString() << "\": not probing while \"" << active
            << "\" is streaming\n";
      }
      return;
    }

    const std::string name = card.name.toStdString();
    if(loadAsioDriver(const_cast<char*>(name.c_str())))
    {
      ASIODriverInfo info{};
      info.asioVersion = 2;
      const ASIOError init = ASIOInit(&info);
      if(init == ASE_OK)
      {
        long numIn = 0, numOut = 0;
        const ASIOError chans = ASIOGetChannels(&numIn, &numOut);
        if(chans == ASE_OK)
        {
          card.inputChan = (int)numIn;
          card.outputChan = (int)numOut;
        }
        else
        {
          ossia::asio_diagnostics::log()
              << "ASIOGetChannels failed for \"" << name << "\", rc=" << chans
              << " -- listing it with 0 channels\n";
        }
        card.unavailable.clear();
        if(ossia::asio_diagnostics::verbose())
        {
          ossia::asio_diagnostics::log()
              << "\"" << name << "\": driver \"" << info.name << "\" asio v"
              << info.asioVersion << " driver v" << info.driverVersion << ", "
              << card.inputChan << " in / " << card.outputChan << " out\n";
        }
        ASIOExit();
      }
      else
      {
        // The driver is installed and loadable but cannot initialize right now.
        // Keep it listed, tagged with the reason, so the user can see it is
        // known but currently unusable rather than wondering why it vanished.
        card.inputChan = 0;
        card.outputChan = 0;
        card.unavailable = reasonFor(init);
        ossia::asio_diagnostics::log()
            << "ASIOInit failed for \"" << name << "\", rc=" << init << " ("
            << (info.errorMessage[0] ? info.errorMessage : "no message") << ") -- "
            << card.unavailable.toStdString() << '\n';
      }
      if(asioDrivers)
        asioDrivers->removeCurrentDriver();
    }
    else
    {
      card.inputChan = 0;
      card.outputChan = 0;
      card.unavailable = QObject::tr("driver failed to load");
      ossia::asio_diagnostics::log()
          << "loadAsioDriver(\"" << name
          << "\") failed -- CoCreateInstance refused the driver DLL "
             "(bitness mismatch or broken installation)\n";
    }

    card.probed = true;
  }

  QString prettyName() const override { return QObject::tr("ASIOSDK"); }

  std::shared_ptr<ossia::audio_engine> make_engine(
      const Audio::Settings::Model& set, const score::ApplicationContext& ctx) override
  {
    return std::make_shared<ossia::asio_engine>(
        set.getCardOut().toStdString(), set.getDefaultIn(), set.getDefaultOut(),
        set.getRate(), set.getBufferSize());
  }

  void setCard(QComboBox* combo, QString val)
  {
    for(int i = 0; i < combo->count(); i++)
    {
      if(combo->itemData(i).toString() == val)
      {
        combo->setCurrentIndex(i);
        return;
      }
    }
  }

  QWidget* make_settings(
      Audio::Settings::Model& m, Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp, QWidget* parent) override
  {
    auto w = new ASIOSettingsWidget{parent};
    auto lay = new QFormLayout{w};

    auto card_list = new QComboBox{w};
    auto show_ui = new QPushButton{tr("Show Control Panel"), w};

    // Label is decorated ("Audio 8 DJ (device not connected)"); the item data
    // stays the bare driver name, since that is what gets saved in the settings
    // and matched by setCard().
    auto populate = [this, card_list, &m] {
      const QString selected = m.getCardOut();
      // Rebuilding the list must not look like the user picking a device.
      const QSignalBlocker block{card_list};
      card_list->clear();
      for(const ASIOCard& card : devices)
        card_list->addItem(card.displayName(), card.name);
      setCard(card_list, selected);
    };
    populate();

    // Now that the page exists, refresh it whenever it is actually shown: that
    // is when the user is looking, and it is the only chance to notice hardware
    // plugged in since startup. Probing loads drivers, so it is skipped while an
    // ASIO engine streams -- the cached results from startup stand in that case.
    w->on_show = [this, populate] {
      if(!ossia::asio_engine::active_driver().empty())
        return;
      rescan(/* force = */ true);
      populate();
    };

    using Model = Audio::Settings::Model;

    {
      lay->addRow(QObject::tr("Device"), card_list);

      auto update_dev = [=, &m, &m_disp](const ASIOCard& dev) {
        if(dev.name != m.getCardOut())
        {
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardIn>(m, dev.name);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardOut>(m, dev.name);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultIn>(
              m, dev.inputChan);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultOut>(
              m, dev.outputChan);
        }
      };

      QObject::connect(
          card_list, SignalUtils::QComboBox_currentIndexChanged_int(), &v, [=](int i) {
            if(i >= 0 && i < (int)devices.size())
            {
              update_dev(devices[i]);
            }
          });

      // populate() already restored the saved selection; this only covers the
      // first run, where nothing has been chosen yet.
      if(m.getCardOut().isEmpty() && devices.size() > 1)
        update_dev(devices[1]);
    }

    {
      lay->addWidget(show_ui);
      connect(show_ui, &QPushButton::clicked, this, [=] {
        const int idx = card_list->currentIndex();
        if(idx <= 0 || idx >= (int)devices.size())
          return;

        const auto& dev = devices[idx];
        using res = ossia::asio_engine::control_panel_result;
        switch(ossia::asio_engine::open_control_panel(dev.name.toStdString()))
        {
          case res::ok:
            break;
          case res::other_driver_active:
            // ASIO permits one loaded driver per process, so we cannot reach
            // this driver's panel until the engine actually switches to it.
            score::warning(
                w, tr("ASIO"),
                tr("Cannot open the control panel of \"%1\" while \"%2\" is in use.\n"
                   "Apply this device first (OK), then reopen the settings.")
                    .arg(dev.name)
                    .arg(QString::fromStdString(ossia::asio_engine::active_driver())));
            break;
          case res::load_failed:
            score::warning(
                w, tr("ASIO"),
                tr("Could not load the driver \"%1\".").arg(dev.name));
            break;
          case res::init_failed:
            score::warning(
                w, tr("ASIO"),
                tr("The driver \"%1\" could not be initialized. Its hardware may be "
                   "disconnected or in use by another application.")
                    .arg(dev.name));
            break;
        }
      });
    }

    addBufferSizeWidget(*w, m, v);
    addSampleRateWidget(*w, m, v);

    con(m, &Model::changed, w, [=, &m] { setCard(card_list, m.getCardOut()); });
    return w;
  }

private:
  //! Whether the driver list has been enumerated successfully at least once.
  bool m_scanned{};
};

}

#endif
