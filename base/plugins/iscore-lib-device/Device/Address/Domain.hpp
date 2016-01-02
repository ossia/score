#pragma once
#include <State/Value.hpp>

namespace Device
{
struct Domain
{
        State::Value min;
        State::Value max;
        State::ValueList values;
};

inline bool operator==(
        const Device::Domain& lhs,
        const Device::Domain& rhs)
{
    return lhs.min == rhs.min && lhs.max == rhs.max && lhs.values == rhs.values;
}
}
