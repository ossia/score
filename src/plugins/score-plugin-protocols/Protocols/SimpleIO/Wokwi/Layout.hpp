#pragma once

#include <Protocols/SimpleIO/SimpleIOSpecificSettings.hpp>

namespace Protocols
{
SimpleIOSpecificSettings loadWokwi(const rapidjson::Document& doc);
}
