#pragma once
#include <QtCore>
#include <boost/mpl/list.hpp>
#include <boost/mpl/for_each.hpp>
#include <eggs/variant.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template<typename... Args>
class VariantBasedNode
{
    public:
        VariantBasedNode(const VariantBasedNode& t) = default;
        VariantBasedNode(VariantBasedNode&& t) = default;
        VariantBasedNode& operator=(const VariantBasedNode& t) = default;
        template<typename T>
        VariantBasedNode(const T& t):
            m_data{t}
        {

        }

        template<typename T>
        bool is() const { return m_data.template target<T>() != nullptr; }

        template<typename T>
        void set(const T& t) { m_data = t; }

        template<typename T>
        const T& get() const { return *m_data.template target<T>(); }

        template<typename T>
        T& get() { return *m_data.template target<T>(); }

        auto which() const
        { return m_data.which(); }

    protected:
        eggs::variant<Args...> m_data;
};

template<typename... Args>
void Visitor<Reader<DataStream>>::readFrom(const eggs::variant<Args...>& var)
{
    m_stream << (quint64)var.which();

    if((quint64)var.which() != (quint64)var.npos)
    {
        // This trickery iterates over all the types in Args...
        // A single type should be serialized, even if we cannot break.
        bool done = false;
        typedef boost::mpl::list<Args...> typelist;
        boost::mpl::for_each<typelist>([&] (auto&& elt) {
            if(done)
                return;

            if(auto res = var.template target<typename std::remove_reference<decltype(elt)>::type>())
            {
                this->readFrom(*res);
                done = true;
            }
        });
    }

    insertDelimiter();
}


template<typename... Args>
void Visitor<Writer<DataStream>>::writeTo(eggs::variant<Args...>& var)
{
    quint64 which;
    m_stream >> which;

    if(which != (quint64)var.npos)
    {
        // Here we iterate until we are on the correct type, and we deserialize it.
        quint64 i = 0;
        typedef boost::mpl::list<Args...> typelist;
        boost::mpl::for_each<typelist>([&] (auto&& elt) {
            if(i++ != which)
                return;

            typename std::remove_reference<decltype(elt)>::type data;
            this->writeTo(data);
            var = data;
        });
    }
    checkDelimiter();
}


template<typename T>
class TypeToName;
template<typename... Args>
void Visitor<Reader<JSONObject>>::readFrom(const eggs::variant<Args...>& var)
{
    if((quint64)var.which() != (quint64)var.npos)
    {
        bool done = false;
        typedef boost::mpl::list<Args...> typelist;
        boost::mpl::for_each<typelist>([&] (auto&& elt) {
            if(done)
                return;

            using current_type = typename std::remove_reference<decltype(elt)>::type;

            if(auto res = var.template target<current_type>())
            {
                this->m_obj[TypeToName<current_type>::name()] = toJsonObject(*res);
                done = true;
            }
        });
    }
}

template<typename... Args>
void Visitor<Writer<JSONObject>>::writeTo(eggs::variant<Args...>& var)
{
    bool done = false;
    typedef boost::mpl::list<Args...> typelist;
    boost::mpl::for_each<typelist>([&] (auto&& elt) {
        if(done)
            return;
        using current_type = typename std::remove_reference<decltype(elt)>::type;

        if(m_obj.contains(TypeToName<current_type>::name()))
        {
            typename std::remove_reference<decltype(elt)>::type data;
            fromJsonObject(m_obj[TypeToName<current_type>::name()].toObject(), data);
            var = data;
            done = true;
        }
    });
}
