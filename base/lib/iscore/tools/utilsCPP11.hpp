#pragma once
#include <algorithm>
template <typename Vector, typename Functor>
void vec_erase_remove_if(Vector& v, Functor&& f)
{
    v.erase(std::remove_if(std::begin(v), std::end(v), f), std::end(v));
}
