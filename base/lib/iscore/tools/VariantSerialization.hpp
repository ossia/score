#pragma once
#include <eggs/variant.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <boost/mpl/list.hpp>
#include <boost/mpl/for_each.hpp>
#include <QDebug>

template<typename... Args>
struct TSerializer<DataStream, eggs::variant<Args...>>
{
        static void readFrom(
                DataStream::Serializer& s,
                const eggs::variant<Args...>& var)
        {
            s.stream() << (quint64)var.which();

            // TODO this should be an assert.
            if((quint64)var.which() != (quint64)var.npos)
            {
                // This trickery iterates over all the types in Args...
                // A single type should be serialized, even if we cannot break.
                bool done = false;
                using typelist = boost::mpl::list<Args...>;
                boost::mpl::for_each<typelist>([&] (auto&& elt) {
                    if(done)
                        return;

                    if(auto res = var.template target<typename std::remove_reference<decltype(elt)>::type>())
                    {
                        s.stream() << *res;
                        done = true;
                    }
                });
            }

            s.insertDelimiter();
        }

        static void writeTo(
                DataStream::Deserializer& s,
                eggs::variant<Args...>& var)
        {
            quint64 which;
            s.stream() >> which;

            if(which != (quint64)var.npos)
            {
                // Here we iterate until we are on the correct type, and we deserialize it.
                quint64 i = 0;
                using typelist = boost::mpl::list<Args...>;
                boost::mpl::for_each<typelist>([&] (auto&& elt) {
                    if(i++ != which)
                        return;

                    typename std::remove_reference<decltype(elt)>::type data;
                    s.stream() >> data;
                    var = data;
                });
            }
            s.checkDelimiter();
        }
};


template<typename T>
class TypeToName;

// This part is required because it isn't as straightforward to save variant data
// in JSON as it is to save it in a DataStream.
// Basically, each variant member has an associated name that will be the
// key in the JSON parent object. This name is defined by specializing
// template<> class TypeToName<T>.
// For instance:
// template<> class TypeToName<iscore::Address>
// { public: static constexpr const char * name() { return "Address"; } };

// This allows easy store and retrieval under a familiar name


// TODO add some ASSERT for the variant being set on debug mode. npos case should not happen since we have the OptionalVariant.
// TODO qstring and Qt-serializable objects ???
// The _eggs_impl functions are because enum's don't need full-fledged objects in json.
template<typename T, std::enable_if_t<!is_value_t<T>::value>* = nullptr>
QJsonValue readFrom_eggs_impl(const T& res)
{
    return toJsonObject(res);
}
template<typename T, std::enable_if_t<is_value_t<T>::value>* = nullptr>
QJsonValue readFrom_eggs_impl(const T& res)
{
    return toJsonValue(res);
}

template<typename... Args>
void Visitor<Reader<JSONObject>>::readFrom(const eggs::variant<Args...>& var)
{
    if((quint64)var.which() != (quint64)var.npos)
    {
        bool done = false;
        using typelist = boost::mpl::list<Args...>;
        boost::mpl::for_each<typelist>([&] (auto&& elt) {
            if(done)
                return;

            using current_type = typename std::remove_reference<decltype(elt)>::type;

            if(auto res = var.template target<current_type>())
            {
                this->m_obj[TypeToName<current_type>::name()] = readFrom_eggs_impl(*res);
                done = true;
            }
        });
    }
}

/**
 * These two methods are because enum's don't need full-fledged objects.
 */
template<typename T, std::enable_if_t<!is_value_t<T>::value>* = nullptr>
auto writeTo_eggs_impl(const QJsonValue& res)
{
    return fromJsonObject<T>(res.toObject());
}
template<typename T, std::enable_if_t<is_value_t<T>::value>* = nullptr>
auto writeTo_eggs_impl(const QJsonValue& res)
{
    return fromJsonValue<T>(res);
}

template<typename... Args>
void Visitor<Writer<JSONObject>>::writeTo(eggs::variant<Args...>& var)
{
    bool done = false;
    using typelist = boost::mpl::list<Args...>;
    boost::mpl::for_each<typelist>([&] (auto&& elt) {
        if(done)
            return;
        using current_type = typename std::remove_reference<decltype(elt)>::type;

        if(m_obj.contains(TypeToName<current_type>::name()))
        {
            current_type data = writeTo_eggs_impl<current_type>(m_obj[TypeToName<current_type>::name()]);

            var = data;
            done = true;
        }
    });
}
