#include "Layout.hpp"

#include <Protocols/SimpleIO/Wokwi/Library.hpp>

#include <ossia/detail/json.hpp>

#include <QDebug>
#include <QStringList>
#define SCORE_FLAG(i) (1 << i)
namespace Protocols
{

SimpleIOSpecificSettings loadWokwi(const rapidjson::Document& doc)
{
  static const auto lib = Wokwi::DeviceLibrary::instance();
  SimpleIOSpecificSettings vec;
  using namespace Wokwi;
  // 1. Load all the parts from the JSON
  Wokwi::Device d;
  if(!doc.IsObject())
    return vec;

  rapidjson::Document::ConstMemberIterator parts_it;
  if(parts_it = doc.FindMember("parts"); parts_it == doc.MemberEnd())
    return vec;
  if(!parts_it->value.IsArray())
    return vec;

  rapidjson::Document::ConstMemberIterator connections_it;
  if(connections_it = doc.FindMember("connections"); connections_it == doc.MemberEnd())
    return vec;
  if(!connections_it->value.IsArray())
    return vec;

  rapidjson::Document::ConstArray parts = parts_it->value.GetArray();
  rapidjson::Document::ConstArray connections = connections_it->value.GetArray();

  for(auto& part : parts)
  {
    // { "type": "wokwi-servo", "id": "servo1", "top": -280.4, "left": -182.4, "attrs": {} },
    if(!part.IsObject())
      return vec;

    Part p;
    if(auto type = part.FindMember("type");
       type != part.MemberEnd() && type->value.IsString())
      p.type = type->value.GetString();
    if(auto id = part.FindMember("id"); id != part.MemberEnd() && id->value.IsString())
      p.id = id->value.GetString();

    if(!p.type.isEmpty() && !p.id.isEmpty())
      d.parts.push_back(p);
  }

  for(auto& cc : connections)
  {
    // [ "led1:C", "bb1:10t.a", "green", [ "v0" ] ],
    if(!cc.IsArray())
      return vec;

    auto con = cc.GetArray();
    if(con.Size() < 2)
      continue;
    if(!con[0].IsString() && !con[1].IsString())
      continue;

    Port p_source;
    Port p_sink;
    {
      QString s_source = QString::fromUtf8(con[0].GetString());
      if(auto split = s_source.split(':'); split.size() >= 2)
      {
        p_source.id = split[0];
        p_source.pin = split[1];
      }
      else
      {
        continue;
      }
    }
    {
      QString s_sink = QString::fromUtf8(con[1].GetString());
      if(auto split = s_sink.split(':'); split.size() >= 2)
      {
        p_sink.id = split[0];
        p_sink.pin = split[1];
      }
      else
      {
        continue;
      }
    }

    Connection c;
    c.source = p_source;
    c.sink = p_sink;

    if(c.source.id.isEmpty() || c.sink.id.isEmpty())
      continue;
    if(c.source.pin.isEmpty() || c.sink.pin.isEmpty())
      continue;

    d.connections.push_back(c);
  }

  // TODO: simplify the graph. For instance, remove resistors, etc.
  // Some neat application of boost.graph here...

  // 2. Check what we have connected: we want to know if they're:
  // - GPIOs
  // - PWMs
  // - ADCs
  // - DACs
  // ...

  // Locate the board id: esp, uno, etc
  QString board_id;
  const BoardPart* this_board{};
  for(auto& part : d.parts)
  {
    if(part.type.startsWith("board-"))
    {
      board_id = part.id;
      for(auto& board : lib.boards)
      {
        if(board.type == part.type)
        {
          this_board = &board;
          break;
        }
      }
      break;
    }
  }
  if(!this_board)
  {
    qDebug() << "Could not find board";
    return {};
  }

  // Locate the MCU
  const MCU* this_mcu = ossia::ptr_find_if(
      lib.mcus, [&](const MCU& mcu) { return mcu.name == this_board->mcu; });
  if(!this_mcu)
  {
    qDebug() << "Missing MCU: " << this_board->mcu;
    return {};
  }

  // Locate the stuff plugged on the board
  for(auto& con : d.connections)
  {
    if(!(con.source.id == board_id || con.sink.id == board_id))
      continue;
    Port board_port = con.source.id == board_id ? con.source : con.sink;
    Port other_port = con.source.id != board_id ? con.source : con.sink;

    // What our uc pin is plugged to, in the schematic
    Part other;
    for(const auto& part : d.parts)
    {
      if(part.id == other_port.id)
      {
        other = part;
        break;
      }
    }
    if(other.type.isEmpty())
    {
      qDebug() << "Could not find matching part for " << other_port.id;
      continue;
    }

    // Find library info on these:
    // 1. The board side
    const BoardPin* board_pin = ossia::ptr_find_if(
        this_board->pins, [&](auto& p) { return p.name == board_port.pin; });
    if(!board_pin)
    {
      qDebug() << "board_pin" << board_port.pin << "not found";
      continue;
    }
    const MCUPin* mcu_pin = ossia::ptr_find(this_mcu->pins, board_pin->target);
    if(!mcu_pin)
    {
      qDebug() << "mcu_pin" << board_pin->target << "not found";
      continue;
    }

    // 2. The device side
    const DevicePart* device = ossia::ptr_find_if(
        lib.devices, [&](auto& dev) { return dev.type == other.type; });
    if(!device)
    {
      qDebug() << "device" << other.id << ":" << other.type << "not found";
      continue;
    }
    const DevicePin* device_pin = ossia::ptr_find_if(
        device->pins, [&](auto& p) { return p.name == other_port.pin; });
    if(!device_pin)
    {
      qDebug() << "device_pin" << other_port.id << ":" << other_port.pin << "not found";
      continue;
    }

    // 3. Discover the characteristics of the connection
    enum Direction
    {
      Input,
      Output
    };

    std::optional<Direction> direction{};

    // Try to guess direction with the device pin
    if(device_pin->direction == device_pin->Sink)
      direction = Output;
    else
      direction = Input;

    // Try to guess direction with the device type
    if(!direction)
    {
      // TODO ?
    }

    // 4. Create the endpoint
    SimpleIO::Port ctl; //{.control = ctl, .name = {"foo"}, .path = {"/0/0"}}
    ctl.name = other_port.pin;
    ctl.path = other_port.id + "/" + other_port.pin;

    switch(device_pin->type)
    {
      case CapabilityType::GPIO:
        if(auto gpio = mcu_pin->gpio())
          ctl.control = SimpleIO::GPIO{
              .chip = 0,
              .line = gpio->line,
              .flags = 0,
              .events = 0,
              .direction = (direction == Input ? false : true)};
        break;
      case CapabilityType::PWM:
        if(auto pwm = mcu_pin->pwm())
          ctl.control = SimpleIO::PWM{.chip = pwm->device, .channel = pwm->channel};
        break;
      case CapabilityType::Analog:
        if(direction == Input)
        {
          if(auto adc = mcu_pin->adc())
            ctl.control = SimpleIO::ADC{.chip = adc->device, .channel = adc->channel};
        }
        else
        {
          if(auto dac = mcu_pin->dac())
            ctl.control = SimpleIO::DAC{.chip = dac->device, .channel = dac->channel};
        }
        break;
      default:
        qDebug() << "device_pin type unhandled";
        break;
    }

    vec.ports.push_back(ctl);
  }

  vec.board = board_id;
  qDebug() << vec.board;
  return vec;
}
}
