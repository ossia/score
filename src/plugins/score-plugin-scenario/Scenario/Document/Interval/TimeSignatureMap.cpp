#include "TimeSignatureMap.hpp"
#include <Process/TimeValueSerialization.hpp>
#include <score/serialization/MapSerialization.hpp>
#include <ossia/detail/flat_map.hpp>

namespace Scenario
{

struct TimeSignatureMap::impl : ossia::flat_map<TimeVal, ossia::time_signature>
{
    using flat_map::flat_map;

};

TimeSignatureMap::TimeSignatureMap()
{

}
TimeSignatureMap::~TimeSignatureMap()
{
    if(map)
        delete map;
}
TimeSignatureMap::TimeSignatureMap(const TimeSignatureMap& other)
{
    if(other.map)
    {
        map = new impl{*other.map};
    }
}
TimeSignatureMap::TimeSignatureMap(TimeSignatureMap&& other)
{
    map = other.map;
    other.map = nullptr;
}
TimeSignatureMap& TimeSignatureMap::operator=(const TimeSignatureMap& other)
{
  if(other.map)
  {
      map = new impl{*other.map};
  }
  return *this;
}
TimeSignatureMap& TimeSignatureMap::operator=(TimeSignatureMap&& other)
{
  map = other.map;
  other.map = nullptr;
  return *this;
}

void TimeSignatureMap::clear()
{
  if(map)
    map->clear();
}
bool TimeSignatureMap::empty() const noexcept
{
  if(map)
    return map->empty();
  else
    return true;
}
std::size_t TimeSignatureMap::size() const noexcept
{
  if(map)
    return map->size();
  else
    return 0;
}
ossia::time_signature TimeSignatureMap::at(const TimeVal& t)
{
  SCORE_ASSERT(map);
  return map->at(t);
}
ossia::time_signature& TimeSignatureMap::operator[](const TimeVal& t)
{
  if(!map)
    map = new impl;
  return (*map)[t];
}
TimeSignatureMap::const_iterator TimeSignatureMap::find(const TimeVal& t) const
{
  if(!map)
    map = new impl;
  return (*map).find(t).underlying;
}
TimeSignatureMap::const_iterator TimeSignatureMap::last_before(const TimeVal& t) const
{
  if(!map)
    map = new impl;
  return ossia::last_before(*map, t).underlying;
}
TimeSignatureMap::const_iterator TimeSignatureMap::upper_bound(const TimeVal& t) const
{
  if(!map)
    map = new impl;
  return map->upper_bound(t).underlying;
}
void TimeSignatureMap::erase(const_iterator t)
{
  if(!map)
    return;

  map->erase(t);
}
void TimeSignatureMap::erase(const TimeVal& t)
{
  if(!map)
    return;
  map->erase(t);
}

TimeSignatureMap::const_iterator TimeSignatureMap::begin() const
{
  if(!map)
    map = new impl;
  return map->begin().underlying;
}
TimeSignatureMap::const_iterator TimeSignatureMap::end() const
{
  if(!map)
    map = new impl;
  return map->end().underlying;
}

bool TimeSignatureMap::operator!=(const TimeSignatureMap& other) const noexcept
{
  if(!map && !other.map)
    return false;
  else if(!map && other.map)
    return true;
  else if(map && !other.map)
    return true;
  else // (map && other.map)
    return *map != *other.map;
}
}

void TSerializer<DataStream, Scenario::TimeSignatureMap>::readFrom(DataStream::Serializer& s, const Scenario::TimeSignatureMap& path)
{
  if(!path.map)
    path.map = new Scenario::TimeSignatureMap::impl;
  TSerializer<DataStream, ossia::flat_map<TimeVal, ossia::time_signature>>::readFrom(s, *path.map);
}

void TSerializer<DataStream, Scenario::TimeSignatureMap>::writeTo(DataStream::Deserializer& s, Scenario::TimeSignatureMap& path)
{
  if(!path.map)
    path.map = new Scenario::TimeSignatureMap::impl;
  TSerializer<DataStream, ossia::flat_map<TimeVal, ossia::time_signature>>::writeTo(s, *path.map);
}

void TSerializer<JSONObject, Scenario::TimeSignatureMap>::readFrom(JSONObject::Serializer& s, const Scenario::TimeSignatureMap& path)
{
  if(!path.map)
    path.map = new Scenario::TimeSignatureMap::impl;
  TSerializer<JSONObject, ossia::flat_map<TimeVal, ossia::time_signature>>::readFrom(s, *path.map);
}

void TSerializer<JSONObject, Scenario::TimeSignatureMap>::writeTo(JSONObject::Deserializer& s, Scenario::TimeSignatureMap& path)
{
  if(!path.map)
    path.map = new Scenario::TimeSignatureMap::impl;
  TSerializer<JSONObject, ossia::flat_map<TimeVal, ossia::time_signature>>::writeTo(s, *path.map);
}
