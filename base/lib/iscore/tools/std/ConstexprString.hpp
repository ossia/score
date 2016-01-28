#pragma once
#if !defined(USE_CONSTEXPR_STRING)
#include <string>
namespace std
{
inline std::string operator "" _CS(const char* s, std::size_t size)
{
    return std::string(s, size);
}

}
#else
// Source: https://github.com/michaelbprice/cxx_snippets/blob/master/string_literal/string_literal.cpp

char * gets(char *); // works around silly C++11/14 library issue on ToT clang

#include <iostream>
#include <ostream>
#include <string>
#include <type_traits>

namespace std {

#pragma region Some forward declarations

template <typename charT, charT... Chars>
struct basic_string_literal;

template <typename charT, charT... Chars>
constexpr basic_string_literal<charT, Chars...> operator"" _CS();

#pragma endregion

#pragma region Some helper types

template <size_t N, typename charT, charT HeadChar, charT... TailChars>
struct basic_string_literal_indexer
{
    static_assert(N < sizeof...(TailChars) + 1, "Index is too big");
    using type = typename basic_string_literal_indexer<N - 1, charT, TailChars...>::type;
};

template <typename charT, charT HeadChar, charT... TailChars>
struct basic_string_literal_indexer<0, charT, HeadChar, TailChars...>
{
    using type = basic_string_literal<charT, HeadChar, TailChars...>;
};

// TODO: basic_string_literal_reverser - Might need name-able parameter packs...

#pragma endregion


template <typename charT, char HeadChar, char... TailChars>
struct basic_string_literal<charT, HeadChar, TailChars...>
{
public:
    static constexpr const charT _data[] = { HeadChar, TailChars..., '\0' };

#pragma region Friends and constructors (broken)

//    template <typename charU, charU... CharUs>
//    friend constexpr basic_string_literal operator""_CS <charU, CharUs...>();
//
//private:
    constexpr basic_string_literal() = default;

#pragma endregion

public:
    #pragma region Type aliases & static values

    using type = basic_string_literal;
    using value_type = charT;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    // iterator types???

    using tail_type = basic_string_literal<charT, TailChars...>;
    //using reverse_type = basic_string_literal_reverser<charT, HeadChar, TailChars...>::type

    //const static charT head_value = HeadChar;
    const static size_type size_value = sizeof...(TailChars) + 1;

    #pragma endregion

    #pragma region Size-related functions

    constexpr size_type size() const
    {
        return size_value;
    }

    constexpr size_type length() const
    {
        return size_value;
    }

    constexpr bool empty() const
    {
        return false;
    }

    #pragma endregion

    #pragma region Accessors

    constexpr charT operator[] (size_t pos) const
    {
        return _data[pos];
    }

    constexpr charT at(size_t pos) const
    {
        return _data[pos];
    }

    constexpr charT back() const
    {
        return _data[size_value - 1];
    }

    constexpr charT front() const
    {
        return _data[0];
    }

    #pragma endregion

    #pragma region Searching

    template <charT... LookForChars>
    constexpr size_type find(const basic_string_literal<charT, LookForChars...>& str, size_type pos = 0) const
    {
        //static_assert(sizeof...(Chars) > 0, "find() is not valid for an empty string");
        //static_assert(sizeof...(LookForChars) > 0, "The parameter to find() cannot be an empty string");

        for (size_type i = 0; i + pos < size(); ++i)
        {
            bool wasMatched = true;

            for (size_type j = 0; i + pos + j < size() && j < str.size(); ++j)
            {
                if (_data[i + pos + j] != str._data[j])
                {
                    wasMatched = false;
                    break;
                }
            }

            if (wasMatched)
                return i + pos;
        }

        return std::string::npos; // TODO: Should I have my own npos?
    }

    constexpr size_type find(const charT* s, size_type pos, size_type count) const;

    constexpr size_type find(const charT* s, size_type pos = 0) const;

    constexpr size_type find(charT ch, size_t pos = 0) const
    {
        for (size_type i = 0; i < size(); ++i)
        {
            if (_data[i + pos] == ch)
                return i + pos;
        }

        return std::string::npos;
    }

