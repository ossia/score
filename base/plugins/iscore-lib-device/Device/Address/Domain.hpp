#pragma once
#include <State/Value.hpp>

namespace iscore
{
struct Domain
{
        Value min;
        Value max;
        ValueList values;
};

inline bool operator==(
        const iscore::Domain& lhs,
        const iscore::Domain& rhs)
{
    return lhs.min == rhs.min && lhs.max == rhs.max && lhs.values == rhs.values;
}
}
