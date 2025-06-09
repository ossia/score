#pragma once

#include <Protocols/SimpleIO/SimpleIOSpecificSettings.hpp>

#include <ossia/detail/json_fwd.hpp>
namespace Protocols
{
SimpleIOSpecificSettings loadWokwi(const rapidjson::Document& doc);
}
