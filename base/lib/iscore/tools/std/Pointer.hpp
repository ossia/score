#pragma once
#include <memory>
#include <typeinfo>

// http://stackoverflow.com/a/21174979/1495627
template<typename Derived, typename Base, typename Del>
std::unique_ptr<Derived>
static_unique_ptr_cast( std::unique_ptr<Base, Del>&& p )
{
    return std::unique_ptr<Derived>(static_cast<Derived *>(p.release()));
}

template<typename Derived, typename Base, typename Del>
std::unique_ptr<Derived>
dynamic_unique_ptr_cast( std::unique_ptr<Base, Del>&& p )
{
    if(Derived *result = dynamic_cast<Derived *>(p.get())) {
        p.release();
        return std::unique_ptr<Derived>(result);
    }
    return nullptr;
}

#ifdef ISCORE_DEBUG
template<typename T,
         typename U>
auto safe_unique_ptr_cast(std::unique_ptr<U> other)
{
    auto res = dynamic_unique_ptr_cast<T>(other);
    ISCORE_ASSERT(res);
    return res;
}

#else
#define safe_unique_ptr_cast static_unique_ptr_cast
#endif
