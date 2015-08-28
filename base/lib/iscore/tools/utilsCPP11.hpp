#pragma once
#include <algorithm>

/**
 * TODO review use of these functions.
 * Most cases should be converted to IdContainer.
 */
template <typename Vector, typename Functor>
void vec_erase_remove_if(Vector& v, Functor&& f)
{
    v.erase(std::remove_if(std::begin(v), std::end(v), f), std::end(v));
}

template<typename Container>
using return_type_of_iterator =
    typename std::remove_reference<
        decltype(*std::declval<Container>().begin())
    >::type;



template<typename Range, typename Fun>
void erase_if(Range&& r, Fun&& f)
{
    for(auto&& it = begin(r); it != end(r); )
    {
        it = f(*it) ? r.erase(it) : ++it;
    }
}
