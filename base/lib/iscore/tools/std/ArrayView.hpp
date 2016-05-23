#pragma once

#include <cstddef>
#include <cassert>
#include <stdexcept>

#if defined(_WIN32)
  #include <malloc.h>
#else
  #include <alloca.h>
#endif


namespace iscore
{
template<typename T>
class dynarray_impl
{
    private:
        T* m_ptr{};
        std::size_t m_size{};

    public:
        using value_type = T;
        dynarray_impl(T* t, std::size_t size):
            m_ptr{t},
            m_size{size}
        {
        }

        dynarray_impl(const dynarray_impl& other) = default;
        dynarray_impl(dynarray_impl&& other) = default;
        dynarray_impl& operator=(const dynarray_impl& other) = default;
        dynarray_impl& operator=(dynarray_impl&& other) = default;

        auto begin() const { return m_ptr; }
        auto end() const { return m_ptr + m_size; }

        auto size() const { return m_size; }

        T& operator[](std::size_t pos) const
        {
            ISCORE_ASSERT(m_ptr);
            ISCORE_ASSERT(pos < m_size);
            return *(m_ptr + pos);
        }

        T& at(std::size_t pos) const
        {
            if(!m_ptr || !(pos < m_size))
                throw std::out_of_range{"pos >= m_size"};

            return *(m_ptr + pos);
        }
};


#define make_dynarray(Type, Count) iscore::dynarray_impl<Type>{(Type*)alloca(sizeof(Type) * Count), Count}


template<typename T>
class dynvector_impl
{
    private:
        T* m_ptr{};
        std::size_t m_size{};
        std::size_t m_capacity{};

    public:
        using value_type = T;
        using iterator = T*;
        using const_iterator = T*;

        dynvector_impl(T* t, std::size_t capacity):
            m_ptr{t},
            m_capacity{capacity}
        {
        }

        dynvector_impl(const dynvector_impl& other) = default;
        dynvector_impl(dynvector_impl&& other) = default;
        dynvector_impl& operator=(const dynvector_impl& other) = default;
        dynvector_impl& operator=(dynvector_impl&& other) = default;

        iterator begin() const { return m_ptr; }
        iterator end() const { return m_ptr + m_size; }

        std::size_t size() const { return m_size; }

        T& operator[](std::size_t pos) const
        {
            ISCORE_ASSERT(m_ptr);
            ISCORE_ASSERT(pos < m_size);
            return *(m_ptr + pos);
        }

        T& at(std::size_t pos) const
        {
            if(!m_ptr || !(pos < m_size))
                throw std::out_of_range{"pos >= m_size"};

            return *(m_ptr + pos);
        }

        void push_back(T&& t)
        {
            ISCORE_ASSERT(m_size + 1 <= m_capacity);
            *(m_ptr + m_size) = std::move(t);
            m_size++;
        }

        void push_back(const T& t)
        {
            ISCORE_ASSERT(m_size + 1 <= m_capacity);
            *(m_ptr + m_size) = t;
            m_size++;
        }

};

#define make_dynvector(Type, Count) iscore::dynvector_impl<Type>{(Type*)alloca(sizeof(Type) * Count), Count}
}
