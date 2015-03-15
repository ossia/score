#pragma once
#include <cstdint>
using StateId = int32_t;


template<typename T>
StateId stateId(QList<T>&) {
    return 1000 + stateId(T{});
}
