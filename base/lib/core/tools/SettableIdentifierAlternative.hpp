#pragma once
#include <boost/optional.hpp>

template<typename tag, typename value_type>
class id
{
public:

    explicit id() = default;
    explicit id(value_type val) : m_id{val} { }

    friend bool operator==(const id& lhs, const id& rhs)
    {
        return lhs.m_id == rhs.m_id;
    }

    friend bool operator!=(const id& lhs, const id& rhs)
    {
        return lhs.m_id != rhs.m_id;
    }

    explicit operator value_type() const
    { return m_id; }

private:
    value_type m_id{};
};

template<typename tag, typename impl>
using optional_tagged_id = id<tag, boost::optional<impl>>;

template<typename tag>
using optional_tagged_int32_id = optional_tagged_id<tag, int32_t>;

template<typename T>
class id_mixin : public T
{
public:
    using id_type = optional_tagged_int32_id<T>;
    template <typename... Args>
    explicit id_mixin(Args&&... args):
        T{std::forward<Args>(args)...}
    {

    }

    template <typename... Args>
    explicit id_mixin(optional_tagged_int32_id<T> id,
                      Args&&... args):
        T{std::forward<Args>(args)...},
        m_id{id}
    {

    }

    optional_tagged_int32_id<T> id() const
    {
        return m_id;
    }

private:
    optional_tagged_int32_id<T> m_id;
};