    template <charT... LookForChars>
    constexpr size_type rfind(const basic_string_literal<charT, LookForChars...>& str, size_type pos = size_value) const
    {
        //static_assert(sizeof...(Chars) > 0, "find() is not valid for an empty string");
        //static_assert(sizeof...(LookForChars) > 0, "The parameter to find() cannot be an empty string");

        for ( ; pos > 0; --pos)
        {
            for (size_type j = 0; pos + j < size() && j < str.size(); ++j)
            {
                // TODO: Do I need to subtract one from 'pos' to get the right index here...

                if (_data[pos + j] != str._data[j])
                    break;

                if (j == str.size() - 1)
                    return pos;
            }
        }

        return std::string::npos;
    }

    constexpr size_type rfind(const charT* s, size_type pos, size_type count) const;

    constexpr size_type rfind(const charT* s, size_type pos = size_value) const;

    constexpr size_type rfind(charT ch, size_t pos = size_value - 1) const
    {
        for ( ; pos > 0; --pos)
        {
            if (_data[pos - 1] == ch)
                return pos - 1;
        }

        return std::string::npos;
    }

    template <charT... LookForChars>
    constexpr size_type find_first_of(const basic_string_literal<charT, LookForChars...>& str, size_type pos = 0) const;
    constexpr size_type find_first_of(const charT* s, size_type pos, size_type count) const;
    constexpr size_type find_first_of(const charT* s, size_type pos = 0) const;
    constexpr size_type find_first_of(charT ch, size_type pos = 0) const;

    template <charT... LookForChars>
    constexpr size_type find_first_not_of(const basic_string_literal<charT, LookForChars...>& str, size_type pos = 0) const;
    constexpr size_type find_first_not_of(const charT* s, size_type pos, size_type count) const;
    constexpr size_type find_first_not_of(const charT* s, size_type pos = 0) const;
    constexpr size_type find_first_not_of(charT ch, size_type pos = 0) const;

    template <charT... LookForChars>
    constexpr size_type find_last_of(const basic_string_literal<charT, LookForChars...>& str, size_type pos = size_value) const;
    constexpr size_type find_last_of(const charT* s, size_type pos, size_type count) const;
    constexpr size_type find_last_of(const charT* s, size_type pos = size_value) const;
    constexpr size_type find_last_of(charT ch, size_t pos = size_value) const;

    template <charT... LookForChars>
    constexpr size_type find_last_not_of(const basic_string_literal<charT, LookForChars...>& str, size_type pos = size_value) const;
    constexpr size_type find_last_not_of(const charT* s, size_type pos, size_type count) const;
    constexpr size_type find_last_not_of(const charT* s, size_type pos = size_value) const;
    constexpr size_type find_last_not_of(charT ch, size_t pos = size_value) const;

#pragma endregion

    #pragma region Substrings

    template <size_type Pos>
    constexpr auto substr() const
        -> typename basic_string_literal_indexer<Pos, charT, HeadChar, TailChars...>::type
    {
        return typename basic_string_literal_indexer<Pos, charT, HeadChar, TailChars...>::type();
    }

    constexpr tail_type cdr () const
    {
        return tail_type();
    }

    #pragma endregion

    #pragma region Comparison

    template <charT... OtherChars>
    constexpr int compare(const basic_string_literal<charT, OtherChars...>& other) const
    {
        for (size_type i = 0; i < size() && i < other.size(); ++i)
        {
            if (_data[i] < other._data[i])
                return -1;

            if (_data[i] > other._data[i])
                return 1;
        }

        if (size() < other.size())
            return -1;

        if (size() > other.size())
            return 1;

        return 0;
    }

    constexpr int compare(const basic_string<charT>& str) const;
    constexpr int compare(std::size_t pos1, std::size_t count1, const basic_string<charT>& str) const;
    constexpr int compare(std::size_t pos1, std::size_t count1, const basic_string<charT>& str,
        std::size_t pos2, std::size_t count2) const;
    constexpr int compare(const charT* s) const;
    constexpr int compare(std::size_t pos1, std::size_t count1, const charT* s) const;
    constexpr int compare(std::size_t pos1, std::size_t count1,
        const charT* s, std::size_t count2) const;

