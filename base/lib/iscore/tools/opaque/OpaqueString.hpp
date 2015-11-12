#pragma once

#include <string>
#include <functional>

class OpaqueString
{
        friend struct std::hash<OpaqueString>;
        friend bool operator==(const OpaqueString& lhs, const OpaqueString& rhs) {
            return lhs.impl == rhs.impl;
        }

        friend bool operator<(const OpaqueString& lhs, const OpaqueString& rhs) {
            return lhs.impl < rhs.impl;
        }

    public:
        OpaqueString() = default;
        OpaqueString(const char* str): impl{str} {}
        OpaqueString(const std::string& str): impl{str} {}
        OpaqueString(std::string&& str): impl{std::move(str)} {}

    protected:
        std::string impl;
};

namespace std
{
template<>
struct hash<OpaqueString>
{
        std::size_t operator()(const OpaqueString& kagi) const noexcept
        {
            return std::hash<std::string>()(kagi.impl);
        }
};
}
