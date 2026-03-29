#pragma once
#include <Gfx/GfxDevice.hpp>
#include <Gfx/GfxExecContext.hpp>
#include <Gfx/GfxInputDevice.hpp>

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Video/ExternalInput.hpp>
#include <Video/FrameQueue.hpp>

#include <ossia/gfx/texture_parameter.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_node.hpp>

#include <verdigris>

class QPlainTextEdit;
class QFormLayout;
class QLineEdit;

namespace Gfx::GStreamer
{

struct GStreamerSettings
{
  QString pipeline;

  // Output-specific settings (used when pipeline contains appsrc)
  int width{1280};
  int height{720};
  int rate{30};
  int audio_channels{2};

  // Input transfer function: what colorspace is the rendered RGBA texture in?
  // 13 = sRGB (default), 8 = Linear, 16 = PQ (HDR10), 18 = HLG, 2 = Passthrough
  // Stored as int to avoid including ffmpeg headers here; maps to AVColorTransferCharacteristic.
  int input_transfer{13}; // AVCOL_TRC_IEC61966_2_1 (sRGB)
};

// Returns true if the pipeline string contains appsrc (output mode)
inline bool pipeline_is_output(const QString& pipeline)
{
  return pipeline.contains(QStringLiteral("appsrc"));
}

class ProtocolFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("2c644357-16a4-4c25-9e27-8e5c4a9a647d")
public:
  QString prettyName() const noexcept override;
  QString category() const noexcept override;
  QUrl manual() const noexcept override;

  Device::DeviceEnumerators
  getEnumerators(const score::DocumentContext& ctx) const override;

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& plugin,
      const score::DocumentContext& ctx) override;
  const Device::DeviceSettings& defaultSettings() const noexcept override;

  Device::AddressDialog* makeAddAddressDialog(
      const Device::DeviceInterface& dev, const score::DocumentContext& ctx,
      QWidget* parent) override;
  Device::AddressDialog* makeEditAddressDialog(
      const Device::AddressSettings&, const Device::DeviceInterface& dev,
      const score::DocumentContext& ctx, QWidget*) override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;

  QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

  void serializeProtocolSpecificSettings(
      const QVariant& data, const VisitorVariant& visitor) const override;

  bool checkCompatibility(
      const Device::DeviceSettings& a,
      const Device::DeviceSettings& b) const noexcept override;
};

}

SCORE_SERIALIZE_DATASTREAM_DECLARE(, Gfx::GStreamer::GStreamerSettings);
Q_DECLARE_METATYPE(Gfx::GStreamer::GStreamerSettings)
W_REGISTER_ARGTYPE(Gfx::GStreamer::GStreamerSettings)
