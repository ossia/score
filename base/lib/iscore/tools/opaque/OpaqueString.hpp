#pragma once
#include <QString>
#include <string>
#include <functional>

#include <iscore/tools/std/ConstexprString.hpp>

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

#if defined(USE_CONSTEXPR_STRING)
        template <typename charT, charT... Chars>
        explicit OpaqueString(const std::basic_string_literal<charT, Chars...>& str): impl{str.c_str()} {}
#endif
        explicit OpaqueString(const char* str): impl{str} {}
        explicit OpaqueString(const std::string& str): impl{str} {}
        explicit OpaqueString(const QString& str): impl{str.toStdString()} {}
        explicit OpaqueString(std::string&& str): impl{std::move(str)} {}

        explicit OpaqueString(const OpaqueString& str): impl{str.impl} {}
        explicit OpaqueString(OpaqueString&& str): impl{std::move(str.impl)} {}

        OpaqueString& operator=(const OpaqueString& str) { impl = str.impl; return *this; }
        OpaqueString& operator=(OpaqueString&& str) { impl = std::move(str.impl); return *this; }

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
