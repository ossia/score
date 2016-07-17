#pragma once
#include <vector>
#include <iscore/tools/NotifyingMap.hpp>

template<typename T>
std::vector<T*> shallow_copy(const IdContainer<T>& m)
{
    return {m.get().begin(), m.get().end()};
}

template<typename T>
std::vector<T*> shallow_copy(const NotifyingMap<T>& m)
{
    return shallow_copy(m.map());
}
