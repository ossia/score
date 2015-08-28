#pragma once
#include <iscore/tools/SettableIdentifier.hpp>
#include <boost/range/algorithm.hpp>

/**
 * @brief getNextId
 * @return a random int32
 */
int32_t getNextId();

/**
 * @brief getNextId
 * @param ids A vector of ids
 *
 * @return A new id not in
template<typename Vector>the vector.
 */
int getNextId(const Vector& ids)
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

/**
 * The following functions all generate ids
 * with type safety using different containers.
 */

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
