#pragma once

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

#include <Gfx/GfxInputDevice.hpp>
#include <Gfx/SharedInputSettings.hpp>

#include <score_plugin_gfx_export.h>

#include <memory>

namespace Video
{
class ExternalInput;
}

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

/** Headless factory for the PipeWire capture stream (no Device/Document
 *  machinery) — mirrors videoio's makeAJACapture so test harnesses can
 *  drive the real consumer path directly.
 *
 *  @param path input URL, e.g. "pipewire://<node-name>?width=1280&height=720&fps=60&format=rgba"
 */
SCORE_PLUGIN_GFX_EXPORT
std::shared_ptr<::Video::ExternalInput> makePipewireCapture(const QString& path);

/** Testability: transport the capture stream's last negotiation settled on —
 *  "shm", "dmabuf", "none" (not negotiated yet), or empty (not a PipeWire
 *  capture stream). */
SCORE_PLUGIN_GFX_EXPORT
QString pipewireInputNegotiatedTransport(::Video::ExternalInput& input);

} // namespace Gfx::PipeWire
