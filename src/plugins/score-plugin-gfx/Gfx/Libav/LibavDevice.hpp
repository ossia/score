#pragma once
#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV

#include <Gfx/GfxDevice.hpp>
#include <Gfx/GfxInputDevice.hpp>
#include <Gfx/Libav/LibavOutputSettings.hpp>

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <ossia/detail/hash_map.hpp>

#include <verdigris>

class QComboBox;
class QFormLayout;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;

namespace Gfx
{

struct LibavSettings
{
  enum Direction
  {
    Input,
    Output
  };

  Direction direction{Input};
  QString path;

  // Shared
  int width{1280};
  int height{720};
  double rate{30};
  int audio_channels{2};
  int threads{0};

  // Output-specific
  QString audio_encoder_short, audio_encoder_long;
  QString audio_converted_smpfmt;
  int audio_sample_rate{44100};
  QString video_encoder_short, video_encoder_long;
  QString video_render_pixfmt;
  QString video_converted_pixfmt;
  QString muxer, muxer_long;
  ossia::hash_map<QString, QString> options;
  int input_transfer{13}; // AVColorTransferCharacteristic: 13=sRGB

  // Convert to LibavOutputSettings for the encoder
  LibavOutputSettings toOutputSettings() const
  {
    LibavOutputSettings s;
    s.path = path;
    s.width = width;
    s.height = height;
    s.rate = rate;
    s.audio_channels = audio_channels;
    s.threads = threads;
    s.audio_encoder_short = audio_encoder_short;
    s.audio_encoder_long = audio_encoder_long;
    s.audio_converted_smpfmt = audio_converted_smpfmt;
    s.audio_sample_rate = audio_sample_rate;
    s.video_encoder_short = video_encoder_short;
    s.video_encoder_long = video_encoder_long;
    s.video_render_pixfmt = video_render_pixfmt;
    s.video_converted_pixfmt = video_converted_pixfmt;
    s.muxer = muxer;
    s.muxer_long = muxer_long;
    s.options = options;
    s.input_transfer = input_transfer;
    return s;
  }
};

class LibavProtocolFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("8b3e4f2a-1d5c-4e7b-a9f3-6c2d8e4b1a7f")
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

SCORE_SERIALIZE_DATASTREAM_DECLARE(, Gfx::LibavSettings);
Q_DECLARE_METATYPE(Gfx::LibavSettings)
W_REGISTER_ARGTYPE(Gfx::LibavSettings)
#endif
