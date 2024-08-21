#pragma once
#include <Protocols/SimpleIO/HardwareDevice.hpp>
#include <Protocols/SimpleIO/SimpleIOSpecificSettings.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/fmt.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <QDebug>

#include <avnd/binding/ossia/from_value.hpp>
#include <avnd/binding/ossia/to_value.hpp>
#include <avnd/binding/ossia/uuid.hpp>
#include <avnd/introspection/hardware.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <libsimpleio/libadc.h>
#include <libsimpleio/libdac.h>
#include <libsimpleio/libgpio.h>
#include <libsimpleio/libpwm.h>
namespace Protocols::SimpleIO
{
template <typename T>
class GenericHardwareDevice : public HardwareDevice
{
public:
  friend struct TSerializer<DataStream, GenericHardwareDevice<T>>;
  friend struct TSerializer<JSONObject, GenericHardwareDevice<T>>;
  using value_ins = avnd::parameter_input_introspection<T>;
  using value_outs = avnd::parameter_output_introspection<T>;
  using gpios = avnd::gpio_port_hardware_introspection<T>;
  using pwms = avnd::pwm_port_hardware_introspection<T>;

  explicit GenericHardwareDevice(QObject* parent) noexcept
      : HardwareDevice{parent}
  {
  }

  GenericHardwareDevice(DataStream::Deserializer& vis, QObject* parent)
      : HardwareDevice{vis, parent}
  {
    vis.writeTo(*this);
  }

  GenericHardwareDevice(JSONObject::Deserializer& vis, QObject* parent)
      : HardwareDevice{vis, parent}
  {
    vis.writeTo(*this);
  }

  ~GenericHardwareDevice()
  { // FIXME release
  }

  UuidKey<HardwareDevice> concreteKey() const noexcept override
  {
    return oscr::uuid_from_string<T>();
  }

  void serialize_impl(const VisitorVariant& vis) const override
  {
    score::serialize_dyn(vis, *this);
  }

  std::unique_ptr<HardwareDevice> clone() noexcept override
  {
    return std::make_unique<GenericHardwareDevice<T>>(nullptr);
  }

  // 1. Setup the GUI Widget
  // 2. Get the configuration
  void setupConfiguration(QWidget* widg) override { SCORE_TODO; }
  std::vector<Protocols::SimpleIO::Port> getConfiguration() const noexcept override
  {
    return {};
  }
  void loadConfiguration(const std::vector<Protocols::SimpleIO::Port>& hardware) override
  {
    // FIXME this->hardware = hardware;
    namespace sio = Protocols::SimpleIO;
    this->hardware.clear();
    this->hardware.push_back(
        sio::Port{.control = sio::PWM{.chip = 0, .channel = 0, .polarity = 0}});
    this->hardware.push_back(
        sio::Port{.control = sio::PWM{.chip = 0, .channel = 1, .polarity = 0}});
  }

  // FIXME Refactor with AvndDevice
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
      node = &ossia::net::create_node(root, fmt::format("/{}", name));
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