    #pragma endregion

    #pragma region Conversions

    constexpr const char * c_str () const
    {
        return _data;
    }

    constexpr const char * data () const
    {
        return _data;
    }

    std::string to_string () const
    {
        return _data;
    }

    constexpr difference_type to_number () const
    {
        static_assert((_data[0] >= '0' && _data[0] <= '9'), "Not a numeric string");

        if (size_value == 1)
        {
            return _data[0] - '0';
        }

        const difference_type right_side_value = cdr().to_number();

        auto power_of_ten = size_value - 1;
        auto multiplier = size_type{1};

        for (size_type i = 0; i < power_of_ten; ++i)
            multiplier *= 10;

        return ((_data[0] - '0') * multiplier) + right_side_value;
    }

    constexpr operator const char* () const
    {
        return _data;
    }

    operator std::string () const
    {
        return _data;
    }

    constexpr operator size_type () const
    {
        return to_number();
    }

    #pragma endregion

};

template<typename charT>
struct basic_string_literal<charT>
{

#pragma region Friends and constructors (broken)

//template <typename charU>
//friend constexpr basic_string_literal operator "" _CS <charU>();
//
//private:
    static constexpr const charT _data[] = { '\0' };

    constexpr basic_string_literal() = default;

#pragma endregion

public:
    #pragma region Type aliases

    using type = basic_string_literal;
    using value_type = charT;
    using size_type = std::size_t;
    using difference_type = std::size_t;
    // iterator types???

    using tail_type = basic_string_literal<charT>;
    //using reverse_type = basic_string_literal<charT>;

    #pragma endregion

    #pragma region Size-related functions

    constexpr size_type size() const
    {
        return 0;
    }

    constexpr size_type length() const
    {
        return 0;
    }

    constexpr bool empty() const
    {
        return true;
    }

    #pragma endregion

    constexpr charT operator[] (size_t pos) const
    {
        return _data[0];
    }

    constexpr charT at(size_t pos) const
    {
        return _data[0];
    }

    constexpr charT back() const
    {
        return _data[0];
    }

    constexpr charT front() const
    {
        return _data[0];
    }


    // There are no search functions for empty strings

    // There are no substring functions for empty strings

    #pragma region Comparison

    template <charT... OtherChars>
    constexpr int compare(const basic_string_literal<charT, OtherChars...>& other) const
    {
        return other.empty() ? 0 : -1;
    }

    constexpr int compare(const basic_string<charT>& str) const
    {
        return str.empty() ? 0 : -1;
    }

    constexpr int compare(std::size_t pos1, std::size_t count1, const basic_string<charT>& str) const;
    constexpr int compare(std::size_t pos1, std::size_t count1, const basic_string<charT>& str,
                          std::size_t pos2, std::size_t count2) const;
    constexpr int compare(const charT* s) const;
    constexpr int compare(std::size_t pos1, std::size_t count1, const charT* s) const;
    constexpr int compare(std::size_t pos1, std::size_t count1,
                          const charT* s, std::size_t count2) const;

    #pragma endregion

    #pragma region Conversions

    constexpr const char * c_str() const
    {
        return _data;
    }

    constexpr const char * data() const
    {
        return _data;
    }

    std::string to_string() const
    {
        return _data;
    }

    // TODO: Make this only accessible to other basic_string_literals
    constexpr size_type to_number() const
    {
        return 0;
    }

    constexpr operator const char* () const
    {
        return _data;
    }

    operator std::string() const
    {
        return _data;
    }

