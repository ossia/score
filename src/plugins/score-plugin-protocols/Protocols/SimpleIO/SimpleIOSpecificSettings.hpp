#pragma once
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_SIMPLEIO)
#include <score/tools/std/StringHash.hpp>

#include <ossia/detail/variant.hpp>

#include <QString>

#include <utility>
#include <vector>
#include <verdigris>

namespace Protocols
{

namespace SimpleIO
{
struct GPIO
{
  int32_t chip{};
  int32_t line{};
  int32_t flags{};
  int32_t events{};
  int32_t state{};
  bool direction{};
};
struct PWM
{
  int32_t chip{};
  int32_t channel{};
};
struct ADC
{
  int32_t chip{};
  int32_t channel{};
};
struct DAC
{
  int32_t chip{};
  int32_t channel{};
};
struct HID
{
  int32_t chip{};
  int32_t channel{};
};
struct Custom
{
};

using Type = ossia::variant<GPIO, PWM, ADC, DAC, HID, Custom>;

struct Port
{
  Type control;
  QString name;
  QString path;
};

}

struct SimpleIOSpecificSettings
{
  std::vector<SimpleIO::Port> ports;
};
}

Q_DECLARE_METATYPE(Protocols::SimpleIOSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::SimpleIOSpecificSettings)
#endif
