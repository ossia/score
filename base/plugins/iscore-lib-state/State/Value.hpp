#pragma once
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore_lib_state_export.h>
#include <boost/optional/optional.hpp>
#include <eggs/variant.hpp>
#include <QChar>
#include <QList>
#include <QString>
#include <algorithm>
#include <vector>

class DataStream;
class JSONObject;
class QDebug;

namespace iscore
{
struct no_value_t {};
inline bool operator==(no_value_t, no_value_t) { return true; }
inline bool operator!=(no_value_t, no_value_t) { return false; }
struct impulse_t {};
inline bool operator==(impulse_t, impulse_t) { return true; }
inline bool operator!=(impulse_t, impulse_t) { return false; }

class ValueImpl;

using tuple_t = std::vector<ValueImpl>;
enum class ValueType { Impulse, Int, Float, Bool, String, Char, Tuple, NoValue };

class ISCORE_LIB_STATE_EXPORT ValueImpl
{
        ISCORE_SERIALIZE_FRIENDS(ValueImpl, DataStream)
        ISCORE_SERIALIZE_FRIENDS(ValueImpl, JSONObject)

    public:
        using variant_t = eggs::variant<impulse_t, int, float, bool, QString, QChar, tuple_t, no_value_t>;
        ValueImpl();
        ValueImpl(const ValueImpl&) = default;
        ValueImpl(ValueImpl&&) = default;

        ValueImpl& operator=(const ValueImpl&) = default;
        ValueImpl& operator=(ValueImpl&&) = default;

        ValueImpl(no_value_t v);
        ValueImpl(impulse_t v);
        ValueImpl(int v);
        ValueImpl(float v);
        ValueImpl(double v);
        ValueImpl(bool v);
        ValueImpl(const QString& v);
        ValueImpl(QChar v);
        ValueImpl(const tuple_t& v);

        ValueImpl& operator=(no_value_t v);
        ValueImpl& operator=(impulse_t v);
        ValueImpl& operator=(int v);
        ValueImpl& operator=(float v);
        ValueImpl& operator=(double v);
        ValueImpl& operator=(bool v);
        ValueImpl& operator=(const QString& v);
        ValueImpl& operator=(QChar v);
        ValueImpl& operator=(const tuple_t& v);

        const auto& impl() const { return m_variant; }

        bool operator ==(const ValueImpl& other) const;
        bool operator !=(const ValueImpl& other) const;

        bool isNumeric() const;
        bool isValid() const;

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

        friend QDebug& operator<<(QDebug& s, const ValueImpl& m);

    private:
        variant_t m_variant;
};
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
            return iscore::Value{std::forward<Val>(val)};
        }

        Value() = default;
        Value(const value_type& v): val(v) {}
        Value(value_type&& v): val(std::move(v)) {}
        Value(const Value&) = default;
        Value(Value&&) = default;
        Value& operator=(const Value&) = default;
        Value& operator=(Value&&) = default;

        bool operator==(const Value& m) const;
        bool operator!=(const Value& m) const;
        bool operator<(const Value& m) const;
};

using ValueList = QList<Value>;
using OptionalValue = boost::optional<iscore::Value>;
}

Q_DECLARE_METATYPE(iscore::no_value_t)
Q_DECLARE_METATYPE(iscore::impulse_t)
Q_DECLARE_METATYPE(iscore::Value)
Q_DECLARE_METATYPE(iscore::ValueList)

