#include "ValueConversion.hpp"
#include <QJsonArray>
#include <array>
namespace iscore
{
namespace convert
{

static const std::array<const QString, 8> ValueTypesArray{{
    QString{"Impulse"},
    QString{"Int"},
    QString{"Float"},
    QString{"Bool"},
    QString{"String"},
    QString{"Char"},
    QString{"Tuple"},
    QString{"NoValue"}
}};

static const std::array<const QString, 8> ValuePrettyTypesArray{{
    QObject::tr("Impulse"),
    QObject::tr("Int"),
    QObject::tr("Float"),
    QObject::tr("Bool"),
    QObject::tr("String"),
    QObject::tr("Char"),
    QObject::tr("Tuple"),
    QObject::tr("NoValue")
}};

template<>
QVariant value(const iscore::Value& val)
{
    static const constexpr struct {
        public:
            using return_type = QVariant;
            return_type operator()(const no_value_t&) const { return QVariant{}; }
            return_type operator()(const impulse_t&) const { return QVariant{QVariant::UserType + 1}; }
            return_type operator()(int i) const { return QVariant::fromValue(i); }
            return_type operator()(float f) const { return QVariant::fromValue(f); }
            return_type operator()(bool b) const { return QVariant::fromValue(b); }
            return_type operator()(const QString& s) const { return QVariant::fromValue(s); }
            return_type operator()(const QChar& c) const { return QVariant::fromValue(c); }

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
    } visitor{};

    return eggs::variants::apply(visitor, val.val.impl());
}

template<>
QJsonValue value(const iscore::Value& val)
{
    static const constexpr struct {
        public:
            using return_type = QJsonValue;
            return_type operator()(const no_value_t&) const { return {}; }
            return_type operator()(const impulse_t&) const { return {}; }
            return_type operator()(int i) const { return i; }
            return_type operator()(float f) const { return f; }
            return_type operator()(bool b) const { return b; }
            return_type operator()(const QString& s) const { return s; }

            return_type operator()(const QChar& c) const
            {
                // Note : it is saved as a string but the actual type should be saved also
                // so that the QChar can be recovered.
                return QString(c);
            }

            return_type operator()(const tuple_t& t) const
            {
                QJsonArray arr;

                for(const auto& elt : t)
                {
                    arr.append(eggs::variants::apply(*this, elt.impl()));
                }

                return arr;
            }
    } visitor{};

    return eggs::variants::apply(visitor, val.val.impl());
}


QString textualType(const iscore::Value& val)
{
    const auto& impl = val.val.impl();
    ISCORE_ASSERT(impl.which() >= 0 && impl.which() < ValueTypesArray.size());
    return ValueTypesArray[impl.which()];
}

static ValueType which(const QString& val)
{
    auto it = std::find(ValueTypesArray.begin(), ValueTypesArray.end(), val);
    ISCORE_ASSERT(it != ValueTypesArray.end()); // What happens if there is a corrupt save file ?
    return static_cast<iscore::ValueType>(std::distance(ValueTypesArray.begin(), it));
}


static iscore::ValueImpl toValueImpl(const QJsonValue& val, iscore::ValueType which)
{
    switch(which)
    {
        case ValueType::NoValue:
            return iscore::ValueImpl{iscore::no_value_t{}};
        case ValueType::Impulse:
            return iscore::ValueImpl{iscore::impulse_t{}};
        case ValueType::Int:
            return iscore::ValueImpl{val.toInt()};
        case ValueType::Float:
            return iscore::ValueImpl{val.toDouble()};
        case ValueType::Bool:
            return iscore::ValueImpl{val.toBool()};
        case ValueType::String:
            return iscore::ValueImpl{val.toString()};
        case ValueType::Char:
        {
            auto str = val.toString();
            if(str.size() > 0)
                return iscore::ValueImpl{val.toString()[0]};
            return iscore::ValueImpl{QChar{}};
        }
        case ValueType::Tuple:
        {
            auto arr = val.toArray();
            iscore::tuple_t tuple;
            tuple.reserve(arr.size());

            for(const auto& elt : arr)
            {
                tuple.push_back(toValueImpl(elt, which));
            }

            return iscore::ValueImpl{tuple};
        }
        default:
            ISCORE_ABORT;
            throw;
    }
}

static iscore::Value toValue(const QJsonValue& val, ValueType which)
{
    return iscore::Value{toValueImpl(val, which)};
}

iscore::Value toValue(const QJsonValue& val, const QString& type)
{
    return toValue(val, which(type));
}



QString prettyType(const iscore::Value& val)
{
    const auto& impl = val.val.impl();
    ISCORE_ASSERT(impl.which() >= 0 && impl.which() < ValuePrettyTypesArray.size());
    return ValuePrettyTypesArray[impl.which()];
}


const QStringList& ValuePrettyTypesList()
{
    static bool init = false;
    static QStringList lst;
    if(!init)
    {
        for(const auto& str : ValuePrettyTypesArray)
            lst.append(str);
        init = true;
    }
    return lst;
}

template<>
int value(const iscore::Value& val)
{
    static const constexpr struct {
        public:
            using return_type = int;
            return_type operator()(const no_value_t&) const { return 0; }
            return_type operator()(const impulse_t&) const { return 0; }
            return_type operator()(int v) const { return v; }
            return_type operator()(float v) const { return v; }
            return_type operator()(bool v) const { return v; }
            return_type operator()(const QString& v) const { return QLocale::c().toInt(v); }
            return_type operator()(const QChar& v) const { return QLocale::c().toInt(QString(v)); }
            return_type operator()(const tuple_t& v) const { return 0; }
    } visitor{};

    return eggs::variants::apply(visitor, val.val.impl());
}

template<>
float value(const iscore::Value& val)
{
    static const constexpr struct {
        public:
            using return_type = float;
            return_type operator()(const no_value_t&) const { return {}; }
            return_type operator()(const impulse_t&) const { return {}; }
            return_type operator()(int v) const { return v; }
            return_type operator()(float v) const { return v; }
            return_type operator()(bool v) const { return v; }
            return_type operator()(const QString& v) const { return QLocale::c().toFloat(v); }
            return_type operator()(const QChar& v) const { return QLocale::c().toFloat(QString(v)); }
            return_type operator()(const tuple_t& v) const { return {}; }
    } visitor{};

    return eggs::variants::apply(visitor, val.val.impl());
}
template<>
double value(const iscore::Value& val)
{
    return (double) value<float>(val);
}

template<>
bool value(const iscore::Value& val)
{
    static const constexpr struct {
        public:
            using return_type = bool;
            return_type operator()(const no_value_t&) const { return {}; }
            return_type operator()(const impulse_t&) const { return {}; }
            return_type operator()(int v) const { return v; }
            return_type operator()(float v) const { return v; }
            return_type operator()(bool v) const { return v; }
            return_type operator()(const QString& v) const { return v == "true" || v == "True"; } // TODO boueeeff
            return_type operator()(const QChar& v) const { return v.toLatin1(); }
            return_type operator()(const tuple_t& v) const { return false; }
    } visitor{};

    return eggs::variants::apply(visitor, val.val.impl());
}

template<>
QChar value(const iscore::Value& val)
{
    static const constexpr struct {
        public:
            using return_type = QChar;
            return_type operator()(const no_value_t&) const { return '-'; }
            return_type operator()(const impulse_t&) const { return '-'; }
            return_type operator()(int) const { return '-'; }
            return_type operator()(float) const { return '-'; }
            return_type operator()(bool v) const { return v ? 'T' : 'F'; }
            return_type operator()(const QString&) const { return '-'; } // TODO boueeeff
            return_type operator()(const QChar& v) const { return  v; }
            return_type operator()(const tuple_t&) const { return '-'; }
    } visitor{};

    return eggs::variants::apply(visitor, val.val.impl());
}

template<>
QString value(const iscore::Value& val)
{
    static const constexpr struct {
            using return_type = QString;
            return_type operator()(const iscore::no_value_t&) const { return {}; }
            return_type operator()(const iscore::impulse_t&) const { return {}; }
            return_type operator()(int i) const { return QLocale::c().toString(i); }
            return_type operator()(float f) const { return QLocale::c().toString(f); }
            return_type operator()(bool b) const {
                static const QString tr = "true";
                static const QString f = "false";
                return b ? tr : f;
            }
            return_type operator()(const QString& s) const { return s; }
            return_type operator()(const QChar& c) const { return c; }
            return_type operator()(const iscore::tuple_t& t) const { return ""; }
    } visitor{};

    return eggs::variants::apply(visitor, val.val.impl());
}

template<>
tuple_t value(const iscore::Value& val)
{
    static const constexpr struct {
            using return_type = tuple_t;
            return_type operator()(const iscore::no_value_t&) const { return {impulse_t{}}; }
            return_type operator()(const iscore::impulse_t&) const { return {impulse_t{}}; }
            return_type operator()(int i) const { return {i}; }
            return_type operator()(float f) const { return {f}; }
            return_type operator()(bool b) const {
                return {b};
            }
            return_type operator()(const QString& s) const { return {s}; }
            return_type operator()(const QChar& c) const { return {c}; }
            return_type operator()(const iscore::tuple_t& t) const { return t; }
    } visitor{};

    return eggs::variants::apply(visitor, val.val.impl());
}


QString toPrettyString(const iscore::Value& val)
{
    static const constexpr struct {
            QString operator()(const iscore::no_value_t&) const { return {}; }
            QString operator()(const iscore::impulse_t&) const { return {}; }
            QString operator()(int i) const { return QLocale::c().toString(i); }
            QString operator()(float f) const { return QLocale::c().toString(f); }
            QString operator()(bool b) const {
                static const QString tr = "true";
                static const QString f = "false";
                return b ? tr : f;
            }
            QString operator()(const QString& s) const
            {
                // TODO escape ?
                return QString("\"%1\"").arg(s);
            }

            QString operator()(const QChar& c) const
            {
                return QString("'%1'").arg(c);
            }

            QString operator()(const iscore::tuple_t& t) const
            {
                QString s{"["};

                for(const auto& elt : t)
                {
                    s += eggs::variants::apply(*this, elt.impl());
                    s += ", ";
                }

                s+= "]";
                return s;
            }
    } visitor{};

    return eggs::variants::apply(visitor, val.val.impl());
}


bool convert(const iscore::Value& orig, iscore::Value& toConvert)
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
        case ValueType::Tuple:
            toConvert.val = value<tuple_t>(toConvert);
            break;
        default:
            break;
    }