  HardwareDevice::used_parameters setupDevice(
      ossia::net::simpleio_protocol& proto, ossia::net::node_base& rroot,
      const QString& name, const QString& path) override
  {
    used_parameters p;
    auto& root = ossia::net::create_node(rroot, path.toStdString());

    // Create ossia parameters for all the inputs
    // Create SIO devices for all the outputs

    // FIXME Refactor with AvndDevice
    {
      // Create the parameters on the device
      inputs.resize(value_ins::size);
      value_ins::for_all_n(
          impl.inputs, [&]<std::size_t I>(auto& f, avnd::predicate_index<I>) {
        inputs[I] = create_node<true>(root, f);

        // FIXME do it in push instead ?
        inputs[I]->add_callback(
            [&f, this](const ossia::value& v) { set_field_value(f, v); });
      });

      outputs.resize(value_outs::size);
      value_outs::for_all_n(
          impl.outputs, [&]<std::size_t I>(auto& f, avnd::predicate_index<I>) {
        outputs[I] = create_node<false>(root, f);
      });

      // FIXME ADCs etc
      // Create GPIO
      m_gpio.resize(gpios::size);
      gpios::for_all_n2(
          impl.hardware, [&]<std::size_t I, std::size_t F>(
                             auto& f, avnd::predicate_index<I>, avnd::field_index<F>) {
        auto port = get<GPIO>(this->hardware[F].control);
        GPIO_impl impl{};
        int32_t error;
        int32_t flags = port.flags;
        int32_t events{};
        int32_t state{};

        GPIO_line_open(port.chip, port.line, flags, events, state, &impl.fd, &error);

        m_gpio[I] = std::move(impl);
      });

      // Create PWM
      m_pwm.resize(pwms::size);
      pwms::for_all_n2(
          impl.hardware, [&]<std::size_t I, std::size_t F>(
                             auto& f, avnd::predicate_index<I>, avnd::field_index<F>) {
        auto port = get<PWM>(this->hardware[F].control);
        PWM_impl impl{};
        int32_t error;

        PWM_configure(
            port.chip, port.channel, 1'000'000, 500'000, port.polarity, &error);
        PWM_open(port.chip, port.channel, &impl.fd, &error);
        m_pwm[I] = std::move(impl);
      });
    }
    return p;
  }

  void teardownDevice() override { SCORE_TODO; }

  void process_on_input()
  {
    impl();
    apply_outputs();
  }

  void apply_outputs()
  {
    value_outs::for_all_n(
        impl.outputs, [&]<std::size_t I>(auto& f, avnd::predicate_index<I>) {
      SCORE_ASSERT(outputs[I]);
      outputs[I]->set_value(oscr::to_ossia_value(f, f.value));
    });

    gpios::for_all_n2(
        impl.hardware, [&]<std::size_t I, std::size_t F>(
                           auto& f, avnd::predicate_index<I>, avnd::field_index<F>) {
      auto port = m_gpio[I];
      // FIXME
    });

    pwms::for_all_n2(
        impl.hardware, [&]<std::size_t I, std::size_t F>(
                           auto& f, avnd::predicate_index<I>, avnd::field_index<F>) {
      auto port = m_pwm[I];
      if(f.frequency)
      {
        // FIXME PWM_configure()..
      }

      if(f.duty_cycle)
      {
        int error;
        int val = std::clamp(*f.duty_cycle, 0.f, 1.f) * 1'000'000;
        PWM_write(port.fd, val, &error);
      }
    });

    // FIXME adcs, dacs
  }

  void pull(ossia::net::parameter_base& v) override { SCORE_TODO; }

  void push(const ossia::net::parameter_base& p, const ossia::value& v) override
  {
    // Set value
    // Process
  }

  void push_to_hardware()
  {
    /*
    if(auto it = m_dac.find(&p); it != m_dac.end())
    {
      int32_t error;
      DAC_write(it->second.fd, ossia::convert<int>(v), &error);
      return;
    }
    else if(auto it = m_pwm.find(&p); it != m_pwm.end())
    {
      int32_t error;
      // Input is between [0; 1], we map that to the duty cycle range [0, period]
      int val = std::clamp(ossia::convert<float>(v), 0.f, 1.f) * 1'000'000;
      PWM_write(it->second.fd, val, &error);
      return;
    }
    else if(auto it = m_gpio_out.find(&p); it != m_gpio_out.end())
    {
      int32_t error;
      GPIO_line_write(it->second.fd, ossia::convert<bool>(v), &error);
      return;
    }
    else if(auto it = m_custom.find(&p); it != m_custom.end())
    {
      it->second->push(p, v);
      return;
    }
*/
  }

  T impl;
  std::vector<ossia::net::parameter_base*> inputs;
  std::vector<ossia::net::parameter_base*> outputs;

  std::vector<Protocols::SimpleIO::Port> hardware;

  std::vector<ADC_impl> m_adc;
  std::vector<DAC_impl> m_dac;
  std::vector<PWM_impl> m_pwm;
  std::vector<GPIO_impl> m_gpio;
};

template <typename T>
class GenericHardwareDeviceFactory : public HardwareDeviceFactory
{
public:
  UuidKey<HardwareDeviceFactory> concreteKey() const noexcept override
  {
    return oscr::uuid_from_string<T>();
  }
  QString prettyName() const noexcept override
  {
    return oscr::fromStringView(avnd::get_name<T>());
  }
  HardwareDevice* make(QObject* parent) override
  {
    return new GenericHardwareDevice<T>(parent);
  }
  HardwareDevice* load(const VisitorVariant& vis, QObject* parent) override
  {
    return score::deserialize_dyn(vis, [&](auto&& deserializer) {
      return new GenericHardwareDevice<T>{deserializer, parent};
    });
  }
};
}

template <typename T>
struct is_custom_serialized<Protocols::SimpleIO::GenericHardwareDevice<T>>
    : std::true_type
{
};

template <typename T>
struct TSerializer<DataStream, Protocols::SimpleIO::GenericHardwareDevice<T>>
{
  using model_type = Protocols::SimpleIO::GenericHardwareDevice<T>;
  static void readFrom(DataStream::Serializer& s, const model_type& obj)
  {
    s.insertDelimiter();
  }

  static void writeTo(DataStream::Deserializer& s, model_type& obj)
  {
    s.checkDelimiter();
  }
};

template <typename T>
struct TSerializer<JSONObject, Protocols::SimpleIO::GenericHardwareDevice<T>>
{
  using model_type = Protocols::SimpleIO::GenericHardwareDevice<T>;
  static void readFrom(JSONObject::Serializer& s, const model_type& obj) { }
  static void writeTo(JSONObject::Deserializer& s, model_type& obj) { }
};

#include <halp/controls.hpp>
struct Motor2PWM
{
  static constexpr auto name() { return "Motor (2 PWM)"; }
  static constexpr auto uuid() { return "1b5a3764-5e22-11ef-b97c-5ce91ee31bcd"; }
  struct
  {
    halp::hslider_f32<"speed"> speed;
    halp::toggle<"direction"> direction;
  } inputs;

  struct
  {
  } outputs;

  struct
  {

    struct
    {
      enum
      {
        pwm
      };
      std::optional<float> duty_cycle;
      std::optional<float> frequency;
    } pwm1;

    struct
    {
      enum
      {
        pwm
      };
      std::optional<float> duty_cycle;
      std::optional<float> frequency;
    } pwm2;
  } hardware;

  // Hardware : put in separate bank ? As it can be used for I & O. e.g. Serial UART, etc.
  void operator()()
  {
    auto& p1 = hardware.pwm1.duty_cycle;
    auto& p2 = hardware.pwm2.duty_cycle;
    float v = 1. - inputs.speed;
    if(inputs.direction)
    {
      p1 = 1;
      p2 = v;
    }
    else
    {
      p1 = 1;
      p2 = v;
    }

    qDebug() << *p1 << *p2;
  }
};
