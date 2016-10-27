#pragma once
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore_lib_state_export.h>
#include <iscore/tools/std/Optional.hpp>
#include <ossia/editor/value/impulse.hpp>
#include <eggs/variant.hpp>
#include <QChar>
#include <QList>
#include <QString>
#include <algorithm>
#include <vector>

class DataStream;
class JSONObject;
class QDebug;

namespace State
{
using impulse_t = ossia::Impulse;

class ValueImpl;
using vec2f = std::array<float, 2>;
using vec3f = std::array<float, 3>;
using vec4f = std::array<float, 4>;
using tuple_t = std::vector<ValueImpl>;
enum class ValueType : std::size_t { Impulse, Int, Float, Bool, String, Char, Vec2f, Vec3f, Vec4f, Tuple, NoValue = eggs::variant<>::npos};

class ISCORE_LIB_STATE_EXPORT ValueImpl
{
        ISCORE_SERIALIZE_FRIENDS(ValueImpl, DataStream)
        ISCORE_SERIALIZE_FRIENDS(ValueImpl, JSONObject)

    public:
        using variant_t = eggs::variant<impulse_t, int, float, bool, std::string, char, vec2f, vec3f, vec4f, tuple_t>;
        ValueImpl() = default;
        ValueImpl(const ValueImpl&) = default;
        ValueImpl(ValueImpl&&) = default;

        ValueImpl& operator=(const ValueImpl&) = default;
        ValueImpl& operator=(ValueImpl&&) = default;

        ValueImpl(impulse_t v);
        ValueImpl(int v);
        ValueImpl(float v);
        ValueImpl(double v);
        ValueImpl(bool v);
        ValueImpl(const QString& v);
        ValueImpl(std::string v);
        ValueImpl(QChar v);
        ValueImpl(char v);
        ValueImpl(vec2f v);
        ValueImpl(vec3f v);
        ValueImpl(vec4f v);
        ValueImpl(tuple_t v);

        ValueImpl& operator=(impulse_t v);
        ValueImpl& operator=(int v);
        ValueImpl& operator=(float v);
        ValueImpl& operator=(double v);
        ValueImpl& operator=(bool v);
        ValueImpl& operator=(const QString& v);
        ValueImpl& operator=(QString&& v);
        ValueImpl& operator=(const std::string& v);
        ValueImpl& operator=(std::string&& v);
        ValueImpl& operator=(QChar v);
        ValueImpl& operator=(char v);
        ValueImpl& operator=(vec2f v);
        ValueImpl& operator=(vec3f v);
        ValueImpl& operator=(vec4f v);
        ValueImpl& operator=(const tuple_t& v);
        ValueImpl& operator=(tuple_t&& v);

        const auto& impl() const { return m_variant; }

        bool operator ==(const ValueImpl& other) const;
        bool operator !=(const ValueImpl& other) const;

        bool isNumeric() const;
        bool isValid() const;
        bool isArray() const;

        ValueType which() const
        {
            return static_cast<ValueType>(m_variant.which());
        }

        template<typename TheType>
        bool is() const
        { return m_variant.target<TheType>() != nullptr; }

        template<typename TheType>
        TheType get() const
        { return eggs::variants::get<TheType>(m_variant); }

    private:
        variant_t m_variant;
};

template<>
inline QString ValueImpl::get<QString>() const
{ return QString::fromStdString(eggs::variants::get<std::string>(m_variant)); }
template<>
inline QChar ValueImpl::get<QChar>() const
{ return QChar(eggs::variants::get<char>(m_variant)); }

template<>
inline bool ValueImpl::is<QString>() const
{ return is<std::string>(); }
template<>
inline bool ValueImpl::is<QChar>() const
{ return is<char>(); }
/**
 * @brief The Value struct
 *
 * A variant used to represent the data that can be in a message.
 *
 */
struct ISCORE_LIB_STATE_EXPORT Value
{
        using value_type = ValueImpl;
        value_type val{};

        template<typename Val>
        static Value fromValue(Val&& val)
        {
            return State::Value{std::forward<Val>(val)};
        }

        Value() = default;
        Value(value_type v): val(std::move(v)) {}
        Value(const Value&) = default;
        Value(Value&&) = default;
        Value& operator=(const Value&) = default;
        Value& operator=(Value&&) = default;

        bool operator==(const Value& m) const;
        bool operator!=(const Value& m) const;
        //bool operator<(const Value& m) const;
};

using ValueList = QList<Value>;
using OptionalValue = optional<State::Value>;

ISCORE_LIB_STATE_EXPORT QDebug& operator<<(QDebug& s, const Value& m);

// Hopefully we won't need this for much longer.
ISCORE_LIB_STATE_EXPORT ossia::value toOSSIAValue(const State::ValueImpl& val);
ISCORE_LIB_STATE_EXPORT State::Value fromOSSIAValue(const ossia::value& val);

}

Q_DECLARE_METATYPE(State::impulse_t)
Q_DECLARE_METATYPE(State::Value)
Q_DECLARE_METATYPE(State::ValueList)
Q_DECLARE_METATYPE(State::ValueType)

Q_DECLARE_METATYPE(State::vec2f)
Q_DECLARE_METATYPE(State::vec3f)
Q_DECLARE_METATYPE(State::vec4f)
