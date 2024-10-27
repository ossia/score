#pragma once
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Explorer/DefaultProtocolFactory.hpp>

#include <Crousti/Concepts.hpp>

#include <ossia/network/base/protocol.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <QFormLayout>

#include <avnd/concepts/generic.hpp>

namespace oscr
{
template <typename Node>
class Protocol final : public ossia::net::protocol_base
{
public:
  explicit Protocol(const ossia::net::network_context_ptr& ctx)
      : ossia::net::protocol_base{flags{}}
  {
    if_possible(this->impl.io_context = &ctx->context);
    if_possible(this->impl.init());
  }

  template <bool Input, typename Field>
  ossia::net::parameter_base* create_node(ossia::net::node_base& root, Field& f)
  {
    ossia::net::node_base* node{};
    if constexpr(avnd::has_path<Field>)
    {
      static constexpr auto name = avnd::get_path<Field>();
      node = &ossia::net::create_node(root, name);
    }
    else
    {
      static constexpr auto name = avnd::get_name<Field>();
      if constexpr(Input)
      {
        node = &ossia::net::create_node(root, fmt::format("/in/{}", name));
      }
      else
      {
        node = &ossia::net::create_node(root, fmt::format("/out/{}", name));
      }
    }

    using val_t = std::decay_t<decltype(f.value)>;
    return node->create_parameter(oscr::type_for_arg<val_t>());
  }

  void set_field_value(auto& f, const ossia::value& v)
  {
    oscr::from_ossia_value(f, v, f.value);
    if_possible(f.update(impl));
    process_on_input();
  }

  void set_device(ossia::net::device_base& dev) override
  {
    // Create the parameters on the device
    auto& root = dev.get_root_node();
    inputs.resize(avnd::input_introspection<Node>::size);
    avnd::input_introspection<Node>::for_all_n(
        avnd::get_inputs(impl), [&]<std::size_t I>(auto& f, avnd::field_index<I>) {
      inputs[I] = create_node<true>(root, f);
      inputs[I]->add_callback(
          [&f, this](const ossia::value& v) { set_field_value(f, v); });
    });

    outputs.resize(avnd::output_introspection<Node>::size);
    avnd::output_introspection<Node>::for_all_n(
        avnd::get_outputs(impl), [&]<std::size_t I>(auto& f, avnd::field_index<I>) {
      outputs[I] = create_node<false>(root, f);
    });
  }

  bool push(const ossia::net::parameter_base& param, const ossia::value& v) override
  {
    // Push on input : process
    for(int i = 0; i < std::ssize(inputs); i++)
    {
      auto* p = inputs[i];
      if(p == &param)
      {
        avnd::input_introspection<Node>::for_nth(
            avnd::get_inputs(impl), i,
            [this, &v](auto& f) { this->set_field_value(f, v); });
        break;
      }
    }
    return false;
  }

  void process_on_input()
  {
    impl();

    avnd::output_introspection<Node>::for_all_n(
        avnd::get_outputs(impl), [&]<std::size_t I>(auto& f, avnd::field_index<I>) {
      SCORE_ASSERT(outputs[I]);
      outputs[I]->set_value(oscr::to_ossia_value(f, f.value));
    });
  }

  bool pull(ossia::net::parameter_base&) override { return false; }
  bool push_raw(const ossia::net::full_parameter_data&) override { return false; }
  bool observe(ossia::net::parameter_base&, bool) override { return false; }
  bool update(ossia::net::node_base& node_base) override { return false; }

  Node impl;
  std::vector<ossia::net::parameter_base*> inputs;
  std::vector<ossia::net::parameter_base*> outputs;
};

template <typename Node_T>
class DeviceImplementation final : public Device::OwningDeviceInterface
{
public:
  DeviceImplementation(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& plugin, const score::DocumentContext& ctx)
      : OwningDeviceInterface{settings}
      , m_ctx{plugin}
  {
    m_capas.canRefreshTree = true;
    m_capas.canAddNode = false;
    m_capas.canRemoveNode = false;
    m_capas.canRenameNode = false;
    m_capas.canSetProperties = false;
    m_capas.canSerialize = false;
  }

