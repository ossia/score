#include "State.hpp"

uint qHash(const QVariant &state, uint seed)
{
    return 0;
}

uint qHash(const iscore::State &state, uint seed) noexcept
{
    return qHash(state.data(), seed);
}


uint qHash(const iscore::State &state) noexcept
{
    return qHash(state, 0xABCDEF);
}
