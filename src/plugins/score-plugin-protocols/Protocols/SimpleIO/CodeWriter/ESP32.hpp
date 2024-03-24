#pragma once
#include <Device/CodeWriter.hpp>
#include <Device/Protocol/DeviceInterface.hpp>

#include <Protocols/SimpleIO/SimpleIOSpecificSettings.hpp>

namespace Protocols
{
class SimpleIOCodeWriter_ESP32 : public Device::CodeWriter
{
public:
  SimpleIOSpecificSettings settings;
  SimpleIOCodeWriter_ESP32(const Device::DeviceInterface& p)
      : Device::CodeWriter{p}
      , settings{p.settings().deviceSpecificSettings.value<SimpleIOSpecificSettings>()}
  {
  }

  std::string get_global_vars() const noexcept
  {
    std::string ret;
    struct
    {
      std::string operator()(const SimpleIO::GPIO& pin) const noexcept { return ""; }
      std::string operator()(const SimpleIO::PWM& pin) const noexcept { return ""; }
      std::string operator()(const SimpleIO::ADC& pin) const noexcept { return ""; }
      std::string operator()(const SimpleIO::DAC& pin) const noexcept
      {
        return fmt::format("DacESP32 ossia_dac_{0}(DAC_CHANNEL_{0});", pin.channel);
      }
      std::string operator()(const SimpleIO::HID& pin) const noexcept { return ""; }
      std::string operator()(const SimpleIO::Custom& pin) const noexcept { return ""; }

    } vis;
    for(auto& port : settings.ports)
      ret += visit(vis, port.control);
    return ret;
  }
  std::string get_model_struct() const noexcept
  {
    std::string ret;
    struct
    {
      std::string varname;
      std::string operator()(const SimpleIO::GPIO& pin) const noexcept
      {
        return fmt::format("bool {};\n", varname);
      }
      std::string operator()(const SimpleIO::PWM& pin) const noexcept
      {
        return fmt::format("float {0}; float {0}_dutycycle;\n", varname);
      }
      std::string operator()(const SimpleIO::ADC& pin) const noexcept
      {
        return fmt::format("float {};\n", varname);
      }
      std::string operator()(const SimpleIO::DAC& pin) const noexcept
      {
        return fmt::format("float {};\n", varname);
      }
      std::string operator()(const SimpleIO::HID& pin) const noexcept { return ""; }
      std::string operator()(const SimpleIO::Custom& pin) const noexcept { return ""; }
    } vis;
    for(auto& port : settings.ports)
    {
      vis.varname = varname(port);
      ret += visit(vis, port.control);
    }
    return ret;
  }

  std::string get_init_code() const noexcept
  {
    std::string ret;
    struct
    {
      // FIXME pinMode
      std::string operator()(const SimpleIO::GPIO& pin) const noexcept { return ""; }
      std::string operator()(const SimpleIO::PWM& pin) const noexcept { return ""; }
      std::string operator()(const SimpleIO::ADC& pin) const noexcept { return ""; }
      std::string operator()(const SimpleIO::DAC& pin) const noexcept { return ""; }
      std::string operator()(const SimpleIO::HID& pin) const noexcept { return ""; }
      std::string operator()(const SimpleIO::Custom& pin) const noexcept { return ""; }

    } vis;
    for(auto& port : settings.ports)
      ret += visit(vis, port.control);
    return ret;
  }

  std::string init() override
  {
    return fmt::format(
        R"_(
#include <OSCBundle.h>
#include <WiFiUdp.h>
#include <DacESP32.h>
  
WiFiUDP ossia_udp_socket;
OSCBundle ossia_osc_bundle;

// Global variables
{}

struct ossia_data_model
{{
{}
}} ossia_model;

void ossia_init_board() {{

  // UDP
  {{ Initialize _("UDP"); 
  ossia_udp_socket.begin(local_osc_udp_port);
  }}

  // Per-device init
  {}
}}
)_",
        get_global_vars(), get_model_struct(), get_init_code());
  }

  static std::string varname(const SimpleIO::Port& port) noexcept
  {
    return "v_" + QString{port.path}.replace('/', '_').toStdString();
  }

  std::string read_pin(const SimpleIO::Port& port) const noexcept
  {
    std::string ret;
    struct
    {
      std::string varname;
      std::string operator()(const SimpleIO::GPIO& pin) const noexcept
      {
        return pin.direction == true
                   ? ""
                   : fmt::format(
                       "ossia_model.{} = digitalRead({});\n", varname, pin.line);
      }
      std::string operator()(const SimpleIO::PWM& pin) const noexcept { return ""; }
      std::string operator()(const SimpleIO::ADC& pin) const noexcept
      {
        return fmt::format(
            "ossia_model.{} = analogRead(ADC{}_CHANNEL_{}_GPIO_NUM) / 4095.f;\n",
            varname, pin.chip, pin.channel);
      }
      std::string operator()(const SimpleIO::DAC& pin) const noexcept { return ""; }
      std::string operator()(const SimpleIO::HID& pin) const noexcept { return ""; }
      std::string operator()(const SimpleIO::Custom& pin) const noexcept { return ""; }

    } vis{.varname = varname(port)};
    return visit(vis, port.control);
  }

