#pragma once
#include <ossia/ossia.hpp>
#include <Process/TimeValue.hpp>

#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/ClipMode.hpp>
#include <Device/Address/Domain.hpp>
#include <Device/Address/IOType.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <State/Value.hpp>

#include <iscore_plugin_engine_export.h>

namespace ossia {
namespace net {
class node_base;
}
}

// Utility functions to convert from one node to another.
namespace Engine
{
namespace ossia_to_iscore
{


template<typename> struct MatchingType;
template<> struct MatchingType<float> {
        static constexpr const auto val = ossia::val_type::FLOAT;
        using type = ossia::Float;
        static auto convert(float f) { return State::Value::fromValue(f); }
};
template<> struct MatchingType<double> {
        static constexpr const auto val = ossia::val_type::FLOAT;
        using type = ossia::Float;
        static auto convert(double f) { return State::Value::fromValue(f); }
};
template<> struct MatchingType<int> {
        static constexpr const auto val = ossia::val_type::INT;
        using type = ossia::Int;
        static auto convert(int f) { return State::Value::fromValue(f); }
};
template<> struct MatchingType<bool> {
        static constexpr const auto val = ossia::val_type::BOOL;
        using type = ossia::Bool;
        static auto convert(bool f) { return State::Value::fromValue(f); }
};
template<> struct MatchingType<State::impulse_t> {
        static constexpr const auto val = ossia::val_type::IMPULSE;
        using type = ossia::Impulse;
        static auto convert(State::impulse_t) { return State::Value::fromValue(State::impulse_t{}); }
};
template<> struct MatchingType<std::string> {
        static constexpr const auto val = ossia::val_type::STRING;
        using type = ossia::String;
        static auto convert(const std::string& f) { return State::Value::fromValue(QString::fromStdString(f)); }
        static auto convert(std::string&& f) { return State::Value::fromValue(QString::fromStdString(std::move(f))); }
};
template<> struct MatchingType<QString> {
        static constexpr const auto val = ossia::val_type::STRING;
        using type = ossia::String;
        static auto convert(const QString& f) { return State::Value::fromValue(f); }
        static auto convert(QString&& f) { return State::Value::fromValue(std::move(f)); }
};
template<> struct MatchingType<char> {
        static constexpr const auto val = ossia::val_type::CHAR;
        using type = ossia::Char;
        static auto convert(char f) { return State::Value::fromValue(f); }
};
template<> struct MatchingType<QChar> {
        static constexpr const auto val = ossia::val_type::CHAR;
        using type = ossia::Char;
        static auto convert(QChar f) { return State::Value::fromValue(f); }
};
template<> struct MatchingType<State::vec2f> {
        static constexpr const auto val = ossia::val_type::VEC2F;
        using type = ossia::Vec2f;
        static auto convert(const State::vec2f& t) { return State::Value::fromValue(t); }
};
template<> struct MatchingType<State::vec3f> {
        static constexpr const auto val = ossia::val_type::VEC3F;
        using type = ossia::Vec3f;
        static auto convert(const State::vec3f& t) { return State::Value::fromValue(t); }
};
template<> struct MatchingType<State::vec4f> {
        static constexpr const auto val = ossia::val_type::VEC4F;
        using type = ossia::Vec4f;
        static auto convert(const State::vec4f& t) { return State::Value::fromValue(t); }
};
template<> struct MatchingType<State::tuple_t> {
        static constexpr const auto val = ossia::val_type::TUPLE;
        using type = ossia::Tuple;
        static auto convert(const State::tuple_t& t) { return State::Value::fromValue(t); }
        static auto convert(State::tuple_t&& t) { return State::Value::fromValue(std::move(t)); }
};
template<> struct MatchingType<::TimeValue> {
        static constexpr const auto val = ossia::val_type::FLOAT;
        using type = ossia::Float;
        static auto convert(::TimeValue&& t) { return State::Value::fromValue(t.msec()); }
};



Device::IOType ToIOType(ossia::access_mode t);
Device::ClipMode ToClipMode(ossia::bounding_mode b);
Device::Domain ToDomain(ossia::net::domain& domain);

ISCORE_PLUGIN_ENGINE_EXPORT State::Value ToValue(const ossia::value& val);
State::Value ToValue(ossia::val_type);

State::Address ToAddress(const ossia::net::node_base& node);
Device::AddressSettings ToAddressSettings(const ossia::net::node_base& node);
ISCORE_PLUGIN_ENGINE_EXPORT Device::Node ToDeviceExplorer(const ossia::net::node_base& node);


inline ::TimeValue time(ossia::time_value t)
{
    return t.isInfinite()
            ? ::TimeValue{PositiveInfinity{}}
            : ::TimeValue::fromMsecs(double(t));
}
}
}
