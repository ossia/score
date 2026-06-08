#pragma once

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

#include <Gfx/GfxInputDevice.hpp>
#include <Gfx/SharedInputSettings.hpp>

namespace Gfx::PipeWire
{

/** Protocol factory for the PipeWire video INPUT device. Connects to a
 *  PipeWire node providing raw video frames and surfaces them as
 *  AVFrames through score's score::gfx pipeline.
 *
 *  Build-time gated on `OSSIA_ENABLE_PIPEWIRE` (see CMakeLists).
 *  Registration happens in score_plugin_gfx.cpp under the
 *  `SCORE_HAS_PIPEWIRE_VIDEO_IO` define. */
class InputFactory final : public SharedInputProtocolFactory
{
  SCORE_CONCRETE("cf6a355f-34d1-4d24-a6ea-3d204f93cde9")
public:
  QString prettyName() const noexcept override;
  QUrl manual() const noexcept override;

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& plugin,
      const score::DocumentContext& ctx) override;
  const Device::DeviceSettings& defaultSettings() const noexcept override;

  Device::DeviceEnumerators
  getEnumerators(const score::DocumentContext& ctx) const override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;
};

class PipeWireDevice;

} // namespace Gfx::PipeWire