  std::string readPins() override
  {
    // Read local inputs: GPIO -> data model
    std::string read;
    for(auto& port : settings.ports)
      read += read_pin(port);

    return fmt::format(
        R"_(
void ossia_read_pins() {{
  {}
}}
)_",
        read);
  }

  std::string write_pin(const SimpleIO::Port& port) const noexcept
  {
    std::string ret;
    struct
    {
      std::string varname;
      std::string operator()(const SimpleIO::GPIO& pin) const noexcept
      {
        return pin.direction == false
                   ? ""
                   : fmt::format(
                       "digitalWrite({}, ossia_model.{});\n", pin.line, varname);
      }
      std::string operator()(const SimpleIO::PWM& pin) const noexcept { return ""; }
      std::string operator()(const SimpleIO::ADC& pin) const noexcept { return ""; }
      std::string operator()(const SimpleIO::DAC& pin) const noexcept
      {
        return fmt::format(
            "ossia_dac_{}.outputVoltage(ossia_model.{} * 3.3f);\n", pin.channel,
            varname);
      }
      std::string operator()(const SimpleIO::HID& pin) const noexcept { return ""; }
      std::string operator()(const SimpleIO::Custom& pin) const noexcept { return ""; }

    } vis{.varname = varname(port)};
    return visit(vis, port.control);
  }
  std::string writePins() override
  {
    // Process local outputs: data model -> GPIO
    std::string write;
    for(auto& port : settings.ports)
      write += write_pin(port);

    return fmt::format(
        R"_(
void ossia_write_pins() {{
  {}
}}
)_",
        write);
  }

  std::string read_osc(const SimpleIO::Port& port) const noexcept
  {
    std::string ret;
    struct
    {
      const SimpleIO::Port& port;
      std::string operator()(const SimpleIO::GPIO& pin) const noexcept
      {
        return pin.direction == false
                   ? ""
                   : fmt::format(
                       R"_(inmsg.dispatch("/{}", [](OSCMessage& msg) {{ ossia_model.{} = msg.getBoolean(0); }});
)_",
                       port.path.toStdString(), varname(port));
      }
      std::string operator()(const SimpleIO::PWM& pin) const noexcept
      {
        return fmt::format(
            R"_(inmsg.dispatch("/{}", [](OSCMessage& msg) {{ ossia_model.{} = msg.getFloat(0); }});
)_",
            port.path.toStdString(), varname(port));
      }
      std::string operator()(const SimpleIO::ADC& pin) const noexcept { return ""; }
      std::string operator()(const SimpleIO::DAC& pin) const noexcept
      {
        return fmt::format(
            R"_(inmsg.dispatch("/{}", [](OSCMessage& msg) {{ ossia_model.{} = msg.getFloat(0); }});
)_",
            port.path.toStdString(), varname(port));
      }
      std::string operator()(const SimpleIO::HID& pin) const noexcept { return ""; }
      std::string operator()(const SimpleIO::Custom& pin) const noexcept { return ""; }

    } vis{port};
    return visit(vis, port.control);
  }

  std::string readOSC() override
  {
    // Read network inputs: /osc -> data model
    std::string read;
    for(auto& port : settings.ports)
      read += read_osc(port);
    return fmt::format(
        R"_(
void ossia_read_osc() {{
  OSCMessage inmsg;
  int size = ossia_udp_socket.parsePacket();
  if(size <= 0)
    return;

  while(size--)
    inmsg.fill(ossia_udp_socket.read());

  if(inmsg.hasError())
    return;

  {}
}}
)_",
        read);
  }

  std::string write_osc(const SimpleIO::Port& port) const noexcept
  {
    std::string ret;
    struct
    {
      const SimpleIO::Port& port;
      std::string operator()(const SimpleIO::GPIO& pin) const noexcept
      {
        return pin.direction == true
                   ? ""
                   : fmt::format(
                       R"_(ossia_osc_bundle.add("/{}").add(ossia_model.{});
)_",
                       port.path.toStdString(), varname(port));
      }
      std::string operator()(const SimpleIO::PWM& pin) const noexcept { return ""; }
      std::string operator()(const SimpleIO::ADC& pin) const noexcept
      {
        return fmt::format(
            R"_(ossia_osc_bundle.add("/{}").add(ossia_model.{});
)_",
            port.path.toStdString(), varname(port));
      }
      std::string operator()(const SimpleIO::DAC& pin) const noexcept { return ""; }
      std::string operator()(const SimpleIO::HID& pin) const noexcept { return ""; }
      std::string operator()(const SimpleIO::Custom& pin) const noexcept { return ""; }

    } vis{port};
    return visit(vis, port.control);
  }

  virtual std::string writeOSC() override
  {
    // Process network outputs: data model -> /osc
    std::string write;
    for(auto& port : settings.ports)
      write += write_osc(port);

    return fmt::format(
        R"_(
void ossia_write_osc() {{
  {}

  ossia_udp_socket.beginPacket(remote_osc_udp_host, remote_osc_udp_port);
  ossia_osc_bundle.send(ossia_udp_socket);
  ossia_udp_socket.endPacket();
  //Udp.flush();
  ossia_osc_bundle.empty();
}}
)_",
        write);
  }
};

}
