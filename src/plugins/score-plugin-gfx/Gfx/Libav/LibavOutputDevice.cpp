#include "LibavOutputDevice.hpp"

#if SCORE_HAS_LIBAV
#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Libav/LibavEncoder.hpp>
#include <Gfx/Libav/LibavEncoderNode.hpp>

#include <score/serialization/MimeVisitor.hpp>

#include <ossia/network/generic/generic_node.hpp>

#include <ossia-qt/name_utils.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QMenu>
#include <QMimeData>

#include <wobjectimpl.h>

SCORE_SERALIZE_DATASTREAM_DEFINE(Gfx::LibavOutputSettings);
namespace Gfx
{
// ffmpeg -y  -hwaccel cuda -hwaccel_output_format cuda -f video4linux2 -input_format mjpeg -framerate 30 -i  /dev/video0
// -fflags nobuffer
// -c:v hevc_nvenc
// -preset:v llhq
// -rc constqp
// -zerolatency 1
// -delay 0
// -forced-idr 1
// -g 1
// -cbr 1
// -qp 10
// -f matroska  -

struct EncoderConfiguration
{
  AVPixelFormat hardwareAcceleration{AV_PIX_FMT_NONE};
  std::string encoder;
  int threads{};
};

class libav_output_protocol : public Gfx::gfx_protocol_base
{
public:
  LibavEncoder encoder;
  explicit libav_output_protocol(GfxExecutionAction& ctx)
      : gfx_protocol_base{ctx}
  {
  }
  ~libav_output_protocol() { }

  void start_execution() override { encoder.start(); }
  void stop_execution() override { encoder.stop(); }
};

class libav_output_device : public ossia::net::device_base
{
  ossia::net::generic_node root;

public:
  libav_output_device(
      const LibavOutputSettings& set, LibavEncoder& enc,
      std::unique_ptr<gfx_protocol_base> proto, std::string name)
      : ossia::net::device_base{std::move(proto)}
      , root{name, *this}
  {
    auto& p = *static_cast<gfx_protocol_base*>(m_protocol.get());
    auto node = new LibavEncoderNode{set, enc, 0};
    root.add_child(std::make_unique<gfx_node_base>(*this, p, node, "Video"));
  }

  const ossia::net::generic_node& get_root_node() const override { return root; }
  ossia::net::generic_node& get_root_node() override { return root; }
};
class LibavOutputDevice final : public GfxOutputDevice
{
  W_OBJECT(LibavOutputDevice)
public:
  using GfxOutputDevice::GfxOutputDevice;
  ~LibavOutputDevice();

private:
  bool reconnect() override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

  mutable std::unique_ptr<libav_output_device> m_dev;
};

class LibavOutputSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  LibavOutputSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void setDefaults();
  QLineEdit* m_deviceNameEdit{};
  Device::DeviceSettings m_settings;
};

LibavOutputDevice::~LibavOutputDevice() { }

bool LibavOutputDevice::reconnect()
{
  disconnect();

  try
  {
    auto set = this->settings().deviceSpecificSettings.value<LibavOutputSettings>();
    auto plug = m_ctx.findPlugin<DocumentPlugin>();
    if(plug)
    {
      auto m_protocol = new libav_output_protocol{plug->exec};
      m_dev = std::make_unique<libav_output_device>(
          set, m_protocol->encoder, std::unique_ptr<libav_output_protocol>(m_protocol),
          this->settings().name.toStdString());
    }
  }
  catch(std::exception& e)
  {
    qDebug() << "Could not connect: " << e.what();
  }
  catch(...)
  {
    // TODO save the reason of the non-connection.
  }

  return connected();
}

QString LibavOutputProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Libav Output");
}

QString LibavOutputProtocolFactory::category() const noexcept
{
  return StandardCategories::util;
}

Device::DeviceInterface* LibavOutputProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new LibavOutputDevice(settings, ctx);
}

const Device::DeviceSettings&
LibavOutputProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Libav Output";
    LibavOutputSettings specif;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::AddressDialog* LibavOutputProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev, const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* LibavOutputProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings& set, const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx, QWidget* parent)
{
  return nullptr;
}

Device::ProtocolSettingsWidget* LibavOutputProtocolFactory::makeSettingsWidget()
{
  return new LibavOutputSettingsWidget;
}

QVariant LibavOutputProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<LibavOutputSettings>(visitor);
}

void LibavOutputProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<LibavOutputSettings>(data, visitor);
}

bool LibavOutputProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  return a.name != b.name;
}

LibavOutputSettingsWidget::LibavOutputSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_deviceNameEdit);

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);
  setLayout(layout);

  setDefaults();
}

void LibavOutputSettingsWidget::setDefaults()
{
  m_deviceNameEdit->setText("Libav Output");
}

Device::DeviceSettings LibavOutputSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_settings;
  s.name = m_deviceNameEdit->text();
  s.protocol = LibavOutputProtocolFactory::static_concreteKey();
  return s;
}

void LibavOutputSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_settings = settings;

  auto prettyName = settings.name;
  m_deviceNameEdit->setText(prettyName);
}

}

template <>
void DataStreamReader::read(const Gfx::LibavOutputSettings& n)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::LibavOutputSettings& n)
{
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::LibavOutputSettings& n)
{
}

template <>
void JSONWriter::write(Gfx::LibavOutputSettings& n)
{
}

W_OBJECT_IMPL(Gfx::LibavOutputDevice)
#endif
