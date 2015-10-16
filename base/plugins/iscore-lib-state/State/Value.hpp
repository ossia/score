#pragma once
#include <QVariant>
#include <iscore/tools/Todo.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <boost/optional.hpp>
#include <eggs/variant.hpp>


namespace iscore
{
struct impulse_t {};
class ValueImpl;
using tuple_t = std::vector<ValueImpl>;

class ValueImpl
{
    public:
        using variant_t = eggs::variant<impulse_t, int, float, bool, QString, QChar, tuple_t>;
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
        ValueImpl(QChar v);
        ValueImpl(const tuple_t& v);

        ValueImpl& operator=(impulse_t v);
        ValueImpl& operator=(int v);
        ValueImpl& operator=(float v);
        ValueImpl& operator=(double v);
        ValueImpl& operator=(bool v);
        ValueImpl& operator=(const QString& v);
        ValueImpl& operator=(QChar v);
        ValueImpl& operator=(const tuple_t& v);

        const auto& impl() const { return m_variant; }

        const char* typeName() const;
        int type() const;
        bool operator ==(const ValueImpl& other) const;
        bool operator !=(const ValueImpl& other) const;

        bool isValid() const;
        int toInt() const;
        float toFloat() const;
        float toFloat(bool*) const;
        double toDouble() const;
        bool toBool() const;
        QString toString() const;
        QStringList toStringList() const;
        QChar toChar() const;
        tuple_t toTuple() const;

        template<typename TheType>
        bool canConvert() const
        { return {}; }

        template<typename TheType>
        TheType value() const
        { return {}; }


        friend QDebug& operator<<(QDebug& s, const ValueImpl& m);
        friend QDataStream& operator<<(QDataStream& s, const ValueImpl& m);

        friend QDataStream& operator>>(QDataStream& s, ValueImpl& m);

    private:
        variant_t m_variant;
};
/**
 * @brief The Value struct
 *
 * A variant used to represent the data that can be in a message.
 *
 */
struct Value
{
        using value_type = ValueImpl;
        value_type val{};

        friend QDataStream& operator<<(QDataStream& s, const Value& m);

        friend QDataStream& operator>>(QDataStream& s, Value& m);

        template<typename Val>
        static Value fromValue(Val&& val)
        {
            return iscore::Value{value_type{std::forward<Val>(val)}};
        }

        static Value fromVariant(const value_type& var);

        Value() = default;
        Value(const Value&) = default;
        Value(Value&&) = default;
        Value& operator=(const Value&) = default;
        Value& operator=(Value&&) = default;

        bool operator==(const Value& m) const;

        bool operator!=(const Value& m) const;

        bool operator<(const Value& m) const;

        static iscore::Value fromQVariant(const QVariant& var);
        QVariant toQVariant() const;
        QString toString() const;
        /*
        template<typename Fun> auto map(Fun&& f)
        {
            // todo make a generic switch like serialize_dyn.
        }
        */
};

using ValueList = QList<Value>;
using OptionalValue = boost::optional<iscore::Value>;
}

Q_DECLARE_METATYPE(iscore::Value)
Q_DECLARE_METATYPE(iscore::ValueList)

