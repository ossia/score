#include <ossia/detail/config.hpp>

#include <QDebug>
#if defined(OSSIA_PROTOCOL_SIMPLEIO)
#include "SimpleIODevice.hpp"
#include "SimpleIOSpecificSettings.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/detail/flat_map.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/common/complex_type.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <libsimpleio/libadc.h>
#include <libsimpleio/libdac.h>
#include <libsimpleio/libgpio.h>
#include <libsimpleio/libpwm.h>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Protocols::SimpleIODevice)
namespace ossia::net
{

namespace sio = Protocols::SimpleIO;
struct simpleio_protocol : public ossia::net::protocol_base
{
public:
  explicit simpleio_protocol(ossia::net::network_context_ptr ctx)
      : protocol_base{flags{}}
      , m_context{ctx}
  {
  }

  ~simpleio_protocol() { teardown(); }

  void set_device(ossia::net::device_base& dev) override { m_device = &dev; }
  void init(const Protocols::SimpleIOSpecificSettings& conf)
  {

    ossia::small_vector<const sio::ADC*, 4> adc;
    ossia::small_vector<const sio::DAC*, 4> dac;
    ossia::small_vector<const sio::PWM*, 8> pwm;
    ossia::small_vector<const sio::GPIO*, 16> gpio;
    ossia::small_vector<const sio::Custom*, 8> custom;

    for(auto& port : conf.ports)
    {
      if(auto ptr = ossia::get_if<sio::ADC>(&port.control))
      {
        adc.push_back(ptr);
      }
      else if(auto ptr = ossia::get_if<sio::DAC>(&port.control))
      {
        dac.push_back(ptr);
      }
      else if(auto ptr = ossia::get_if<sio::PWM>(&port.control))
      {
        pwm.push_back(ptr);
      }
      else if(auto ptr = ossia::get_if<sio::GPIO>(&port.control))
      {
        gpio.push_back(ptr);
      }
      else if(auto ptr = ossia::get_if<sio::Custom>(&port.control))
      {
        custom.push_back(ptr);
        m_custom_storage.push_back(*ptr);
      }
    }

    static_assert(offsetof(sio::Port, control) == 0);
    SCORE_ASSERT(m_device);
    auto& root = m_device->get_root_node();
    for(auto* ptr : adc)
    {
      auto& port = *reinterpret_cast<const sio::Port*>(ptr);
      auto param
          = ossia::create_parameter(root, "/adc/" + port.name.toStdString(), "float");

      sio::ADC_impl impl{};
      int32_t error;
      ADC_open(ptr->chip, ptr->channel, &impl.fd, &error);
      m_adc.emplace(param, impl);
    }

    for(auto* ptr : dac)
    {
      auto& port = *reinterpret_cast<const sio::Port*>(ptr);
      auto param
          = ossia::create_parameter(root, "/dac/" + port.name.toStdString(), "float");

      sio::DAC_impl impl{};
      int32_t error;
      DAC_open(ptr->chip, ptr->channel, &impl.fd, &error);
      m_dac.emplace(param, impl);
    }

    for(auto* ptr : pwm)
    {
      auto& port = *reinterpret_cast<const sio::Port*>(ptr);
      auto param
          = ossia::create_parameter(root, "/pwm/" + port.name.toStdString(), "float");
      param->push_value(0.5f);

      // FIXME add a child parameter to set the period.
      sio::PWM_impl impl{};
      int32_t error;

      PWM_configure(ptr->chip, ptr->channel, 1'000'000, 500'000, ptr->polarity, &error);
      PWM_open(ptr->chip, ptr->channel, &impl.fd, &error);
      m_pwm.emplace(param, impl);
    }

    for(auto* ptr : gpio)
    {
      auto& port = *reinterpret_cast<const sio::Port*>(ptr);
      auto param
          = ossia::create_parameter(root, "/gpio/" + port.name.toStdString(), "bool");

      sio::GPIO_impl impl{};
      int32_t error;
      int32_t flags = ptr->flags;
      int32_t events{};
      int32_t state{};

      GPIO_line_open(ptr->chip, ptr->line, flags, events, state, &impl.fd, &error);

      if(ptr->direction)
        m_gpio_out.emplace(param, impl);
      else
        m_gpio_in.emplace(param, impl);
    }

    int custom_i = 0;
    for(auto& ptr : custom)
    {
      if(auto& d = m_custom_storage[custom_i].device)
      {
        auto& port = *reinterpret_cast<const sio::Port*>(ptr);

        d->loadConfiguration(ptr->device->getConfiguration());
        auto nodes = d->setupDevice(*this, root, port.name, port.path);
        for(auto node : nodes)
        {
          m_custom[node] = d.get();
        }
      }
    }
  }

  void teardown()
  {
    int error;
    for(auto& adc : m_adc)
      ADC_close(adc.second.fd, &error);
    for(auto& dac : m_dac)
      DAC_close(dac.second.fd, &error);
    for(auto& pwm : m_pwm)
      PWM_close(pwm.second.fd, &error);
    for(auto& gpio : m_gpio_in)
      GPIO_close(gpio.second.fd, &error);
    for(auto& gpio : m_gpio_out)
      GPIO_close(gpio.second.fd, &error);
    for(auto& custom : m_custom_storage)
      custom.device->teardownDevice();
  }

  bool pull(parameter_base& p) override
  {
    if(auto it = m_adc.find(&p); it != m_adc.end())
    {
      int32_t sample, error;
      ADC_read(it->second.fd, &sample, &error);
      p.set_value(sample);
      return true;
    }
    else if(auto it = m_gpio_in.find(&p); it != m_gpio_in.end())
    {
      int32_t sample, error;
      GPIO_line_read(it->second.fd, &sample, &error);
      p.set_value(sample);
      return true;
    }
    else if(auto it = m_custom.find(&p); it != m_custom.end())
    {
      it->second->pull(p);
      return true;
    }
    return false;
  }

  bool push(const parameter_base& p, const value& v) override
  {
    if(auto it = m_dac.find(&p); it != m_dac.end())
    {
      int32_t error;
      DAC_write(it->second.fd, ossia::convert<int>(v), &error);
      return true;
    }
    else if(auto it = m_pwm.find(&p); it != m_pwm.end())
    {
      int32_t error;
      // Input is between [0; 1], we map that to the duty cycle range [0, period]
      int val = std::clamp(ossia::convert<float>(v), 0.f, 1.f) * 1'000'000;
      PWM_write(it->second.fd, val, &error);
      return true;
    }
    else if(auto it = m_gpio_out.find(&p); it != m_gpio_out.end())
    {
      int32_t error;
      GPIO_line_write(it->second.fd, ossia::convert<bool>(v), &error);
      return true;
    }
    else if(auto it = m_custom.find(&p); it != m_custom.end())
    {
      it->second->push(p, v);
      return true;
    }
    return false;
  }

  bool push_raw(const full_parameter_data&) override { return false; }
  bool observe(parameter_base&, bool) override { return false; }
  bool update(node_base& node_base) override { return false; }

  ossia::net::network_context_ptr m_context;
  ossia::net::device_base* m_device{};

  ossia::small_vector<Protocols::SimpleIO::Custom, 8> m_custom_storage;

  ossia::flat_map<ossia::net::parameter_base*, sio::ADC_impl> m_adc;
  ossia::flat_map<ossia::net::parameter_base*, sio::DAC_impl> m_dac;
  ossia::flat_map<ossia::net::parameter_base*, sio::PWM_impl> m_pwm;
  ossia::flat_map<ossia::net::parameter_base*, sio::GPIO_impl> m_gpio_in;
  ossia::flat_map<ossia::net::parameter_base*, sio::GPIO_impl> m_gpio_out;
  ossia::flat_map<ossia::net::parameter_base*, Protocols::SimpleIO::HardwareDevice*>
      m_custom;
};
}
namespace Protocols
{

SimpleIODevice::SimpleIODevice(
    const Device::DeviceSettings& settings, const ossia::net::network_context_ptr& ctx)
    : OwningDeviceInterface{settings}
    , m_ctx{ctx}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canSerialize = false;
}

SimpleIODevice::~SimpleIODevice() { }

bool SimpleIODevice::reconnect()
{
  disconnect();

  try
  {
    const auto& set
        = m_settings.deviceSpecificSettings.value<SimpleIOSpecificSettings>();
    {
      auto pproto = std::make_unique<ossia::net::simpleio_protocol>(m_ctx);
      auto& proto = *pproto;
      auto dev = std::make_unique<ossia::net::generic_device>(
          std::move(pproto), settings().name.toStdString());
      proto.init(set);
      m_dev = std::move(dev);
    }
    deviceChanged(nullptr, m_dev.get());
  }
  catch(const std::runtime_error& e)
  {
    qDebug() << "SimpleIO error: " << e.what();
  }
  catch(...)
  {
    qDebug() << "SimpleIO error";
  }

  return connected();
}

void SimpleIODevice::disconnect()
{
  OwningDeviceInterface::disconnect();
}
}
#endif