  ~DeviceImplementation() { }

  bool reconnect() override
  {
    disconnect();

    auto protocol = std::make_unique<Protocol<Node_T>>(this->m_ctx.networkContext());
    auto dev = std::make_unique<ossia::net::generic_device>(
        std::move(protocol), settings().name.toStdString());

    m_dev = std::move(dev);
    deviceChanged(nullptr, m_dev.get());

    return connected();
  }
  void disconnect() override { OwningDeviceInterface::disconnect(); }

private:
  const Explorer::DeviceDocumentPlugin& m_ctx;
};

template <typename Node>
class ProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  explicit ProtocolSettingsWidget(QWidget* parent = nullptr)
      : Device::ProtocolSettingsWidget(parent)
  {
    m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
    m_deviceNameEdit->setText("SpatGRIS");
    auto layout = new QFormLayout;
    layout->addRow(tr("Name"), m_deviceNameEdit);
    setLayout(layout);
  }

  virtual ~ProtocolSettingsWidget() { }

  Device::DeviceSettings getSettings() const override
  {
    // TODO should be = m_settings to follow the other patterns.
    Device::DeviceSettings s;
    s.name = m_deviceNameEdit->text();
    s.protocol = oscr::uuid_from_string<Node>();
    return s;
  }

  void setSettings(const Device::DeviceSettings& settings) override
  {
    m_deviceNameEdit->setText(settings.name);
  }

private:
  QLineEdit* m_deviceNameEdit{};
};

template <typename Node>
class ProtocolFactory final : public Protocols::DefaultProtocolFactory
{
  UuidKey<Device::ProtocolFactory> concreteKey() const noexcept override
  {
    return oscr::uuid_from_string<Node>();
  }

  QString prettyName() const noexcept override
  {
    return QString::fromUtf8(avnd::get_name<Node>().data());
  }
  QString category() const noexcept override { return StandardCategories::software; }
  Device::DeviceEnumerators
  getEnumerators(const score::DocumentContext& ctx) const override
  {
    return {};
  }

  Device::DeviceInterface* makeDevice(
      const Device::DeviceSettings& settings,
      const Explorer::DeviceDocumentPlugin& plugin,
      const score::DocumentContext& ctx) override
  {
    return new DeviceImplementation<Node>{settings, plugin, ctx};
  }

  const Device::DeviceSettings& defaultSettings() const noexcept override
  {
    static const Device::DeviceSettings& settings = [&] {
      Device::DeviceSettings s;
      s.protocol = concreteKey();
      s.name = "SpatGRIS";
      s.deviceSpecificSettings = QVariant{};
      return s;
    }();

    return settings;
  }

  Device::ProtocolSettingsWidget* makeSettingsWidget() override
  {
    return new ProtocolSettingsWidget<Node>;
  }

  QVariant makeProtocolSpecificSettings(const VisitorVariant& visitor) const override
  {
    return QVariant{};
  }

  void serializeProtocolSpecificSettings(
      const QVariant& data, const VisitorVariant& visitor) const override
  {
  }

  bool checkCompatibility(
      const Device::DeviceSettings& a,
      const Device::DeviceSettings& b) const noexcept override
  {
    return true;
  }
};

template <typename... Nodes>
static void instantiate_device(
    std::vector<score::InterfaceBase*>& v, const score::ApplicationContext& ctx,
    const score::InterfaceKey& key)
{
  if(key == Device::ProtocolFactory::static_interfaceKey())
  {
    (v.emplace_back(
         static_cast<Device::ProtocolFactory*>(new oscr::ProtocolFactory<Nodes>())),
     ...);
  }
}
}
