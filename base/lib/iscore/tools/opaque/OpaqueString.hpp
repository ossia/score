#pragma once
#include <QString>
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

        explicit OpaqueString(const char* str) noexcept : impl{str} {}
        explicit OpaqueString(std::string str) noexcept : impl{std::move(str)} {}
        explicit OpaqueString(const QString& str) noexcept : impl{str.toStdString()} {}

        explicit OpaqueString(const OpaqueString& str) = default;
        explicit OpaqueString(OpaqueString&& str) noexcept = default;

        OpaqueString& operator=(const OpaqueString& str) = default;
        OpaqueString& operator=(OpaqueString&& str) noexcept = default;

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
