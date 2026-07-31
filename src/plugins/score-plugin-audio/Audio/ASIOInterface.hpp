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
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>

#if defined(OSSIA_ENABLE_ASIO)
#include <ossia/audio/asio_protocol.hpp>

#include <exception>

extern AsioDrivers* asioDrivers;
namespace Audio
{
struct ASIOCard
{
  QString name;
  int driver_index{-1};
  int inputChan{};
  int outputChan{};
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

  void rescan()
  {
    // Probing channel counts means loading each driver in turn, and
    // AsioDrivers::loadDriver() releases whatever driver is already loaded --
    // which would pull the rug out from under a streaming engine. rescan() runs
    // on every setDriver()/initDriver(), so that has to be avoided: when an
    // engine is active, reuse the counts from the previous scan instead. They
    // only change when the hardware does.
    const std::string active = ossia::asio_engine::active_driver();
    const std::vector<ASIOCard> previous = std::move(devices);

    devices.clear();
    devices.push_back(ASIOCard{QObject::tr("No device"), -1, 0, 0});

    try
    {
      auto cards = ossia::asio_engine::enumerate_drivers();
      for(auto& card : cards)
      {
        const QString name = QString::fromStdString(card.name);
        int ins = 0, outs = 0;

        if(!active.empty())
        {
          auto it = std::find_if(
              previous.begin(), previous.end(),
              [&](const ASIOCard& d) { return d.name == name; });
          if(it != previous.end())
          {
            ins = it->inputChan;
            outs = it->outputChan;
          }
          if(ossia::asio_diagnostics::verbose())
          {
            ossia::asio_diagnostics::log()
                << "\"" << card.name << "\": not probing channels while \"" << active
                << "\" is streaming, reusing " << ins << " in / " << outs << " out\n";
          }
        }
        else if(loadAsioDriver(const_cast<char*>(card.name.c_str())))
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
              ins = (int)numIn;
              outs = (int)numOut;
            }
            else
            {
              ossia::asio_diagnostics::log()
                  << "ASIOGetChannels failed for \"" << card.name << "\", rc=" << chans
                  << " -- listing it with 0 channels\n";
            }
            if(ossia::asio_diagnostics::verbose())
            {
              ossia::asio_diagnostics::log()
                  << "\"" << card.name << "\": driver \"" << info.name << "\" asio v"
                  << info.asioVersion << " driver v" << info.driverVersion << ", " << ins
                  << " in / " << outs << " out\n";
            }
            ASIOExit();
          }
          else
          {
            // Hardware absent or claimed by another process: the driver is
            // installed and loadable but cannot initialize right now. Keep it in
            // the list so the user can see it and read the reason.
            ossia::asio_diagnostics::log()
                << "ASIOInit failed for \"" << card.name << "\", rc=" << init << " ("
                << (info.errorMessage[0] ? info.errorMessage : "no message")
                << ") -- device present but unusable\n";
          }
          if(asioDrivers)
            asioDrivers->removeCurrentDriver();
        }
        else
        {
          ossia::asio_diagnostics::log()
              << "loadAsioDriver(\"" << card.name
              << "\") failed -- CoCreateInstance refused the driver DLL "
                 "(bitness mismatch or broken installation)\n";
        }

        devices.push_back(ASIOCard{name, card.driver_index, ins, outs});
      }

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
    auto w = new QWidget{parent};
    auto lay = new QFormLayout{w};

    auto card_list = new QComboBox{w};
    auto show_ui = new QPushButton{tr("Show Control Panel"), w};

    // Populate device list
    for(std::size_t i = 0; i < devices.size(); i++)
    {
      auto& card = devices[i];
      card_list->addItem(card.name, card.name);
    }

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

      if(m.getCardOut().isEmpty())
      {
        if(devices.size() > 1)
        {
          update_dev(devices[1]);
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
};

}

#endif