    #pragma endregion

};

#pragma region Friend functions

#pragma region User-defined literal

template <typename charT, charT... Chars>
constexpr basic_string_literal<charT, Chars...> operator"" _CS()
{
    return basic_string_literal<charT, Chars...>();
}

#pragma endregion

#pragma region Concatenation

template <typename charT, charT... LeftChars, charT... RightChars>
constexpr auto operator+ (const basic_string_literal<charT, LeftChars...> &l,
                          const basic_string_literal<charT, RightChars...> &r)
    -> basic_string_literal<charT, LeftChars..., RightChars...>
{
    return basic_string_literal<charT, LeftChars..., RightChars...>();
}

#pragma endregion

#pragma region Boolean operators

template <typename charT, charT... LeftChars, charT... RightChars>
constexpr bool operator== (const basic_string_literal<charT, LeftChars...>& lhs,
                           const basic_string_literal<charT, RightChars...>& rhs)
{
    return (lhs.compare(rhs) == 0);
}

template <typename charT, charT... LeftChars, charT... RightChars>
constexpr bool operator!= (const basic_string_literal<charT, LeftChars...>& lhs,
                           const basic_string_literal<charT, RightChars...>& rhs)
{
    return (lhs.compare(rhs) != 0);
}

template <typename charT, charT... LeftChars, charT... RightChars>
constexpr bool operator< (const basic_string_literal<charT, LeftChars...>& lhs,
                          const basic_string_literal<charT, RightChars...>& rhs)
{
    return (lhs.compare(rhs) < 0);
}

template <typename charT, charT... LeftChars, charT... RightChars>
constexpr bool operator<= (const basic_string_literal<charT, LeftChars...>& lhs,
                           const basic_string_literal<charT, RightChars...>& rhs)
{
    return (lhs.compare(rhs) <= 0);
}

template <typename charT, charT... LeftChars, charT... RightChars>
constexpr bool operator> (const basic_string_literal<charT, LeftChars...>& lhs,
                          const basic_string_literal<charT, RightChars...>& rhs)
{
    return (lhs.compare(rhs) > 0);
}

template <typename charT, charT... LeftChars, charT... RightChars>
constexpr bool operator>= (const basic_string_literal<charT, LeftChars...>& lhs,
                           const basic_string_literal<charT, RightChars...>& rhs)
{
    return (lhs.compare(rhs) >= 0);
}


template <typename charT, charT... RightChars>
constexpr bool operator== (const charT* lhs, const basic_string_literal<charT, RightChars...>& rhs);

template <typename charT, charT... LeftChars>
constexpr bool operator== (const basic_string_literal<charT, LeftChars...>& lhs, const charT* rhs);

template <typename charT, charT... RightChars>
constexpr bool operator!= (const charT* lhs, const basic_string_literal<charT, RightChars...>& rhs);

template <typename charT, charT... LeftChars>
constexpr bool operator!= (const basic_string_literal<charT, LeftChars...>& lhs, const charT* rhs);

template <typename charT, charT... RightChars>
constexpr bool operator< (const charT* lhs, const basic_string_literal<charT, RightChars...>& rhs);

template <typename charT, charT... LeftChars>
constexpr bool operator< (const basic_string_literal<charT, LeftChars...>& lhs, const charT* rhs);

template <typename charT, charT... RightChars>
constexpr bool operator<= (const charT* lhs, const basic_string_literal<charT, RightChars...>& rhs);

template <typename charT, charT... LeftChars>
constexpr bool operator<= (const basic_string_literal<charT, LeftChars...>& lhs, const charT* rhs);

template <typename charT, charT... RightChars>
constexpr bool operator> (const charT* lhs, const basic_string_literal<charT, RightChars...>& rhs);

template <typename charT, charT... LeftChars>
constexpr bool operator> (const basic_string_literal<charT, LeftChars...>& lhs, const charT* rhs);

template <typename charT, charT... RightChars>
constexpr bool operator>= (const charT* lhs, const basic_string_literal<charT, RightChars...>& rhs);

template <typename charT, charT... LeftChars>
constexpr bool operator>= (const basic_string_literal<charT, LeftChars...>& lhs, const charT* rhs);

// TODO: Perhaps basic_string overloads as well?

#pragma endregion

#pragma region Stream operators

template <typename charT, charT... Chars>
inline std::ostream& operator<< (std::ostream& os, const basic_string_literal<charT, Chars...>& str)
{
    return (os << str.c_str());
}

#pragma endregion

#pragma endregion

} // namespace std


#endif
using std::operator"" _CS;