    return true;
}


iscore::ValueImpl toValueImpl(const QVariant& val)
{
    switch(QMetaType::Type(val.type()))
    {
        case QMetaType::Int:
            return iscore::ValueImpl{val.value<int>()};
        case QMetaType::UInt:
            return iscore::ValueImpl{(int)val.value<unsigned int>()};
        case QMetaType::Long:
            return iscore::ValueImpl{(int)val.value<long>()};
        case QMetaType::LongLong:
            return iscore::ValueImpl{(int)val.value<long long>()};
        case QMetaType::ULong:
            return iscore::ValueImpl{(int)val.value<unsigned long>()};
        case QMetaType::ULongLong:
            return iscore::ValueImpl{(int)val.value<unsigned long long>()};
        case QMetaType::Short:
            return iscore::ValueImpl{(int)val.value<short>()};
        case QMetaType::UShort:
            return iscore::ValueImpl{(int)val.value<unsigned short>()};
        case QMetaType::Float:
            return iscore::ValueImpl{val.value<float>()};
        case QMetaType::Double:
            return iscore::ValueImpl{(float)val.value<double>()};
        case QMetaType::Bool:
            return iscore::ValueImpl{val.value<bool>()};
        case QMetaType::QString:
            return iscore::ValueImpl{val.value<QString>()};
        case QMetaType::Char:
            return iscore::ValueImpl{(QChar)val.value<char>()};
        case QMetaType::QChar:
            return iscore::ValueImpl{val.value<char>()};
        case QMetaType::User + 1:
            return iscore::ValueImpl{impulse_t{}};
        case QMetaType::QVariantList:
        {
            auto list = val.value<QVariantList>();
            tuple_t val;
            val.reserve(list.size());
            for(const auto& elt : list)
            {
                val.push_back(toValueImpl(elt));
            }
        }
        default:
            return iscore::ValueImpl{no_value_t{}};
    }
}


iscore::Value toValue(const QVariant& val)
{
    return iscore::Value{toValueImpl(val)};
}

}
}
