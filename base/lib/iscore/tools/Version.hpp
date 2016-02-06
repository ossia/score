#pragma once
#include <cstdint>

namespace iscore
{
class Version
{
    public:
        explicit Version(int32_t v): m_impl{v} { }
        Version(const Version&) = default;
        Version(Version&&) = default;
        Version& operator=(const Version&) = default;
        Version& operator=(Version&&) = default;

        bool operator==(Version other) { return m_impl == other.m_impl; }
        bool operator!=(Version other) { return m_impl != other.m_impl; }
        bool operator<(Version other) { return m_impl < other.m_impl; }
        bool operator>(Version other) { return m_impl > other.m_impl; }
        bool operator<=(Version other) { return m_impl <= other.m_impl; }
        bool operator>=(Version other) { return m_impl >= other.m_impl; }

    private:
        int32_t m_impl = 0;
};
}
