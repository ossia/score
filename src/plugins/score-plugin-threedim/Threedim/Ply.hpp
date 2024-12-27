#pragma once
#include <Threedim/TinyObj.hpp>

namespace Threedim
{
std::vector<mesh> PlyFromFile(std::string_view filename, float_vec& data);
}
