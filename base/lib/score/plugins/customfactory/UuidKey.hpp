#pragma once
#include <score/tools/Todo.hpp>
#include <score_lib_base_export.h>
#include <array>

class JSONObject;
namespace score
{
namespace uuids
{
// Taken from boost.uuid
struct uuid
{
public:
  typedef uint8_t value_type;
  typedef uint8_t& reference;
  typedef uint8_t const& const_reference;
  typedef uint8_t* iterator;
  typedef uint8_t const* const_iterator;
  typedef std::size_t size_type;
  typedef std::ptrdiff_t difference_type;

  static constexpr size_type static_size() noexcept
  {
    return 16;
  }

public:
  constexpr uuid() noexcept : data{{}}
  {
  }

  constexpr uuid(const uuid& other) noexcept : data{other.data}
  {
  }

  constexpr uuid(uint8_t* other) noexcept
      : data{other[0],  other[1],  other[2],  other[3], other[4],  other[5],
             other[6],  other[7],  other[8],  other[9], other[10], other[11],
             other[12], other[13], other[14], other[15]}
  {
  }

  constexpr auto begin() noexcept
  {
    return data.data();
  }
  constexpr auto end() noexcept
  {
    return data.data() + data.size();
  }

  constexpr auto begin() const noexcept
  {
    return data.data();
  }
  constexpr auto end() const noexcept
  {
    return data.data() + data.size();
  }

  constexpr size_type size() const noexcept
  {
    return static_size();
  }

  constexpr bool is_nil() const noexcept
  {
    for (std::size_t i = 0; i < sizeof(data); ++i)
    {
      if (data[i] != 0U)
        return false;
    }
    return true;
  }

  enum variant_type
  {
    variant_ncs,       // NCS backward compatibility
    variant_rfc_4122,  // defined in RFC 4122 document
    variant_microsoft, // Microsoft Corporation backward compatibility
    variant_future     // future definition
  };
  variant_type variant() const noexcept
  {
    // variant is stored in octet 7
    // which is index 8, since indexes count backwards
    unsigned char octet7 = data[8]; // octet 7 is array index 8
    if ((octet7 & 0x80) == 0x00)
    { // 0b0xxxxxxx
      return variant_ncs;
    }
    else if ((octet7 & 0xC0) == 0x80)
    { // 0b10xxxxxx
      return variant_rfc_4122;
    }
    else if ((octet7 & 0xE0) == 0xC0)
    { // 0b110xxxxx
      return variant_microsoft;
    }
    else
    {
      // assert( (octet7 & 0xE0) == 0xE0 ) // 0b111xxxx
      return variant_future;
    }
  }

  enum version_type
  {
    version_unknown = -1,
    version_time_based = 1,
    version_dce_security = 2,
    version_name_based_md5 = 3,
    version_random_number_based = 4,
    version_name_based_sha1 = 5
  };
  version_type version() const noexcept
  {
    // version is stored in octet 9
    // which is index 6, since indexes count backwards
    uint8_t octet9 = data[6];
    if ((octet9 & 0xF0) == 0x10)
    {
      return version_time_based;
    }
    else if ((octet9 & 0xF0) == 0x20)
    {
      return version_dce_security;
    }
    else if ((octet9 & 0xF0) == 0x30)
    {
      return version_name_based_md5;
    }
    else if ((octet9 & 0xF0) == 0x40)
    {
      return version_random_number_based;
    }
    else if ((octet9 & 0xF0) == 0x50)
    {
      return version_name_based_sha1;
    }
    else
    {
      return version_unknown;
    }
  }

public:
  // or should it be array<uint8_t, 16>
  std::array<uint8_t, 16> data;
};

constexpr inline bool operator==(uuid const& lhs, uuid const& rhs) noexcept
{
  for (std::size_t i = 0; i < uuid::static_size(); ++i)
  {
    if (lhs.data[i] != rhs.data[i])
      return false;
  }
  return true;
}

constexpr inline bool operator<(uuid const& lhs, uuid const& rhs) noexcept
{
  for (std::size_t i = 0; i < uuid::static_size(); ++i)
  {
    if (lhs.data[i] > rhs.data[i])
      return false;
  }
  return true;
}

constexpr inline bool operator!=(uuid const& lhs, uuid const& rhs) noexcept
{
  return !(lhs == rhs);
}

constexpr inline bool operator>(uuid const& lhs, uuid const& rhs) noexcept
{
  return rhs < lhs;
}

constexpr inline bool operator<=(uuid const& lhs, uuid const& rhs) noexcept
{
  return !(rhs < lhs);
}

constexpr inline bool operator>=(uuid const& lhs, uuid const& rhs) noexcept
{
  return !(lhs < rhs);
}

// This is equivalent to boost::hash_range(u.begin(), u.end());
constexpr inline std::size_t hash_value(uuid const& u) noexcept
{
  std::size_t seed = 0;
  for (uuid::const_iterator i = u.begin(), e = u.end(); i != e; ++i)
  {
    seed ^= static_cast<std::size_t>(*i) + 0x9e3779b9 + (seed << 6)
            + (seed >> 2);
  }

  return seed;
}

struct string_generator
{
  typedef uuid result_type;

