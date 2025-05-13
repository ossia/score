#include "Library.hpp"

namespace Wokwi
{
void DeviceLibrary::initDevices()
{
  using enum CapabilityType;
  this->devices.push_back({.type = "wokwi-7segment", .pins = {}});
  // this->devices.push_back({.type = "wokwi-arduino-uno", .pins = {} });
  this->devices.push_back({.type = "wokwi-lcd1602", .pins = {}});
  this->devices.push_back(
      {.type = "wokwi-led",
       .pins
       = {{.name = "A", .type = Analog, .direction = DevicePin::Sink},
          {.name = "C", .type = Analog, .direction = DevicePin::Sink}}});

  this->devices.push_back({.type = "wokwi-neopixel", .pins = {}});
  this->devices.push_back(
      {.type = "wokwi-pushbutton",
       .pins = {
           {.name = "1.l", .type = GPIO, .direction = DevicePin::Either},
           {.name = "1.r", .type = GPIO, .direction = DevicePin::Either},
           {.name = "2.l", .type = GPIO, .direction = DevicePin::Either},
           {.name = "2.r", .type = GPIO, .direction = DevicePin::Either},
       }});
  this->devices.push_back(
      {.type = "wokwi-pushbutton-6mm",
       .pins = {
           {.name = "1.l", .type = GPIO, .direction = DevicePin::Either},
           {.name = "1.r", .type = GPIO, .direction = DevicePin::Either},
           {.name = "2.l", .type = GPIO, .direction = DevicePin::Either},
           {.name = "2.r", .type = GPIO, .direction = DevicePin::Either},
       }});
  this->devices.push_back({.type = "wokwi-resistor", .pins = {}});
  this->devices.push_back({.type = "wokwi-membrane-keypad", .pins = {}});
  this->devices.push_back(
      {.type = "wokwi-potentiometer",
       .pins = {{.name = "SIG", .type = Analog, .direction = DevicePin::Source}}});
  this->devices.push_back({.type = "wokwi-neopixel-matrix", .pins = {}});
  this->devices.push_back({.type = "wokwi-ssd1306", .pins = {}});
  this->devices.push_back(
      {.type = "wokwi-buzzer",
       .pins
       = {{.name = "1", .type = Analog, .direction = DevicePin::Sink},
          {.name = "2", .type = Analog, .direction = DevicePin::Sink}}});
  this->devices.push_back({.type = "wokwi-rotary-dialer", .pins = {}});
  this->devices.push_back(
      {.type = "wokwi-servo",
       .pins = {{.name = "PWM", .type = PWM, .direction = DevicePin::Sink}}});
  this->devices.push_back({.type = "wokwi-dht22", .pins = {}});
  // this->devices.push_back({.type = "wokwi-arduino-mega", .pins = {} });
  // this->devices.push_back({.type = "wokwi-arduino-nano", .pins = {} });
  this->devices.push_back({.type = "wokwi-ds1307", .pins = {}});
  this->devices.push_back(
      {.type = "wokwi-neopixel-ring",
       .pins = {{.name = "DIN", .type = GPIO, .direction = DevicePin::Sink}}});
  this->devices.push_back(
      {.type = "wokwi-slide-switch",
       .pins = {
           {.name = "1", .type = GPIO, .direction = DevicePin::Either},
           {.name = "2", .type = GPIO, .direction = DevicePin::Either},
           {.name = "3", .type = GPIO, .direction = DevicePin::Either},
       }});
  this->devices.push_back({.type = "wokwi-hc-sr04", .pins = {}});
  this->devices.push_back({.type = "wokwi-lcd2004", .pins = {}});
  this->devices.push_back(
      {.type = "wokwi-analog-joystick",
       .pins
       = {{.name = "HORZ", .type = Analog, .direction = DevicePin::Source},
          {.name = "VERT", .type = Analog, .direction = DevicePin::Source}}});

  this->devices.push_back(
      {.type = "wokwi-slide-potentiometer",
       .pins = {{.name = "SIG", .type = Analog, .direction = DevicePin::Source}}});

  this->devices.push_back({.type = "wokwi-ir-receiver", .pins = {}});
  this->devices.push_back({.type = "wokwi-ir-remote", .pins = {}});
  this->devices.push_back(
      {.type = "wokwi-pir-motion-sensor",
       .pins = {{.name = "OUT", .type = GPIO, .direction = DevicePin::Source}}});
  this->devices.push_back(
      {.type = "wokwi-ntc-temperature-sensor",
       .pins = {{.name = "OUT", .type = Analog, .direction = DevicePin::Source}}});
  this->devices.push_back(
      {.type = "wokwi-heart-beat-sensor",
       .pins = {{.name = "OUT", .type = GPIO, .direction = DevicePin::Source}}});
  this->devices.push_back(
      {.type = "wokwi-tilt-switch",
       .pins = {{.name = "OUT", .type = GPIO, .direction = DevicePin::Source}}});
  this->devices.push_back(
      {.type = "wokwi-flame-sensor",
       .pins
       = {{.name = "DOUT", .type = GPIO, .direction = DevicePin::Source},
          {.name = "AOUT", .type = Analog, .direction = DevicePin::Source}}});
  this->devices.push_back(
      {.type = "wokwi-gas-sensor",
       .pins
       = {{.name = "DOUT", .type = GPIO, .direction = DevicePin::Source},
          {.name = "AOUT", .type = Analog, .direction = DevicePin::Source}}});
  // this->devices.push_back({.type = "wokwi-franzininho", .pins = {} });
  // this->devices.push_back({.type = "wokwi-nano-rp2040-connect", .pins = {} });
  this->devices.push_back(
      {.type = "wokwi-small-sound-sensor",
       .pins
       = {{.name = "DOUT", .type = GPIO, .direction = DevicePin::Source},
          {.name = "AOUT", .type = Analog, .direction = DevicePin::Source}}});
  this->devices.push_back(
      {.type = "wokwi-big-sound-sensor",
       .pins
       = {{.name = "DOUT", .type = GPIO, .direction = DevicePin::Source},
          {.name = "AOUT", .type = Analog, .direction = DevicePin::Source}}});
  this->devices.push_back({.type = "wokwi-mpu6050", .pins = {}});
  // this->devices.push_back({.type = "wokwi-esp32-devkit-v1", .pins = {} });
  this->devices.push_back({.type = "wokwi-ky-040", .pins = {}});
  this->devices.push_back(
      {.type = "wokwi-photoresistor-sensor",
       .pins
       = {{.name = "DO", .type = GPIO, .direction = DevicePin::Source},
          {.name = "AO", .type = Analog, .direction = DevicePin::Source}}});
  this->devices.push_back({.type = "wokwi-rgb-led", .pins = {}});
  this->devices.push_back({.type = "wokwi-ili9341", .pins = {}});
  this->devices.push_back({.type = "wokwi-led-bar-graph", .pins = {}});
  this->devices.push_back({.type = "wokwi-microsd-card", .pins = {}});
  this->devices.push_back({.type = "wokwi-dip-switch-8", .pins = {}});
  this->devices.push_back({.type = "wokwi-stepper-motor", .pins = {}});
  this->devices.push_back({.type = "wokwi-hx711", .pins = {}});
  this->devices.push_back({.type = "wokwi-ks2e-m-dc5", .pins = {}});
  this->devices.push_back({.type = "wokwi-biaxial-stepper", .pins = {}});
}
}
