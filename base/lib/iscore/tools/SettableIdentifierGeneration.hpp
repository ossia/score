#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <random>
#include <boost/range/algorithm.hpp>

inline int32_t getNextId()
{
    using namespace std;
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_int_distribution<int32_t>
    dist(numeric_limits<int32_t>::min(),
         numeric_limits<int32_t>::max());

    return dist(gen);
}

template<typename Vector>
inline int getNextId(const Vector& ids)
{
    using namespace boost::range;
    int id {};

    do
    {
        id = getNextId();
    }
    while(find(ids, id) != std::end(ids));

    return id;
}


// TODO refactor
template<typename T>
auto getStrongId(const std::vector<id_type<T>>& v)
{
    using namespace boost::range;
    id_type<T> id {};

    do
    {
        id = id_type<T>{getNextId()};
    }
    while(find(v, id) != std::end(v));

    return id;
}

template<typename T>
auto getStrongId(const QVector<id_type<T>>& v)
{
    using namespace boost::range;
    id_type<T> id {};

    do
    {
        id = id_type<T>{getNextId()};
    }
    while(find(v, id) != std::end(v));

    return id;
}

template<typename Container,
         typename std::enable_if_t<
                    std::is_pointer<
                      typename Container::value_type
                    >::value
                  >* = nullptr>
auto getStrongId(const Container& v)
    -> id_type<typename std::remove_pointer<typename Container::value_type>::type>
{
    using namespace std;
    using local_id_t = id_type<typename std::remove_pointer<typename Container::value_type>::type>;
    vector<int> ids(v.size());   // Map reduce

    transform(v.begin(),
              v.end(),
              ids.begin(),
              [](const typename Container::value_type& elt)
    {
        return * (elt->id().val());
    });

    return local_id_t{getNextId(ids)};
}

template<typename Container,
         typename std::enable_if_t<
                    not std::is_pointer<
                      typename Container::value_type
                    >::value
                  >* = nullptr>
auto getStrongId(const Container& v) ->
    id_type<typename Container::value_type>
{
    using namespace std;
    vector<int> ids(v.size());   // Map reduce

    transform(v.begin(),
              v.end(),
              ids.begin(),
              [](const typename Container::value_type& elt)
    {
        return * (elt.id().val());
    });

    return id_type<typename Container::value_type>{getNextId(ids)};
}
