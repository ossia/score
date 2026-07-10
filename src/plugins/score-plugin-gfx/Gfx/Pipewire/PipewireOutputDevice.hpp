#pragma once

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

#include <Gfx/GfxDevice.hpp>
#include <Gfx/SharedOutputSettings.hpp>

#include <score_plugin_gfx_export.h>

namespace score::gfx
{
class OutputNode;
}

namespace Gfx::PipeWire
{

/** Protocol factory for the PipeWire video OUTPUT device. Acts as a
 *  PipeWire producer (`PW_DIRECTION_OUTPUT`) — accepts frames rendered
 *  by score's QRhi pipeline and publishes them as a `Video/Source`
 *  PipeWire stream that any consumer (OBS, ffplay -f pipewire, etc.)
 *  can pick up.
 *
 *  Build-time gated on `OSSIA_ENABLE_PIPEWIRE`. Registration happens
 *  in score_plugin_gfx.cpp under `SCORE_HAS_PIPEWIRE_VIDEO_IO`. */
class OutputFactory final : public Gfx::SharedOutputProtocolFactory
{
  SCORE_CONCRETE("d5e7b22b-b7f6-4680-9610-2457509b7946")
public:
  QString prettyName() const noexcept override;
  QUrl manual() const noexcept override;

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& plugin,
      const score::DocumentContext& ctx) override;

  const Device::DeviceSettings& defaultSettings() const noexcept override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;
};

/** Headless factory for the PipeWire producer output node (no
 *  Device/Document machinery) — for test harnesses. The returned node is
 *  a score::gfx::OutputNode that owns its offscreen RenderState; caller
 *  owns the pointer (delete after removing from the Graph).
 *
 *  Settings: path = "<node-name>[?format=rgba&dmabuf=on]", width/height/rate.
 */
SCORE_PLUGIN_GFX_EXPORT
score::gfx::OutputNode* makePipewireOutput(const Gfx::SharedOutputSettings& s);

/** Testability: true iff the producer engaged a DMA-BUF publish mode
 *  (Vulkan or EGL/GBM) rather than the CPU-readback fallback. */
SCORE_PLUGIN_GFX_EXPORT
bool pipewireOutputDmabufEngaged(const score::gfx::OutputNode& node);

} // namespace Gfx::PipeWire
