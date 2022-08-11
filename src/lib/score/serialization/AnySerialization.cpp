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

any_serializer::~any_serializer() { }

void any_serializer::cast_error(const char* err)
{
  qWarning() << "Could not read boost::any" << err;
}
}

void apply(DataStreamReader& s, const std::string& key, const ossia::any& v)
{
  auto& ser = score::anySerializers();
  auto it = ser.find(key);
  if(it != ser.end())
  {
    it.value()->apply(s, v);
  }
  else
  {
    SCORE_TODO;
  }
}

void apply(DataStreamWriter& s, const std::string& key, ossia::any& v)
{
  auto& ser = score::anySerializers();
  auto it = ser.find(key);
  if(it != ser.end())
  {
    it.value()->apply(s, v);
  }
  else
  {
    SCORE_TODO;
  }
}

void apply(JSONWriter& s, const std::string& key, ossia::any& v)
{
  auto& ser = score::anySerializers();
  auto it = ser.find(key);
  if(it != ser.end())
  {
    it.value()->apply(s, key, v);
  }
  else
  {
    SCORE_TODO;
  }
}

void apply(JSONReader& s, const std::string& key, const ossia::any& v)
{
  auto& ser = score::anySerializers();
  auto it = ser.find(key);
  if(it != ser.end())
  {
    it.value()->apply(s, key, v);
  }
  else
  {
    SCORE_TODO;
  }
}
