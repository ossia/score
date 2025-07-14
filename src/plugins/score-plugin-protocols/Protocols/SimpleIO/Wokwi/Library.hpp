#pragma once

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/flat_map.hpp>

#include <QString>

#include <vector>

namespace Wokwi
{

enum CapabilityType : int8_t
{
  GPIO,
  PWM,
  Analog,
  SPI,
  I2C,
  UART,
  Power,
  Ground
};

struct MCUCapability
{
  CapabilityType type{};
  uint8_t line{};    // GPIO line
  uint8_t device{};  // PWM 0, 1, 2 / ADC 0, 1, 2...
  uint8_t channel{}; // Channel A, B...
  bool input{};      // Pin supports input for this capability
  bool output{};     // Pin supports output for this capabiliy
};

struct MCUPin : std::vector<MCUCapability>
{
  using vector::vector;
  auto gpio() const noexcept
  {
    return ossia::ptr_find_if(
        *this, [](const auto& p) { return p.type == CapabilityType::GPIO; });
  }
  auto adc() const noexcept
  {
    return ossia::ptr_find_if(*this, [](const auto& p) {
      return p.type == CapabilityType::Analog && p.input;
    });
  }
  auto dac() const noexcept
  {
    return ossia::ptr_find_if(*this, [](const auto& p) {
      return p.type == CapabilityType::Analog && p.output;
    });
  }
  auto pwm() const noexcept
  {
    return ossia::ptr_find_if(
        *this, [](const auto& p) { return p.type == CapabilityType::PWM; });
  }
};

struct MCU
{
  QString name;
  ossia::flat_map<QString, MCUPin> pins;
};

// BoardCapability, BoardPin, BoardPart:
// represent a board in our system-wide board library
struct BoardCapability
{
  CapabilityType type{};
  QString serialType;
  int info{}; // GPIO Line, ADC / DAC / PWM channel...
};

struct BoardPin
{
  QString name;
  QString target;
};

struct BoardPart
{
  QString mcu;
  QString type;
  std::vector<BoardPin> pins;
};

// DevicePin, DevicePart:
// represent a device (LED, motor etc) in our system-wide device library
struct DevicePin
{
  QString name{};
  CapabilityType type{};
  enum
  {
    Either,
    Source,
    Sink,
  } direction{};
};

struct DevicePart
{
  QString type;
  std::vector<DevicePin> pins;
};

// Part, Port, Connection, Device:
// The elements of an actual schematic, which reference
// library elements.
struct Part
{
  QString type;
  QString id;
};

struct Port
{
  QString id;
  QString pin;
};

struct Connection
{
  Port source;
  Port sink;
};

struct Device
{
  std::vector<Part> parts;
  std::vector<Connection> connections;
};

struct DeviceLibrary
{
  std::vector<MCU> mcus;
  std::vector<BoardPart> boards;
  std::vector<DevicePart> devices;

  DeviceLibrary();

  void initBoards();
  void initMCUs();
  void initDevices();

  BoardPart readBoard(const QByteArray& board);

  static const DeviceLibrary& instance();
};
}
