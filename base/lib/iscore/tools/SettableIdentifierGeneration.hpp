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

template<typename Container>
auto getStrongIdFromIdContainer(const Container& v)
{
    using namespace boost::range;
    typename Container::value_type id;

    do
    {
        id = typename Container::value_type{getNextId()};
    }
    while(find(v, id) != std::end(v));

    return id;
}

template<typename T>
auto getStrongId(const std::vector<Id<T>>& v)
{
    return getStrongIdFromIdContainer(v);
}

template<typename T>
auto getStrongId(const QVector<Id<T>>& v)
{
    return getStrongIdFromIdContainer(v);
}

template<typename Container,
         std::enable_if_t<
                    std::is_pointer<
                      typename Container::value_type
                    >::value
                  >* = nullptr>
auto getStrongId(const Container& v)
    -> Id<typename std::remove_pointer<typename Container::value_type>::type>
{
    using namespace std;
    using local_id_t = Id<typename std::remove_pointer<typename Container::value_type>::type>;
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
         std::enable_if_t<
                    ! std::is_pointer<
                      typename Container::value_type
                    >::value
                  >* = nullptr>
auto getStrongId(const Container& v) ->
    Id<typename Container::value_type>
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

    return Id<typename Container::value_type>{getNextId(ids)};
}
