#include "State.hpp"
#include "Message.hpp"
#include "iscore_compiler_detection.hpp"
#include <iscore/tools/ObjectPath.hpp>
#include <algorithm>
#include <type_traits>
#include <QVariant>
#include <QHash>
namespace {

    template<typename T,
             std::enable_if_t<std::is_arithmetic<T>::value>* = nullptr>
    uint qHash(T val) {  return val; }
// Found on stackoverflow : http://stackoverflow.com/questions/17208813/qvariant-as-key-in-qhash
uint qHash( const QVariant & var ) noexcept
{
    Q_ASSERT(var.isValid() && !var.isNull());

    switch ( var.type() )
    {
        case QVariant::Int:
                return qHash( var.toInt() );
        case QVariant::UInt:
                return qHash( var.toUInt() );
        case QVariant::Bool:
                return qHash( var.toUInt() );
        case QVariant::Double:
                return qHash( var.toUInt() );
        case QVariant::LongLong:
                return qHash( var.toLongLong() );
        case QVariant::ULongLong:
                return qHash( var.toULongLong() );
        case QVariant::String:
                return qHash( var.toString() );
        case QVariant::Char:
                return qHash( var.toChar() );
        case QVariant::StringList:
                return qHash( var.toString() );
        case QVariant::ByteArray:
                return qHash( var.toByteArray() );
        case QVariant::Date:
        case QVariant::Time:
        case QVariant::DateTime:
        case QVariant::Url:
        case QVariant::Locale:
        case QVariant::RegExp:
                return qHash( var.toString() );
        case QVariant::Map:
        case QVariant::List:
        case QVariant::BitArray:
        case QVariant::Size:
        case QVariant::SizeF:
        case QVariant::Rect:
        case QVariant::LineF:
        case QVariant::Line:
        case QVariant::RectF:
        case QVariant::Point:
        case QVariant::PointF:
            // not supported yet
            break;
        case QVariant::UserType:
        case QVariant::Invalid:
        default:
            break;
    }

    return -1;
}

std::size_t ArrayHash(const QStringList& arr) noexcept
{
    uint hash = 0;
    for(auto & elt : arr)
        hash = 31 * hash + ::qHash(elt);
    return hash;
}

std::size_t hash_value(const iscore::Address &addr) noexcept
{
    return qHash(addr.device) ^ ArrayHash(addr.path);
}

std::size_t hash_message(const iscore::Message &mess) noexcept
{
    return hash_value(mess.address) ^ qHash(mess.value.val);
}

std::size_t ArrayHash(const iscore::MessageList& arr) noexcept
{
    uint hash = 0;
    for(auto & elt : arr)
        hash = 31 * hash + ::hash_message(elt);
    return hash;
}

std::size_t ArrayHash(const iscore::StateList& arr) noexcept
{
    uint hash = 0;
    for(auto & elt : arr)
        hash = 31 * hash + ::hash_value(elt);
    return hash;
}
}

std::size_t hash_value(const iscore::State& state) noexcept
{
    if(state.data().canConvert<iscore::State>())
    {
        return hash_value(state.data().value<iscore::State>());
    }
    else if(state.data().canConvert<iscore::StateList>())
    {
        return ArrayHash(state.data().value<iscore::StateList>());
    }
    else if(state.data().canConvert<iscore::Message>())
    {
        return hash_message(state.data().value<iscore::Message>());
    }
    else if(state.data().canConvert<iscore::MessageList>())
    {
        return ArrayHash(state.data().value<iscore::MessageList>());
    }
    else
    {
        qDebug() << "Missing type" << state.data().typeName();
        ISCORE_TODO;
        return -1;
    }
}
