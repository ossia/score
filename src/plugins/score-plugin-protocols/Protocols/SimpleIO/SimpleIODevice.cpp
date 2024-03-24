
#include <ossia/detail/config.hpp>

#include <ossia/detail/timer.hpp>

#if defined(OSSIA_PROTOCOL_SIMPLEIO)
#include "SimpleIODevice.hpp"
#include "SimpleIOSpecificSettings.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Protocols/SimpleIO/CodeWriter/ESP32.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/small_vector.hpp>
#include <ossia/detail/variant.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/common/complex_type.hpp>
#include <ossia/network/context.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <QDebug>
#include <QFile>
#include <QMenu>

#include <libsimpleio/libadc.h>
#include <libsimpleio/libdac.h>
#include <libsimpleio/libgpio.h>
#include <libsimpleio/libpwm.h>

#include <wobjectimpl.h>

#include <cstdint>
W_OBJECT_IMPL(Protocols::SimpleIODevice)
namespace ossia::net
{

struct simpleio_protocol : public ossia::net::protocol_base
{
public:
  simpleio_protocol(ossia::net::network_context_ptr ctx)
      : protocol_base{flags{}}
      , m_context{ctx}
      , m_timer{ctx->context}
  {
    m_timer.set_delay(std::chrono::milliseconds{4});
  }

  ~simpleio_protocol()
  {
    stop_processing();

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
  }

  void stop_processing() { m_timer.stop(); }

  void set_device(ossia::net::device_base& dev) override { m_device = &dev; }
  void init(const Protocols::SimpleIOSpecificSettings& conf)
  {
    namespace sio = Protocols::SimpleIO;

    ossia::small_vector<const sio::ADC*, 16> adc;
    ossia::small_vector<const sio::DAC*, 16> dac;
    ossia::small_vector<const sio::PWM*, 16> pwm;
    ossia::small_vector<const sio::GPIO*, 16> gpio;

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
    }

    static_assert(offsetof(sio::Port, control) == 0);
    SCORE_ASSERT(m_device);
    auto& root = m_device->get_root_node();
    for(auto* ptr : adc)
    {
      auto& port = *reinterpret_cast<const sio::Port*>(ptr);
      auto param
          = ossia::create_parameter(root, "/adc/" + port.name.toStdString(), "float");

      ADC_impl impl{};
      int32_t error;
      ADC_open(ptr->chip, ptr->channel, &impl.fd, &error);
      m_adc.emplace(param, impl);
    }

    for(auto* ptr : dac)
    {
      auto& port = *reinterpret_cast<const sio::Port*>(ptr);
      auto param
          = ossia::create_parameter(root, "/dac/" + port.name.toStdString(), "float");

      DAC_impl impl{};
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
      PWM_impl impl{};
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

      GPIO_impl impl{};
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

    if(!m_gpio_in.empty() || !m_adc.empty())
    {
      m_timer.start([this] { this->update_function(); });
    }
  }

  void update_function()
  {
    for(auto& [param, obj] : m_adc)
    {
      int32_t sample, error;
      ADC_read(obj.fd, &sample, &error);
      param->set_value(sample);
    }
    for(auto& [param, obj] : m_gpio_in)
    {
      int32_t sample, error;
      GPIO_line_read(obj.fd, &sample, &error);
      param->set_value(bool(sample));
    }
  }

  bool pull(parameter_base& v) override
  {
    if(auto it = m_adc.find(&v); it != m_adc.end())
    {
      int32_t sample, error;
      ADC_read(it->second.fd, &sample, &error);
      v.set_value(sample);
      return true;
    }

    if(auto it = m_gpio_in.find(&v); it != m_gpio_in.end())
    {
      int32_t sample, error;
      GPIO_line_read(it->second.fd, &sample, &error);
      v.set_value(bool(sample));
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
    return false;
  }
  bool push_raw(const full_parameter_data&) override { return false; }
  bool observe(parameter_base&, bool) override { return false; }
  bool update(node_base& node_base) override { return false; }

  ossia::net::network_context_ptr m_context;
  ossia::timer m_timer;
  ossia::net::device_base* m_device{};

  struct ADC_impl
  {
    int fd{};
    float value{};
  };
  struct DAC_impl
  {
    int fd{};
    float value{};
  };
  struct PWM_impl
  {
    int fd{};
    float value{};
  };
  struct GPIO_impl
  {
    int fd{};
    int value{};
  };

  ossia::flat_map<ossia::net::parameter_base*, ADC_impl> m_adc;
  ossia::flat_map<ossia::net::parameter_base*, DAC_impl> m_dac;
  ossia::flat_map<ossia::net::parameter_base*, PWM_impl> m_pwm;
  ossia::flat_map<ossia::net::parameter_base*, GPIO_impl> m_gpio_in;
  ossia::flat_map<ossia::net::parameter_base*, GPIO_impl> m_gpio_out;
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

static void init_simpleio_osc_device(
    const SimpleIOSpecificSettings& set, ossia::net::device_base& dev)
{
  namespace sio = Protocols::SimpleIO;
  auto& root = dev.get_root_node();

  for(auto& port : set.ports)
  {
    std::string type;

    if(auto ptr = ossia::get_if<sio::ADC>(&port.control))
    {
      type = "float";
    }
    else if(auto ptr = ossia::get_if<sio::DAC>(&port.control))
    {
      type = "float";
    }
    else if(auto ptr = ossia::get_if<sio::PWM>(&port.control))
    {
      type = "float";
    }
    else if(auto ptr = ossia::get_if<sio::GPIO>(&port.control))
    {
      type = "bool";
    }

    ossia::create_parameter(root, port.path.toStdString(), type);
  }
}

bool SimpleIODevice::reconnect()
{
  disconnect();

  try
  {
    const auto& set
        = m_settings.deviceSpecificSettings.value<SimpleIOSpecificSettings>();
    const auto& name = settings().name.toStdString();
    if(set.osc_configuration)
    {
      if(auto proto = ossia::net::make_osc_protocol(m_ctx, *set.osc_configuration))
      {
        m_dev = std::make_unique<ossia::net::generic_device>(std::move(proto), name);
        init_simpleio_osc_device(set, *m_dev);
      }
    }
    else
    {
      auto pproto = std::make_unique<ossia::net::simpleio_protocol>(m_ctx);
      auto& proto = *pproto;
      auto dev = std::make_unique<ossia::net::generic_device>(std::move(pproto), name);
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
void SimpleIODevice::setupContextMenu(QMenu& menu) const
{
  auto act = menu.addAction("Generate code...");
  connect(act, &QAction::triggered, this, [&] {
    SimpleIOCodeWriter_ESP32 wr{*this};
    std::string ret;
    ret += R"_(#pragma once
#include "ossia_embedded_api.hpp"
#include "constants.hpp"
#include "utility.hpp"

#include <soc/adc_channel.h>
)_";
    ret += wr.init();
    ret += wr.readOSC();
    ret += wr.readPins();
    ret += wr.writeOSC();
    ret += wr.writePins();
    qDebug() << ret.c_str();
  });
}
}

#endif
