#pragma once
#include <Gfx/GfxDevice.hpp>
#include <Gfx/GfxExecContext.hpp>
#include <Gfx/GfxInputDevice.hpp>
#include <Gfx/SharedInputSettings.hpp>
#include <Video/CameraInput.hpp>
#include <Video/FrameQueue.hpp>

#include <ossia/detail/lockfree_queue.hpp>
#include <ossia/gfx/texture_parameter.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_node.hpp>

#include <QLineEdit>

class QComboBox;
namespace libfreenect2
{
class Freenect2Device;
class PacketPipeline;
class SyncMultiFrameListener;
class Frame;
}

// Score part

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

namespace Gfx::Kinect2
{
struct Kinect2Settings
{
  QString input;
  bool rgb{true}, ir{true}, depth{true};
};

class ProtocolFactory final : public SharedInputProtocolFactory
{
  SCORE_CONCRETE("1056df8a-f20c-40e4-995e-f18ffda3a16a")
public:
  QString prettyName() const noexcept override;
  Device::DeviceEnumerator*
  getEnumerator(const score::DocumentContext& ctx) const override;

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& plugin,
      const score::DocumentContext& ctx) override;
  const Device::DeviceSettings& defaultSettings() const noexcept override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;
};

}

Q_DECLARE_METATYPE(Gfx::Kinect2::Kinect2Settings)
W_REGISTER_ARGTYPE(Gfx::Kinect2::Kinect2Settings)
