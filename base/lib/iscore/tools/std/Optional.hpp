#pragma once
#include <boost/optional.hpp>
#include <boost/none.hpp>

template<typename Optional_T>
void reset(Optional_T& val)
{
    val = boost::none;
}
