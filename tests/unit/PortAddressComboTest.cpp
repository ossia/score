#include <State/Address.hpp>

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

#include <Process/Dataflow/PortAddressComboBox.hpp>

#include <Explorer/DeviceList.hpp>

#include <ossia/audio/audio_parameter.hpp>
#include <ossia/audio/audio_protocol.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

#include <memory>

namespace
{
Device::DeviceSettings audioSettings()
{
  Device::DeviceSettings s;
  s.name = "audio";
  return s;
}

struct QuietAudioDevice final : Device::DeviceInterface
{
  ossia::net::generic_device dev{std::make_unique<ossia::audio_protocol>(), "audio"};

  QuietAudioDevice()
      : Device::DeviceInterface{audioSettings()}
  {
    m_capas.nodeKinds = Device::NodeKind::AudioIn | Device::NodeKind::AudioOut;
  }

  bool reconnect() override { return true; }
  ossia::net::device_base* getDevice() const override
  {
    return const_cast<ossia::net::generic_device*>(&dev);
  }

  void addVirtualPort(const std::string& path)
  {
    auto& n = ossia::net::find_or_create_node(dev.get_root_node(), path);
    n.set_parameter(std::make_unique<ossia::virtual_audio_parameter>(2, n));
  }
};
}

TEST_CASE(
    "The address list is read again when it opens", "[dataflow][port][address]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Device::DeviceList devices;
    auto audio = new QuietAudioDevice;
    audio->addVirtualPort("/first");
    devices.addDevice(audio);
    {
      Process::PortAddressComboBox combo{
          devices, Process::PortType::Audio, false, nullptr};
      combo.setAddress(State::AddressAccessor{State::Address{"audio", {"first"}}});
      CHECK(combo.findText("audio:/first") > 0);
      CHECK(combo.toolTip() == "audio:/first");

      audio->addVirtualPort("/second");
      CHECK(combo.findText("audio:/second") == -1);

      combo.setEditText("audio:/sec");
      combo.showPopup();
      CHECK(combo.findText("audio:/second") > 0);
      CHECK(combo.currentText() == "audio:/sec");
      CHECK(combo.address().toString_unsafe() == "audio:/first");
      combo.hidePopup();

      combo.setAddress({});
      CHECK(combo.toolTip().isEmpty());
    }
    devices.removeDevice("audio");
  });
}
