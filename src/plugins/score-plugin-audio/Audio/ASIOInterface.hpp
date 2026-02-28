#pragma once
#include <ossia/detail/config.hpp>
#include <Audio/AudioInterface.hpp>
#include <Audio/Settings/Model.hpp>
#include <Audio/Settings/View.hpp>

#include <score/command/Dispatchers/SettingsCommandDispatcher.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>

#if defined(OSSIA_ENABLE_ASIO)
#include <ossia/audio/asio_protocol.hpp>

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
    devices.clear();
    devices.push_back(ASIOCard{QObject::tr("No device"), -1, 0, 0});

    try
    {
      auto cards = ossia::asio_engine::enumerate_drivers();
      for(auto& card : cards)
      {
        // To get channel counts, we must briefly load each driver
        int ins = 0, outs = 0;
        if(loadAsioDriver(const_cast<char*>(card.name.c_str())))
        {
          ASIODriverInfo info{};
          info.asioVersion = 2;
          if(ASIOInit(&info) == ASE_OK)
          {
            long numIn = 0, numOut = 0;
            ASIOGetChannels(&numIn, &numOut);
            ins = (int)numIn;
            outs = (int)numOut;
            ASIOExit();
          }
          if(asioDrivers)
            asioDrivers->removeCurrentDriver();
        }

        devices.push_back(
            ASIOCard{QString::fromStdString(card.name), card.driver_index, ins, outs});
      }
    }
    catch(...)
    {
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
        int idx = card_list->currentIndex();
        if(idx > 0 && idx < (int)devices.size())
        {
          ossia::asio_engine::open_control_panel(devices[idx].name.toStdString());
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
