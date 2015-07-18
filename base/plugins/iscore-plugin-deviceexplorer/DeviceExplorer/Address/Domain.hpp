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
}
