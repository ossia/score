#pragma once
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>

#include <QLineEdit>

#include <Gfx/GfxExecContext.hpp>
#include <Gfx/Graph/nodes.hpp>
#include <Spout/SpoutSender.h>
namespace Gfx
{
struct SpoutNode : OutputNode
{
  SpoutNode();
  virtual ~SpoutNode();

  Renderer* m_renderer{};
  std::shared_ptr<RenderState> m_renderState{};
  std::shared_ptr<SpoutSender> window{};

  void startRendering() override;
  void onRendererChange() override;
  bool canRender() const override;
  void stopRendering() override;

  void setRenderer(Renderer* r) override;
  Renderer* renderer() const override;

  void createOutput(
      GraphicsApi graphicsApi,
      std::function<void()> onReady,
      std::function<void()> onResize
      ) override;
  void destroyOutput() override;

  RenderState* renderState() const override;
  RenderedNode* createRenderer() const noexcept override;
};


class spout_window_context;
class spout_protocol : public ossia::net::protocol_base
{
public:
  spout_protocol(GfxExecutionAction& ctx) : context{&ctx} { }
  GfxExecutionAction* context{};
  bool pull(ossia::net::parameter_base&) override { return false; }
  bool push(const ossia::net::parameter_base&, const ossia::value& v) override { return false; }
  bool push_raw(const ossia::net::full_parameter_data&) override { return false; }
  bool observe(ossia::net::parameter_base&, bool) override { return false; }
  bool update(ossia::net::node_base& node_base) override { return false; }
};

class spout_parameter : public ossia::net::parameter_base
{
  GfxExecutionAction* context{};

public:
  int32_t node_id{};
  ScreenNode* screen{};

  spout_parameter(ossia::net::node_base& n, GfxExecutionAction* ctx)
      : ossia::net::parameter_base{n}, context{ctx}
  {
    screen = new ScreenNode;
    node_id = context->ui->register_node(std::unique_ptr<ScreenNode>{screen});
  }

  virtual ~spout_parameter() { context->ui->unregister_node(node_id); }

  void pull_value() override { }

  ossia::net::parameter_base& push_value(const ossia::value&) override { return *this; }

  ossia::net::parameter_base& push_value(ossia::value&&) override { return *this; }

  void push_texture(port_index idx) { context->setEdge(idx, port_index{this->node_id, 0}); }

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

class spout_device;
class spout_node : public ossia::net::node_base
{
  ossia::net::device_base& m_device;
  node_base* m_parent{};
  std::unique_ptr<spout_parameter> m_parameter;

public:
  spout_node(ossia::net::device_base& dev, std::string name)
      : m_device{dev}
      , m_parameter{std::make_unique<spout_parameter>(
            *this,
            dynamic_cast<spout_protocol&>(dev.get_protocol()).context)}
  {
    m_name = std::move(name);
  }

  spout_parameter* get_parameter() const override { return m_parameter.get(); }

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

class spout_device : public ossia::net::device_base
{
  spout_node root;

public:
  spout_device(std::unique_ptr<ossia::net::protocol_base> proto, std::string name)
      : ossia::net::device_base{std::move(proto)}, root{*this, name}
  {
  }

  const spout_node& get_root_node() const override { return root; }
  spout_node& get_root_node() override { return root; }
};
}

// Score part

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

namespace Gfx
{
class SpoutProtocolFactory final : public Device::ProtocolFactory
{
  SCORE_CONCRETE("ddf45db7-9eaf-453c-8fc0-86ccdf21677c")
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

class SpoutDevice final : public Device::DeviceInterface
{
  W_OBJECT(SpoutDevice)
public:
  SpoutDevice(const Device::DeviceSettings& settings, const score::DocumentContext& ctx);
  ~SpoutDevice();

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
  void setupContextMenu(QMenu&) const override;
  Device::Node refresh() override;
  void disconnect() override;

  void setupNode(ossia::net::node_base&, const ossia::extended_attributes& attr);

  const score::DocumentContext& m_ctx;
  spout_protocol* m_protocol{};
  mutable std::unique_ptr<spout_device> m_dev;
};

class SpoutSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  SpoutSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void setDefaults();
  QLineEdit* m_deviceNameEdit{};
};

}
