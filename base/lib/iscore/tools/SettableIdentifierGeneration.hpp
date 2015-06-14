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

template<typename Vector,
         typename std::enable_if<
                    std::is_pointer<
                      typename Vector::value_type
                    >::value
                  >::type* = nullptr>
auto getStrongId(const Vector& v)
    -> id_type<typename std::remove_pointer<typename Vector::value_type>::type>
{
    using namespace std;
    vector<int> ids(v.size());   // Map reduce

    transform(begin(v),
              end(v),
              begin(ids),
              [](const typename Vector::value_type& elt)
    {
        return * (elt->id().val());
    });

    return id_type<typename std::remove_pointer<typename Vector::value_type>::type> {getNextId(ids)};
}

template<typename Vector,
         typename std::enable_if<
                    not std::is_pointer<
                      typename Vector::value_type
                    >::value
                  >::type* = nullptr>
auto getStrongId(const Vector& ids) ->
    typename Vector::value_type
{
    using namespace boost::range;
    typename Vector::value_type id {};

    do
    {
        id = typename Vector::value_type{getNextId()};
    }
    while(find(ids, id) != std::end(ids));

    return id;
}
