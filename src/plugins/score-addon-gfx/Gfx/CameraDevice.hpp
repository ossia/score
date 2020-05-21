#pragma once
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>

#include <QLineEdit>

#include <Gfx/GfxExecContext.hpp>
#include <Gfx/Graph/videonode.hpp>
#include <Video/CameraInput.hpp>

class QComboBox;
namespace Gfx
{
struct camera_settings
{
  std::string input;
  std::string device;
  int size[2]{{},{}};
  double fps{};
};
class gfx_window_context;
class camera_protocol : public ossia::net::protocol_base
{
public:
  std::shared_ptr<::Video::CameraInput> camera;
  camera_protocol(GfxExecutionAction& ctx) : context{&ctx}
  {
    camera = std::make_shared<::Video::CameraInput>();
  }
  GfxExecutionAction* context{};
  bool pull(ossia::net::parameter_base&) override { return false; }
  bool push(const ossia::net::parameter_base&, const ossia::value& v) override { return false; }
  bool push_raw(const ossia::net::full_parameter_data&) override { return false; }
  bool observe(ossia::net::parameter_base&, bool) override { return false; }
  bool update(ossia::net::node_base& node_base) override { return false; }

  void start_execution() override
  {
    camera->start();
  }
  void stop_execution() override
  {
    camera->stop();
  }
};

class camera_parameter : public ossia::net::parameter_base
{
  GfxExecutionAction* context{};

public:
  std::shared_ptr<::Video::CameraInput> camera;
  int32_t node_id{};
  YUV420Node* node{};

  camera_parameter(const camera_settings& settings, ossia::net::node_base& n, GfxExecutionAction* ctx)
      : ossia::net::parameter_base{n}, context{ctx}
  {
    auto& proto = static_cast<camera_protocol&>(n.get_device().get_protocol());
    camera = proto.camera;
    camera->load(settings.input, settings.device, settings.size[0], settings.size[1], settings.fps);

    node = new YUV420Node(proto.camera);
    node_id = context->ui->register_node(std::unique_ptr<YUV420Node>(node));
  }
  void pull_texture(port_index idx) { context->setEdge(port_index{this->node_id, 0}, idx); }

  virtual ~camera_parameter() { context->ui->unregister_node(node_id); }

  void pull_value() override { }

  ossia::net::parameter_base& push_value(const ossia::value&) override { return *this; }

  ossia::net::parameter_base& push_value(ossia::value&&) override { return *this; }


  ossia::net::parameter_base& push_value() override { return *this; }

  ossia::value value() const override { return {}; }

  ossia::net::parameter_base& set_value(const ossia::value&) override { return *this; }

  ossia::net::parameter_base& set_value(ossia::value&&) override { return *this; }

  ossia::val_type get_value_type() const override { return {}; }

  ossia::net::parameter_base& set_value_type(ossia::val_type) override { return *this; }

  ossia::access_mode get_access() const override { return {}; }

  ossia::net::parameter_base& set_access(ossia::access_mode) override { return *this; }

  const ossia::domain& get_domain() const override { throw; }

  ossia::net::parameter_base& set_domain(const ossia::domain&) override { return *this; }

  ossia::bounding_mode get_bounding() const override { return {}; }

  ossia::net::parameter_base& set_bounding(ossia::bounding_mode) override { return *this; }
};

class camera_device;
class camera_node : public ossia::net::node_base
{
  ossia::net::device_base& m_device;
  node_base* m_parent{};
  std::unique_ptr<camera_parameter> m_parameter;

public:
  camera_node(const camera_settings& settings, ossia::net::device_base& dev, std::string name)
      : m_device{dev}
      , m_parameter{std::make_unique<camera_parameter>(
            settings,
            *this,
            dynamic_cast<camera_protocol&>(dev.get_protocol()).context)}
  {
    m_name = std::move(name);
  }

  camera_parameter* get_parameter() const override { return m_parameter.get(); }

private:
  ossia::net::device_base& get_device() const override { return m_device; }
  ossia::net::node_base* get_parent() const override { return m_parent; }
  ossia::net::node_base& set_name(std::string) override { return *this; }
  ossia::net::parameter_base* create_parameter(ossia::val_type) override
  {
    return m_parameter.get();
  }
  bool remove_parameter() override { return false; }

  std::unique_ptr<ossia::net::node_base> make_child(const std::string& name) override
  {
    return {};
  }
  void removing_child(ossia::net::node_base& node_base) override { }
};

class camera_device : public ossia::net::device_base
{
  camera_node root;

public:
  camera_device(const camera_settings& settings, std::unique_ptr<ossia::net::protocol_base> proto, std::string name)
      : ossia::net::device_base{std::move(proto)}, root{settings, *this, name}
  {
  }

  const camera_node& get_root_node() const override { return root; }
  camera_node& get_root_node() override { return root; }
};
}

// Score part

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

namespace Gfx
{
struct CameraSettings
{
  QString input;
  QString device;
  QSize size{};
  double fps{};
};

class CameraProtocolFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("d615690b-f2e2-447b-b70e-a800552db69c")
  QString prettyName() const noexcept override;
  QString category() const noexcept override;
  Device::DeviceEnumerator* getEnumerator(const score::DocumentContext& ctx) const override;

  Device::DeviceInterface*
  makeDevice(const Device::DeviceSettings& settings, const score::DocumentContext& ctx) override;
  const Device::DeviceSettings& defaultSettings() const noexcept override;
  Device::AddressDialog* makeAddAddressDialog(
      const Device::DeviceInterface& dev,
      const score::DocumentContext& ctx,
      QWidget* parent) override;
  Device::AddressDialog* makeEditAddressDialog(
      const Device::AddressSettings&,
      const Device::DeviceInterface& dev,
      const score::DocumentContext& ctx,
      QWidget*) override;

  Device::ProtocolSettingsWidget* makeSettingsWidget() override;

  QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override;

  void serializeProtocolSpecificSettings(const QVariant& data, const VisitorVariant& visitor)
      const override;

  bool checkCompatibility(const Device::DeviceSettings& a, const Device::DeviceSettings& b)
      const noexcept override;
};

class CameraDevice final : public Device::DeviceInterface
{
  W_OBJECT(CameraDevice)
public:
  CameraDevice(const Device::DeviceSettings& settings, const score::DocumentContext& ctx);
  ~CameraDevice();

  void addAddress(const Device::FullAddressSettings& settings) override;

  void updateAddress(
      const State::Address& currentAddr,
      const Device::FullAddressSettings& settings) override;
  bool reconnect() override;
  void recreate(const Device::Node& n) override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

private:
  using Device::DeviceInterface::refresh;
  QMimeData* mimeData() const override;
  Device::Node refresh() override;
  void disconnect() override;

  void setupNode(ossia::net::node_base&, const ossia::extended_attributes& attr);

  const score::DocumentContext& m_ctx;
  camera_protocol* m_protocol{};
  mutable std::unique_ptr<camera_device> m_dev;
};

class CameraSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  CameraSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void setDefaults();
  QLineEdit* m_deviceNameEdit{};
  QComboBox* m_combo{};
  std::vector<CameraSettings> m_settings;
};

}


Q_DECLARE_METATYPE(Gfx::CameraSettings)
W_REGISTER_ARGTYPE(Gfx::CameraSettings)
