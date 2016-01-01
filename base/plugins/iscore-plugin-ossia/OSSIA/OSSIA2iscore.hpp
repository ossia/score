#pragma once
#include <API/Headers/Editor/TimeValue.h>
#include <API/Headers/Network/Address.h>
#include <Process/TimeValue.hpp>

#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/ClipMode.hpp>
#include <Device/Address/Domain.hpp>
#include <Device/Address/IOType.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <State/Value.hpp>

#include <iscore_plugin_ossia_export.h>

namespace OSSIA {
class Domain;
class Node;
class Value;
}  // namespace OSSIA

// Utility functions to convert from one node to another.
namespace Ossia
{
namespace convert
{


template<typename> struct MatchingType;
template<> struct MatchingType<float> {
        static constexpr const auto val = OSSIA::Value::Type::FLOAT;
        using type = OSSIA::Float;
        static auto convert(float f) { return State::Value::fromValue(f); }
};
template<> struct MatchingType<double> {
        static constexpr const auto val = OSSIA::Value::Type::FLOAT;
        using type = OSSIA::Float;
        static auto convert(double f) { return State::Value::fromValue(f); }
};
template<> struct MatchingType<int> {
        static constexpr const auto val = OSSIA::Value::Type::INT;
        using type = OSSIA::Int;
        static auto convert(int f) { return State::Value::fromValue(f); }
};
template<> struct MatchingType<bool> {
        static constexpr const auto val = OSSIA::Value::Type::BOOL;
        using type = OSSIA::Bool;
        static auto convert(bool f) { return State::Value::fromValue(f); }
};
template<> struct MatchingType<State::impulse_t> {
        static constexpr const auto val = OSSIA::Value::Type::IMPULSE;
        using type = OSSIA::Impulse;
        static auto convert(State::impulse_t) { return State::Value::fromValue(State::impulse_t{}); }
};
template<> struct MatchingType<std::string> {
        static constexpr const auto val = OSSIA::Value::Type::STRING;
        using type = OSSIA::String;
        static auto convert(const std::string& f) { return State::Value::fromValue(QString::fromStdString(f)); }
        static auto convert(std::string&& f) { return State::Value::fromValue(QString::fromStdString(std::move(f))); }
};
template<> struct MatchingType<QString> {
        static constexpr const auto val = OSSIA::Value::Type::STRING;
        using type = OSSIA::String;
        static auto convert(const QString& f) { return State::Value::fromValue(f); }
        static auto convert(QString&& f) { return State::Value::fromValue(std::move(f)); }
};
template<> struct MatchingType<char> {
        static constexpr const auto val = OSSIA::Value::Type::CHAR;
        using type = OSSIA::Char;
        static auto convert(char f) { return State::Value::fromValue(f); }
};
template<> struct MatchingType<QChar> {
        static constexpr const auto val = OSSIA::Value::Type::CHAR;
        using type = OSSIA::Char;
        static auto convert(QChar f) { return State::Value::fromValue(f); }
};
template<> struct MatchingType<State::tuple_t> {
        static constexpr const auto val = OSSIA::Value::Type::TUPLE;
        using type = OSSIA::Tuple;
        static auto convert(const State::tuple_t& t) { return State::Value::fromValue(t); }
        static auto convert(State::tuple_t&& t) { return State::Value::fromValue(std::move(t)); }
};
template<> struct MatchingType<::TimeValue> {
        static constexpr const auto val = OSSIA::Value::Type::FLOAT;
        using type = OSSIA::Float;
        static auto convert(::TimeValue&& t) { return State::Value::fromValue(t.msec()); }
};



Device::IOType ToIOType(OSSIA::Address::AccessMode t);
Device::ClipMode ToClipMode(OSSIA::Address::BoundingMode b);
Device::Domain ToDomain(OSSIA::Domain& domain);

ISCORE_PLUGIN_OSSIA_EXPORT
State::Value ToValue(const OSSIA::Value* val);
State::Value ToValue(OSSIA::Value::Type);

State::Address ToAddress(const OSSIA::Node& node);
Device::AddressSettings ToAddressSettings(const OSSIA::Node& node);
Device::Node ToDeviceExplorer(const OSSIA::Node& node);


inline ::TimeValue time(const OSSIA::TimeValue& t)
{
    return t.isInfinite()
            ? ::TimeValue{PositiveInfinity{}}
            : ::TimeValue::fromMsecs(double(t));
}
}
}
