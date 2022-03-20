#pragma once
#include <verdigris>
#include <vector>
#include <utility>
#include <Process/TimeValue.hpp>
#include <ossia/editor/scenario/time_signature.hpp>
#include <score/serialization/VisitorInterface.hpp>

#include <score_plugin_scenario_export.h>
// Indirected because compile times were too high.
namespace Scenario
{
struct TimeSignatureMap;
}
template<>
struct TSerializer<DataStream, Scenario::TimeSignatureMap>;
template<>
struct TSerializer<JSONObject, Scenario::TimeSignatureMap>;
namespace Scenario
{

struct SCORE_PLUGIN_SCENARIO_EXPORT TimeSignatureMap
{
    friend TimeSignatureMap;
    friend TSerializer<DataStream, TimeSignatureMap>;
    friend TSerializer<JSONObject, TimeSignatureMap>;
public:
    using iterator = std::vector<std::pair<TimeVal, ossia::time_signature>>::iterator;
    using const_iterator = std::vector<std::pair<TimeVal, ossia::time_signature>>::const_iterator;
    explicit TimeSignatureMap();
    ~TimeSignatureMap();
    TimeSignatureMap(const TimeSignatureMap& other);
    TimeSignatureMap(TimeSignatureMap&& other);
    TimeSignatureMap& operator=(const TimeSignatureMap& other);
    TimeSignatureMap& operator=(TimeSignatureMap&& other);

    void clear();
    bool empty() const noexcept;
    std::size_t size() const noexcept;
    ossia::time_signature at(const TimeVal&);
    ossia::time_signature& operator[](const TimeVal&);
    const_iterator find(const TimeVal&) const;
    const_iterator last_before(const TimeVal&) const;
    const_iterator upper_bound(const TimeVal&) const;
    void erase(const_iterator);
    void erase(const TimeVal& t);

    const_iterator begin() const;
    const_iterator end() const;

    bool operator!=(const TimeSignatureMap& other) const noexcept;

private:
    struct impl;
    mutable impl* map{};
};
}

template <>
struct is_custom_serialized<Scenario::TimeSignatureMap> : std::true_type
{
};

template <>
struct TSerializer<DataStream, Scenario::TimeSignatureMap>
{
  static void readFrom(DataStream::Serializer& s, const Scenario::TimeSignatureMap& path);
  static void writeTo(DataStream::Deserializer& s, Scenario::TimeSignatureMap& path);
};

template <>
struct TSerializer<JSONObject, Scenario::TimeSignatureMap>
{
  static void readFrom(JSONObject::Serializer& s, const Scenario::TimeSignatureMap& path);
  static void writeTo(JSONObject::Deserializer& s, Scenario::TimeSignatureMap& path);
};
Q_DECLARE_METATYPE(Scenario::TimeSignatureMap)
W_REGISTER_ARGTYPE(Scenario::TimeSignatureMap)
