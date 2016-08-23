#include <eggs/variant/variant.hpp>
#include <QJsonArray>
#include <QList>
#include <QLocale>
#include <QMetaType>
#include <QObject>

#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QStringList>
#include <algorithm>
#include <array>
#include <iterator>

#include <State/Value.hpp>
#include "ValueConversion.hpp"
#include "Expression.hpp"

namespace State
{
namespace convert
{

const std::array<const QString, 11> ValuePrettyTypes{{
        QObject::tr("Impulse"),
                QObject::tr("Int"),
                QObject::tr("Float"),
                QObject::tr("Bool"),
                QObject::tr("String"),
                QObject::tr("Char"),
                QObject::tr("Vec2f"),
                QObject::tr("Vec3f"),
                QObject::tr("Vec4f"),
                QObject::tr("Tuple"),
                QObject::tr("Container")}};

const std::array<std::pair<QString, ValueType>, 10>
    ValuePrettyTypesPairArray{{
        std::make_pair(QObject::tr("Impulse"), ValueType::Impulse),
        std::make_pair(QObject::tr("Int"), ValueType::Int),
        std::make_pair(QObject::tr("Float"), ValueType::Float),
        std::make_pair(QObject::tr("Bool"), ValueType::Bool),
        std::make_pair(QObject::tr("String"), ValueType::String),
        std::make_pair(QObject::tr("Char"), ValueType::Char),
        std::make_pair(QObject::tr("Vec2f"), ValueType::Vec2f),
        std::make_pair(QObject::tr("Vec3f"), ValueType::Vec3f),
        std::make_pair(QObject::tr("Vec4f"), ValueType::Vec4f),
        std::make_pair(QObject::tr("Tuple"), ValueType::Tuple)
                              }};

template<>
QVariant value(const State::Value& val)
{
    struct vis {
        public:
            using return_type = QVariant;
            return_type operator()(const no_value_t&) const { return QVariant::fromValue(State::no_value_t{}); }
            return_type operator()(const impulse_t&) const { return QVariant::fromValue(State::impulse_t{}); }
            return_type operator()(int i) const { return QVariant::fromValue(i); }
            return_type operator()(float f) const { return QVariant::fromValue(f); }
            return_type operator()(bool b) const { return QVariant::fromValue(b); }
            return_type operator()(const QString& s) const { return QVariant::fromValue(s); }
            return_type operator()(QChar c) const { return QVariant::fromValue(c); }

            return_type operator()(vec2f t) const { return QVector2D{t[0], t[1]}; }
            return_type operator()(vec3f t) const { return QVector3D{t[0], t[1], t[2]}; }
            return_type operator()(vec4f t) const { return QVector4D{t[0], t[1], t[2], t[3]}; }
            return_type operator()(const tuple_t& t) const
            {
                QVariantList arr;
                arr.reserve(t.size());

                for(const auto& elt : t)
                {
                    arr.push_back(eggs::variants::apply(*this, elt.impl()));
                }

                return arr;
            }
    };

    return eggs::variants::apply(vis{}, val.val.impl());
}

template<>
QJsonValue value(const State::Value& val)
{
    struct vis {
        public:
            using return_type = QJsonValue;
            return_type operator()(const no_value_t&) const { return {}; }
            return_type operator()(const impulse_t&) const { return {}; }
            return_type operator()(int i) const { return i; }
            return_type operator()(float f) const { return f; }
            return_type operator()(bool b) const { return b; }
            return_type operator()(const QString& s) const { return s; }

            return_type operator()(QChar c) const
            {
                // Note : it is saved as a string but the actual type should be saved also
                // so that the QChar can be recovered.
                return QString(c);
            }

            return_type operator()(vec2f t) const { return QJsonArray{t[0], t[1]}; }
            return_type operator()(vec3f t) const { return QJsonArray{t[0], t[1], t[2]}; }
            return_type operator()(vec4f t) const { return QJsonArray{t[0], t[1], t[2], t[3]}; }

            return_type operator()(const tuple_t& t) const
            {
                QJsonArray arr;
                auto& strings = iscore::StringConstant();

                for(const auto& elt : t)
                {
                    QJsonObject obj;
                    obj[strings.Type] = textualType(elt);
                    obj[strings.Value] = eggs::variants::apply(*this, elt.impl());
                    arr.append(obj);
                }

                return arr;
            }
    };

    return eggs::variants::apply(vis{}, val.val.impl());
}


QString textualType(const State::Value& val)
{
    struct vis {
        public:
            using return_type = QString;
            return_type operator()(no_value_t) const { return QStringLiteral("None"); }
            return_type operator()(impulse_t) const { return QStringLiteral("Impulse"); }
            return_type operator()(int i) const { return QStringLiteral("Int"); }
            return_type operator()(float f) const { return QStringLiteral("Float"); }
            return_type operator()(bool b) const { return QStringLiteral("Bool"); }
            return_type operator()(const QString& s) const { return QStringLiteral("String"); }
            return_type operator()(QChar c) const { return QStringLiteral("Char"); }
            return_type operator()(vec2f t) const { return QStringLiteral("Vec2f"); }
            return_type operator()(vec3f t) const { return QStringLiteral("Vec3f"); }
            return_type operator()(vec4f t) const { return QStringLiteral("Vec4f"); }
            return_type operator()(const tuple_t& t) const { return QStringLiteral("Tuple"); }
    };

    return eggs::variants::apply(vis{}, val.val.impl());
}

const QHash<QString, State::ValueType> ValueTypesMap{
    { QStringLiteral("Impulse"), State::ValueType::Impulse },
    { QStringLiteral("Int"), State::ValueType::Int },
    { QStringLiteral("Float"), State::ValueType::Float },
    { QStringLiteral("Bool"), State::ValueType::Bool },
    { QStringLiteral("String"), State::ValueType::String },
    { QStringLiteral("Char"), State::ValueType::Char },
    { QStringLiteral("Vec2f"), State::ValueType::Vec2f },
    { QStringLiteral("Vec3f"), State::ValueType::Vec3f },
    { QStringLiteral("Vec4f"), State::ValueType::Vec4f },
    { QStringLiteral("Tuple"), State::ValueType::Tuple },
    { QStringLiteral("None"), State::ValueType::NoValue }
};

static ValueType which(const QString& val)
{
    auto it = ValueTypesMap.find(val);
    ISCORE_ASSERT(it != ValueTypesMap.end()); // What happens if there is a corrupt save file ?
    return static_cast<State::ValueType>(*it);
}

static State::ValueImpl fromQJsonValueImpl(
        const QJsonValue& val)
{
    switch(val.type())
    {
        case QJsonValue::Type::Null:
            return no_value_t{};
        case QJsonValue::Type::Bool:
            return val.toBool();
        case QJsonValue::Type::Double:
            return val.toDouble();
        case QJsonValue::Type::String:
            return val.toString();
        case QJsonValue::Type::Array:
        {
            const auto& arr = val.toArray();
            State::tuple_t tuple;
            tuple.reserve(arr.size());

            for(const auto& v : arr)
            {
                tuple.push_back(fromQJsonValueImpl(v));
            }

            return tuple;
        }
        case QJsonValue::Type::Object:
            return no_value_t{};
        case QJsonValue::Type::Undefined:
            return no_value_t{};
        default:
            return no_value_t{};
    }
}

State::Value fromQJsonValue(const QJsonValue& val)
{
    return State::Value::fromValue(fromQJsonValueImpl(val));
}


static State::ValueImpl fromQJsonValueImpl(const QJsonValue& val, State::ValueType type)
{
    if(val.isNull())
    {
        if(type == State::ValueType::Impulse)
            return State::ValueImpl{State::impulse_t{}};
        else
            return State::ValueImpl{State::no_value_t{}};
    }

    switch(type)
    {
        case ValueType::NoValue:
            return State::ValueImpl{State::no_value_t{}};
        case ValueType::Impulse:
            return State::ValueImpl{State::impulse_t{}};
        case ValueType::Int:
            return State::ValueImpl{val.toInt()};
        case ValueType::Float:
            return State::ValueImpl{val.toDouble()};
        case ValueType::Bool:
            return State::ValueImpl{val.toBool()};
        case ValueType::String:
            return State::ValueImpl{val.toString()};
        case ValueType::Char:
        {
            auto str = val.toString();
            if(!str.isEmpty())
                return State::ValueImpl{str[0]};
            return State::ValueImpl{QChar{}};
        }
        case ValueType::Vec2f:
        {
            auto json_arr = val.toArray();
            State::vec2f arr;
            int n = std::min((int)arr.size(), (int)json_arr.size());
            for(int i = 0; i < n; i++)
            {
                arr[i] = json_arr[i].toDouble();
            }

            return State::ValueImpl{arr};
        }
        case ValueType::Vec3f:
        {
            auto json_arr = val.toArray();
            State::vec3f arr;
            int n = std::min((int)arr.size(), (int)json_arr.size());
            for(int i = 0; i < n; i++)
            {
                arr[i] = json_arr[i].toDouble();
            }

            return State::ValueImpl{arr};
        }
        case ValueType::Vec4f:
        {
            auto json_arr = val.toArray();
            State::vec4f arr;
            int n = std::min((int)arr.size(), (int)json_arr.size());
            for(int i = 0; i < n; i++)
            {
                arr[i] = json_arr[i].toDouble();
            }

            return State::ValueImpl{arr};
        }
        case ValueType::Tuple:
        {
            auto arr = val.toArray();
            State::tuple_t tuple;
            tuple.reserve(arr.size());

            auto& strings = iscore::StringConstant();

            Foreach(arr, [&] (const auto& elt)
            {
                auto obj = elt.toObject();
                auto type_it = obj.find(strings.Type);
                auto val_it = obj.find(strings.Value);
                if(val_it != obj.end() && type_it != obj.end())
                {
                    tuple.push_back(fromQJsonValueImpl(*val_it, which((*type_it).toString())));
                }
            });

            return State::ValueImpl{tuple};
        }
        default:
            ISCORE_ABORT;
            throw;
    }
}

static State::Value fromQJsonValue(const QJsonValue& val, ValueType which)
{
    return State::Value{fromQJsonValueImpl(val, which)};
}

State::Value fromQJsonValue(const QJsonValue& val, const QString& type)
{
    return fromQJsonValue(val, which(type));
}



QString prettyType(const State::Value& val)
{
    const auto& impl = val.val.impl();
    ISCORE_ASSERT(impl.which() < ValuePrettyTypes.size());
    return ValuePrettyTypes.at(impl.which());
}


const QStringList& ValuePrettyTypesList()
{
    static bool init = false;
    static QStringList lst;
    if(!init)
    {
        for(const auto& str : ValuePrettyTypes)
            lst.append(str);
        init = true;
    }
    return lst;
}

template<>
int value(const State::Value& val)
{
    struct {
        public:
            using return_type = int;
            return_type operator()(const no_value_t&) const { return 0; }
            return_type operator()(const impulse_t&) const { return 0; }
            return_type operator()(int v) const { return v; }
            return_type operator()(float v) const { return v; }
            return_type operator()(bool v) const { return v; }
            return_type operator()(const QString& v) const { return QLocale::c().toInt(v); }
            return_type operator()(QChar v) const { return QLocale::c().toInt(QString(v)); }
            return_type operator()(const vec2f& v) const { return 0; }
            return_type operator()(const vec3f& v) const { return 0; }
            return_type operator()(const vec4f& v) const { return 0; }
            return_type operator()(const tuple_t& v) const { return 0; }
    } visitor{};

    return eggs::variants::apply(visitor, val.val.impl());
}

template<>
float value(const State::Value& val)
{
    struct {
        public:
            using return_type = float;
            return_type operator()(const no_value_t&) const { return {}; }
            return_type operator()(const impulse_t&) const { return {}; }
            return_type operator()(int v) const { return v; }
            return_type operator()(float v) const { return v; }
            return_type operator()(bool v) const { return v; }
            return_type operator()(const QString& v) const { return QLocale::c().toFloat(v); }
            return_type operator()(QChar v) const { return QLocale::c().toFloat(QString(v)); }
            return_type operator()(const vec2f& v) const { return 0; }
            return_type operator()(const vec3f& v) const { return 0; }
            return_type operator()(const vec4f& v) const { return 0; }
            return_type operator()(const tuple_t& v) const { return {}; }
    } visitor{};

    return eggs::variants::apply(visitor, val.val.impl());
}
template<>
double value(const State::Value& val)
{
    return (double) value<float>(val);
}

template<>
bool value(const State::Value& val)
{
    struct {
        public:
            using return_type = bool;
            return_type operator()(const no_value_t&) const { return {}; }
            return_type operator()(const impulse_t&) const { return {}; }
            return_type operator()(int v) const { return v; }
            return_type operator()(float v) const { return v; }
            return_type operator()(bool v) const { return v; }
            return_type operator()(const QString& v) const {                
                auto& strings = iscore::StringConstant();

                return v == strings.lowercase_true ||
                       v == strings.True ||
                       v == strings.lowercase_yes||
                       v == strings.Yes; }
            return_type operator()(QChar v) const { return v == 't' || v == 'T' || v == 'y' || v == 'Y'; }
            return_type operator()(const vec2f& v) const { return false; }
            return_type operator()(const vec3f& v) const { return false; }
            return_type operator()(const vec4f& v) const { return false; }
            return_type operator()(const tuple_t& v) const { return false; }
    } visitor{};

    return eggs::variants::apply(visitor, val.val.impl());
}

template<>
QChar value(const State::Value& val)
{
    struct {
        public:
            using return_type = QChar;
            return_type operator()(const no_value_t&) const { return '-'; }
            return_type operator()(const impulse_t&) const { return '-'; }
            return_type operator()(int) const { return '-'; }
            return_type operator()(float) const { return '-'; }
            return_type operator()(bool v) const { return v ? 'T' : 'F'; }
            return_type operator()(const QString& s) const { return !s.isEmpty() ? s[0] : '-'; } // TODO boueeeff
                return_type operator()(QChar v) const { return  v; }
            return_type operator()(const vec2f& v) const { return '-'; }
            return_type operator()(const vec3f& v) const { return '-'; }
            return_type operator()(const vec4f& v) const { return '-'; }
            return_type operator()(const tuple_t&) const { return '-'; }
    } visitor{};

    return eggs::variants::apply(visitor, val.val.impl());
}

template<>
QString value(const State::Value& val)
{
    struct {
            using return_type = QString;
            return_type operator()(const State::no_value_t&) const { return {}; }
            return_type operator()(const State::impulse_t&) const { return {}; }
            return_type operator()(int i) const { return QLocale::c().toString(i); }
            return_type operator()(float f) const { return QLocale::c().toString(f); }
            return_type operator()(bool b) const {
                auto& strings = iscore::StringConstant();

                return b
                        ? strings.lowercase_true
                        : strings.lowercase_false;
            }
            return_type operator()(const QString& s) const { return s; }
            return_type operator()(QChar c) const { return c; }
            return_type operator()(const vec2f& v) const { return {}; }
            return_type operator()(const vec3f& v) const { return {}; }
            return_type operator()(const vec4f& v) const { return {}; }
            return_type operator()(const State::tuple_t& t) const { return {}; }
    } visitor{};

    return eggs::variants::apply(visitor, val.val.impl());
}

template<>
vec2f value(const State::Value& val)
{
    struct vis {
            using return_type = vec2f;
            return_type operator()(const State::no_value_t&) const { return {}; }
            return_type operator()(const State::impulse_t&) const { return {}; }
            return_type operator()(int i) const { return { { float(i) } }; }
            return_type operator()(float f) const { return {{f}}; }
            return_type operator()(bool b) const { return {{float(b)}};}
            return_type operator()(const QString& s) const {
                auto v = parseValue(s);

                if(v && v->val.is<tuple_t>())
                    return this->operator ()(v->val.get<State::tuple_t>());

                return {};
            }
            return_type operator()(QChar c) const { return {}; }
            return_type operator()(const vec2f& v) const { return v; }
            return_type operator()(const vec3f& v) const { return {{v[0], v[1]}}; }
            return_type operator()(const vec4f& v) const { return {{v[0], v[1]}}; }
            return_type operator()(const State::tuple_t& t) const {
                const int n = t.size();
                const int n_2 = std::tuple_size<return_type>::value;
                return_type v;
                for(int i = 0; i < std::min(n, n_2); i++)
                {
                    v[i] = value<float>(t[i]);
                }
                return v;
            }
    };

    return eggs::variants::apply(vis{}, val.val.impl());
}

template<>
vec3f value(const State::Value& val)
{
    struct vis {
            using return_type = vec3f;
            return_type operator()(const State::no_value_t&) const { return {}; }
            return_type operator()(const State::impulse_t&) const { return {}; }
            return_type operator()(int i) const { return {{float(i)}}; }
            return_type operator()(float f) const { return {{f}}; }
            return_type operator()(bool b) const { return {{float(b)}};}
            return_type operator()(const QString& s) const {
                auto v = parseValue(s);

                if(v && v->val.is<tuple_t>())
                    return this->operator ()(v->val.get<State::tuple_t>());

                return {};
            }
            return_type operator()(QChar c) const { return {}; }
            return_type operator()(const vec2f& v) const { return {{v[0], v[1]}}; }
            return_type operator()(const vec3f& v) const { return v; }
            return_type operator()(const vec4f& v) const { return {{v[0], v[1], v[2]}}; }
            return_type operator()(const State::tuple_t& t) const {
                const int n = t.size();
                const int n_2 = std::tuple_size<return_type>::value;
                return_type v;
                for(int i = 0; i < std::min(n, n_2); i++)
                {
                    v[i] = value<float>(t[i]);
                }
                return v;
            }
    };

    return eggs::variants::apply(vis{}, val.val.impl());
}

template<>
vec4f value(const State::Value& val)
{
    struct vis {
            using return_type = vec4f;
            return_type operator()(const State::no_value_t&) const { return {}; }
            return_type operator()(const State::impulse_t&) const { return {}; }
            return_type operator()(int i) const { return {{float(i)}}; }
            return_type operator()(float f) const { return {{f}}; }
            return_type operator()(bool b) const { return {{float(b)}};}
            return_type operator()(const QString& s) const {
                auto v = parseValue(s);

                if(v && v->val.is<tuple_t>())
                    return this->operator ()(v->val.get<State::tuple_t>());

                return {};
            }
            return_type operator()(QChar c) const { return {}; }
            return_type operator()(const vec2f& v) const { return {{v[0], v[1]}}; }
            return_type operator()(const vec3f& v) const { return {{v[0], v[1], v[2]}}; }
            return_type operator()(const vec4f& v) const { return v; }
            return_type operator()(const State::tuple_t& t) const {
                const int n = t.size();
                const int n_2 = std::tuple_size<return_type>::value;
                return_type v;
                for(int i = 0; i < std::min(n, n_2); i++)
                {
                    v[i] = value<float>(t[i]);
                }
                return v;
            }
    };

    return eggs::variants::apply(vis{}, val.val.impl());
}


template<>
tuple_t value(const State::Value& val)
{
    struct vis {
            using return_type = tuple_t;
            return_type operator()(const State::no_value_t&) const { return {impulse_t{}}; }
            return_type operator()(const State::impulse_t&) const { return {impulse_t{}}; }
            return_type operator()(int i) const { return {i}; }
            return_type operator()(float f) const { return {f}; }
            return_type operator()(bool b) const { return {b}; }
            return_type operator()(const QString& s) const {
                auto v = parseValue(s);

                if(v && v->val.is<tuple_t>())
                    return v->val.get<State::tuple_t>();

                return {s};
            }
            return_type operator()(QChar c) const { return {c}; }
            return_type operator()(const vec2f& v) const { return {{v[0], v[1]}}; }
            return_type operator()(const vec3f& v) const { return {{v[0], v[1], v[2]}}; }
            return_type operator()(const vec4f& v) const { return {{v[0], v[1], v[2], v[3]}}; }
            return_type operator()(const State::tuple_t& t) const { return t; }
    };

    return eggs::variants::apply(vis{}, val.val.impl());
}


QString toPrettyString(const State::Value& val)
{
    struct vis {
            QString operator()(const State::no_value_t&) const { return {}; }
            QString operator()(const State::impulse_t&) const { return {}; }
            QString operator()(int i) const {
                const auto& loc = QLocale::c();
                auto str = loc.toString(i);
                str.remove(loc.groupSeparator());
                return str;
            }
            QString operator()(float f) const {
                const auto& loc = QLocale::c();
                auto str = loc.toString(f);
                str.remove(loc.groupSeparator());
                return str;
            }
            QString operator()(bool b) const {
                return b
                        ? iscore::StringConstant().lowercase_true
                        : iscore::StringConstant().lowercase_false;
            }
            QString operator()(const QString& s) const
            {
                // TODO escape ?
                return QString("\"%1\"").arg(s);
            }

            QString operator()(QChar c) const
            {
                return QString("'%1'").arg(c);
            }

            QString operator()(const vec2f& t) const
            {
                QString s{"["};

                s += this->operator()(t[0]);

                for(std::size_t i = 1; i < t.size(); i++)
                {
                    s += ", ";
                    s += this->operator()(t[i]);
                }

                s+= "]";
                return s;
            }

            QString operator()(const vec3f& t) const
            {
                QString s{"["};

                s += this->operator()(t[0]);

                for(std::size_t i = 1; i < t.size(); i++)
                {
                    s += ", ";
                    s += this->operator()(t[i]);
                }

                s+= "]";
                return s;
            }

            QString operator()(const vec4f& t) const
            {
                QString s{"["};

                s += this->operator()(t[0]);

                for(std::size_t i = 1; i < t.size(); i++)
                {
                    s += ", ";
                    s += this->operator()(t[i]);
                }

                s+= "]";
                return s;
            }

            QString operator()(const State::tuple_t& t) const
            {
                QString s{"["};

                auto n = t.size();
                if(n >= 1)
                {
                    s += eggs::variants::apply(*this, t[0].impl());
                }

                for(std::size_t i = 1; i < n; i++)
                {
                    s += ", ";
                    s += eggs::variants::apply(*this, t[i].impl());
                }

                s+= "]";
                return s;
            }
    };

    return eggs::variants::apply(vis{}, val.val.impl());
}


bool convert(const State::Value& orig, State::Value& toConvert)
{
    switch(orig.val.which())
    {
        case ValueType::NoValue:
            toConvert.val = no_value_t{};
            break;
        case ValueType::Impulse:
            toConvert.val = impulse_t{};
            break;
        case ValueType::Int:
            toConvert.val = value<int>(toConvert);
            break;
        case ValueType::Float:
            toConvert.val = value<float>(toConvert);
            break;
        case ValueType::Bool:
            toConvert.val = value<bool>(toConvert);
            break;
        case ValueType::String:
            toConvert.val = value<QString>(toConvert);
            break;
        case ValueType::Char:
            toConvert.val = value<QChar>(toConvert);
            break;
        case ValueType::Vec2f:
            toConvert.val = value<vec2f>(toConvert);
            break;
        case ValueType::Vec3f:
            toConvert.val = value<vec3f>(toConvert);
            break;
        case ValueType::Vec4f:
            toConvert.val = value<vec4f>(toConvert);
            break;
        case ValueType::Tuple:
            toConvert.val = value<tuple_t>(toConvert);
            break;
        default:
            break;
    }

    return true;
}


static State::ValueImpl fromQVariantImpl(const QVariant& val)
{
#pragma GCC diagnostic ignored "-Wswitch"
#pragma GCC diagnostic ignored "-Wswitch-enum"
    switch(auto t = QMetaType::Type(val.type()))
    {
        case QMetaType::Int:
            return State::ValueImpl{val.toInt()};
        case QMetaType::UInt:
            return State::ValueImpl{(int)val.toUInt()};
        case QMetaType::Long:
            return State::ValueImpl{(int)val.value<int64_t>()};
        case QMetaType::LongLong:
            return State::ValueImpl{(int)val.toLongLong()};
        case QMetaType::ULong:
            return State::ValueImpl{(int)val.value<uint64_t>()};
        case QMetaType::ULongLong:
            return State::ValueImpl{(int)val.toULongLong()};
        case QMetaType::Short:
            return State::ValueImpl{(int)val.value<int16_t>()};
        case QMetaType::UShort:
            return State::ValueImpl{(int)val.value<uint16_t>()};
        case QMetaType::Float:
            return State::ValueImpl{val.toFloat()};
        case QMetaType::Double:
            return State::ValueImpl{(float)val.toDouble()};
        case QMetaType::Bool:
            return State::ValueImpl{val.toBool()};
        case QMetaType::QString:
            return State::ValueImpl{val.toString()};
        case QMetaType::Char:
            return State::ValueImpl{(QChar)val.value<char>()};
        case QMetaType::QChar:
            return State::ValueImpl{val.toChar()};
        case QMetaType::QVariantList:
        {
            auto list = val.value<QVariantList>();
            tuple_t tuple_val;
            tuple_val.reserve(list.size());

            Foreach(list, [&] (const auto& elt)
            {
                tuple_val.push_back(fromQVariantImpl(elt));
            });
            return tuple_val;
        }
        case QMetaType::QVector2D:
        {
            auto vec = val.value<QVector2D>();
            return State::ValueImpl{vec2f{{vec[0], vec[1]}}};
        }
        case QMetaType::QVector3D:
        {
            auto vec = val.value<QVector3D>();
            return State::ValueImpl{vec3f{{vec[0], vec[1], vec[2]}}};
        }
        case QMetaType::QVector4D:
        {
            auto vec = val.value<QVector4D>();
            return State::ValueImpl{vec4f{{vec[0], vec[1], vec[2], vec[3]}}};
        }
        default:
        {
            if(t == qMetaTypeId<State::Value>())
            {
                return State::ValueImpl{impulse_t{}};
            }
            else
            {
                return State::ValueImpl{no_value_t{}};
            }
        }
    }

#pragma GCC diagnostic warning "-Wswitch"
#pragma GCC diagnostic warning "-Wswitch-enum"
}


State::Value fromQVariant(const QVariant& val)
{
    return State::Value{fromQVariantImpl(val)};
}

QString prettyType(ValueType t)
{
    return ValuePrettyTypes[static_cast<int>(t)];
}

const std::array<std::pair<QString, State::ValueType>, 10> & ValuePrettyTypesMap()
{
    return ValuePrettyTypesPairArray;
}

const std::array<const QString, 11>& ValuePrettyTypesArray()
{
    return ValuePrettyTypes;
}

}
}
