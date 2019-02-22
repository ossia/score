// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AnySerialization.hpp"

namespace score
{
score::hash_map<std::string, std::unique_ptr<any_serializer>>& anySerializers()
{
  static score::hash_map<std::string, std::unique_ptr<any_serializer>> ser;
  return ser;
}

any_serializer::~any_serializer()
{
}
}