  template <int N>
  static constexpr uuid compute(const char (&s)[N])
  {
    static_assert(N == 37, "Invalid uuid");
    return compute(s, s + N);
  }

  template <typename CharIterator>
  static constexpr uuid compute(CharIterator begin, CharIterator end)
  {
    // check open brace
    auto c = *begin++;

    bool has_dashes = false;

    uint8_t u[16]{};
    int i = 0;
    for (auto it_byte = u; it_byte != u + 16; ++it_byte, ++i)
    {
      if (it_byte != u)
      {
        c = *begin++;
      }

      if (i == 4)
      {
        has_dashes = is_dash(c);
        if (has_dashes)
        {
          c = *begin++;
        }
      }

      if (has_dashes)
      {
        if (i == 6 || i == 8 || i == 10)
        {
          if (is_dash(c))
          {
            c = *begin++;
          }
          else
          {
            throw std::runtime_error{"Invalid uuid"};
          }
        }
      }
      auto res = get_value(c);
      c = *begin++;
      res <<= 4;
      res |= get_value(c);
      *it_byte = res;
    }

    return u;
  }

private:
  static constexpr unsigned char get_value(char c)
  {
    switch (c)
    {
      case '0':
        return 0;
      case '1':
        return 1;
      case '2':
        return 2;
      case '3':
        return 3;
      case '4':
        return 4;
      case '5':
        return 5;
      case '6':
        return 6;
      case '7':
        return 7;
      case '8':
        return 8;
      case '9':
        return 9;
      case 'a':
      case 'A':
        return 10;
      case 'b':
      case 'B':
        return 11;
      case 'c':
      case 'C':
        return 12;
      case 'd':
      case 'D':
        return 13;
      case 'e':
      case 'E':
        return 14;
      case 'f':
      case 'F':
        return 15;
      default:
        throw std::runtime_error{"Invalid uuid"};
    }
  }

  static constexpr unsigned char get_value(QChar c)
  {
    return get_value(c.toLatin1());
  }

  static constexpr bool is_dash(char c)
  {
    return c == '-';
  }
  static constexpr bool is_dash(QChar c)
  {
    return c.toLatin1() == '-';
  }
};
}
using uuid_t = uuids::uuid;
}

#define return_uuid(text)                                                     \
  do                                                                          \
  {                                                                           \
    constexpr const auto t = score::uuids::string_generator::compute((text)); \
    return t;                                                                 \
  } while (0)

template <typename Tag>
class UuidKey : score::uuid_t
{
  using this_type = UuidKey<Tag>;

  friend struct std::hash<this_type>;
  //friend struct boost::hash<this_type>;
  //friend struct boost::hash<const this_type>;
  friend constexpr bool operator==(const this_type& lhs, const this_type& rhs)
  {
    return static_cast<const score::uuid_t&>(lhs)
           == static_cast<const score::uuid_t&>(rhs);
  }
  friend constexpr bool operator!=(const this_type& lhs, const this_type& rhs)
  {
    return static_cast<const score::uuid_t&>(lhs)
           != static_cast<const score::uuid_t&>(rhs);
  }
  friend constexpr bool operator<(const this_type& lhs, const this_type& rhs)
  {
    return static_cast<const score::uuid_t&>(lhs)
           < static_cast<const score::uuid_t&>(rhs);
  }

public:
  constexpr UuidKey() noexcept = default;
  constexpr UuidKey(const UuidKey& other) noexcept = default;
  constexpr UuidKey(UuidKey&& other) noexcept = default;
  constexpr UuidKey& operator=(const UuidKey& other) noexcept = default;
  constexpr UuidKey& operator=(UuidKey&& other) noexcept = default;

  constexpr UuidKey(score::uuid_t other) noexcept : score::uuid_t(other)
  {
  }

  template <int N>
  explicit constexpr UuidKey(const char (&txt)[N])
      : score::uuid_t(score::uuids::string_generator::compute<N>(txt))
  {
  }

  template <typename Iterator>
  constexpr UuidKey(Iterator beg_it, Iterator end_it)
      : score::uuid_t(score::uuids::string_generator::compute(beg_it, end_it))
  {
  }

  constexpr static UuidKey fromString(const std::string& str)
  {
    return UuidKey{str.begin(), str.end()};
  }

  constexpr static UuidKey fromString(const QString& str)
  {
    return UuidKey{str.begin(), str.end()};
  }

  constexpr const score::uuid_t& impl() const
  {
    return *this;
  }
  constexpr score::uuid_t& impl()
  {
    return *this;
  }
};

namespace std
{
template <typename T>
struct hash<UuidKey<T>>
{
  constexpr std::size_t operator()(const UuidKey<T>& kagi) const noexcept
  {
    return score::uuids::hash_value(kagi.impl());
  }
};
}
