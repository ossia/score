#pragma once

#include <boost/functional/hash.hpp>
#include <score/tools/Todo.hpp>
#include <score_lib_base_export.h>


class JSONObject;
#if !defined(__APPLE__) && defined(__GNUC__) && __GNUC__ < 6
// GCC 5.x isn't happy with throw in constexpr...
#undef Q_DECL_RELAXED_CONSTEXPR
#define Q_DECL_RELAXED_CONSTEXPR
#endif
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

  static Q_DECL_CONSTEXPR size_type static_size() noexcept
  {
    return 16;
  }

public:
  Q_DECL_CONSTEXPR uuid() noexcept : data{{}}
  {
  }

  Q_DECL_RELAXED_CONSTEXPR uuid(const uuid& other) noexcept
      : data{other.data}
  {
  }
/*
  Q_DECL_RELAXED_CONSTEXPR uuid& operator=(const uuid& other) noexcept
  {
    for (std::size_t i = 0; i < sizeof(data); ++i)
    {
      data[i] = other.data[i];
    }
    return *this;
  }
*/
  Q_DECL_CONSTEXPR iterator begin() noexcept
  {
    return data.begin();
  }
  Q_DECL_CONSTEXPR const_iterator begin() const noexcept
  {
    return data.begin();
  }
  Q_DECL_CONSTEXPR iterator end() noexcept
  {
    return data.end();
  }
  Q_DECL_CONSTEXPR const_iterator end() const noexcept
  {
    return data.end();
  }

  Q_DECL_CONSTEXPR size_type size() const noexcept
  {
    return static_size();
  }

  Q_DECL_RELAXED_CONSTEXPR bool is_nil() const noexcept
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

Q_DECL_RELAXED_CONSTEXPR inline bool
operator==(uuid const& lhs, uuid const& rhs) noexcept
{
  for (std::size_t i = 0; i < uuid::static_size(); ++i)
  {
    if (lhs.data[i] != rhs.data[i])
      return false;
  }
  return true;
}

Q_DECL_RELAXED_CONSTEXPR inline bool
operator<(uuid const& lhs, uuid const& rhs) noexcept
{
  for (std::size_t i = 0; i < uuid::static_size(); ++i)
  {
    if (lhs.data[i] > rhs.data[i])
      return false;
  }
  return true;
}

Q_DECL_RELAXED_CONSTEXPR inline bool
operator!=(uuid const& lhs, uuid const& rhs) noexcept
{
  return !(lhs == rhs);
}

Q_DECL_RELAXED_CONSTEXPR inline bool
operator>(uuid const& lhs, uuid const& rhs) noexcept
{
  return rhs < lhs;
}

Q_DECL_RELAXED_CONSTEXPR inline bool
operator<=(uuid const& lhs, uuid const& rhs) noexcept
{
  return !(rhs < lhs);
}

Q_DECL_RELAXED_CONSTEXPR inline bool
operator>=(uuid const& lhs, uuid const& rhs) noexcept
{
  return !(lhs < rhs);
}

// This is equivalent to boost::hash_range(u.begin(), u.end());
Q_DECL_RELAXED_CONSTEXPR inline std::size_t hash_value(uuid const& u) noexcept
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
  static Q_DECL_RELAXED_CONSTEXPR uuid compute(const char (&s)[N])
  {
    if (N != 37)
      throw std::runtime_error{"Invalid uuid"};
    return compute(s, s + N);
  }

  template <typename CharIterator>
  static Q_DECL_RELAXED_CONSTEXPR uuid
  compute(CharIterator begin, CharIterator end)
  {
    // check open brace
    auto c = get_next_char(begin, end);

    bool has_dashes = false;

    uuid u{};
    int i = 0;
    for (auto it_byte = u.begin(); it_byte != u.end(); ++it_byte, ++i)
    {
      if (it_byte != u.begin())
      {
        c = get_next_char(begin, end);
      }

      if (i == 4)
      {
        has_dashes = is_dash(c);
        if (has_dashes)
        {
          c = get_next_char(begin, end);
        }
      }

      if (has_dashes)
      {
        if (i == 6 || i == 8 || i == 10)
        {
          if (is_dash(c))
          {
            c = get_next_char(begin, end);
          }
          else
          {
            throw std::runtime_error{"Invalid uuid"};
          }
        }
      }

      *it_byte = get_value(c);

      c = get_next_char(begin, end);
      *it_byte <<= 4;
      *it_byte |= get_value(c);
    }

    return u;
  }

private:
  template <typename CharIterator>
  static Q_DECL_RELAXED_CONSTEXPR auto
  get_next_char(CharIterator& begin, CharIterator end)
  {
    if (begin == end)
    {
      throw std::runtime_error{"Invalid uuid"};
    }
    return *begin++;
  }

  static Q_DECL_RELAXED_CONSTEXPR unsigned char get_value(char c)
  {
    Q_DECL_RELAXED_CONSTEXPR const auto digits_begin
        = "0123456789abcdefABCDEF";
    Q_DECL_RELAXED_CONSTEXPR unsigned char const values[]
        = {0,
           1,
           2,
           3,
           4,
           5,
           6,
           7,
           8,
           9,
           10,
           11,
           12,
           13,
           14,
           15,
           10,
           11,
           12,
           13,
           14,
           15,
           static_cast<unsigned char>(-1)};

    bool found = false;
    for (int i = 0; i < 22; i++)
    {
      if (digits_begin[i] == c)
      {
        found = true;
        (void)found; // to prevent warnings with static analyzers
        return values[i];
      }
    }

    if (!found)
    {
      throw std::runtime_error{"Invalid uuid"};
    }
    return -1;
  }

  static Q_DECL_RELAXED_CONSTEXPR unsigned char get_value(QChar c)
  {
    return get_value(c.toLatin1());
  }

  static Q_DECL_CONSTEXPR bool is_dash(char c)
  {
    return c == '-';
  }
  static Q_DECL_CONSTEXPR bool is_dash(QChar c)
  {
    return c.toLatin1() == '-';
  }
};
}
using uuid_t = uuids::uuid;
}

#define return_uuid(text)                                   \
  do                                                        \
  {                                                         \
    Q_DECL_RELAXED_CONSTEXPR const auto t                   \
        = score::uuids::string_generator::compute((text)); \
    return t;                                               \
  } while (0)

template <typename Tag>
class UuidKey : score::uuid_t
{
  using this_type = UuidKey<Tag>;

  friend struct std::hash<this_type>;
  friend struct boost::hash<this_type>;
  friend struct boost::hash<const this_type>;
  friend Q_DECL_RELAXED_CONSTEXPR bool
  operator==(const this_type& lhs, const this_type& rhs)
  {
    return static_cast<const score::uuid_t&>(lhs)
           == static_cast<const score::uuid_t&>(rhs);
  }
  friend Q_DECL_RELAXED_CONSTEXPR bool
  operator!=(const this_type& lhs, const this_type& rhs)
  {
    return static_cast<const score::uuid_t&>(lhs)
           != static_cast<const score::uuid_t&>(rhs);
  }
  friend Q_DECL_RELAXED_CONSTEXPR bool
  operator<(const this_type& lhs, const this_type& rhs)
  {
    return static_cast<const score::uuid_t&>(lhs)
           < static_cast<const score::uuid_t&>(rhs);
  }

public:
  Q_DECL_RELAXED_CONSTEXPR UuidKey() noexcept = default;
  Q_DECL_RELAXED_CONSTEXPR UuidKey(const UuidKey& other) noexcept = default;
  Q_DECL_RELAXED_CONSTEXPR UuidKey(UuidKey&& other) noexcept = default;
  Q_DECL_RELAXED_CONSTEXPR UuidKey& operator=(const UuidKey& other) noexcept
      = default;
  Q_DECL_RELAXED_CONSTEXPR UuidKey& operator=(UuidKey&& other) noexcept
      = default;

  Q_DECL_RELAXED_CONSTEXPR UuidKey(score::uuid_t other) noexcept
      : score::uuid_t(other)
  {
  }

  template <int N>
  explicit Q_DECL_RELAXED_CONSTEXPR UuidKey(const char (&txt)[N])
      : score::uuid_t(score::uuids::string_generator::compute<N>(txt))
  {
  }

  template <typename Iterator>
  Q_DECL_RELAXED_CONSTEXPR UuidKey(Iterator beg_it, Iterator end_it)
      : score::uuid_t(
            score::uuids::string_generator::compute(beg_it, end_it))
  {
  }

  Q_DECL_RELAXED_CONSTEXPR static UuidKey fromString(const std::string& str)
  {
    return UuidKey{str.begin(), str.end()};
  }

  Q_DECL_RELAXED_CONSTEXPR static UuidKey fromString(const QString& str)
  {
    return UuidKey{str.begin(), str.end()};
  }

  Q_DECL_RELAXED_CONSTEXPR const score::uuid_t& impl() const
  {
    return *this;
  }
  Q_DECL_RELAXED_CONSTEXPR score::uuid_t& impl()
  {
    return *this;
  }
};

namespace std
{
template <typename T>
struct hash<UuidKey<T>>
{
  Q_DECL_CONSTEXPR std::size_t operator()(const UuidKey<T>& kagi) const
      noexcept
  {
    return boost::hash<score::uuid_t>()(
        static_cast<const score::uuid_t&>(kagi));
  }
};
}

namespace boost
{
template <typename T>
struct hash<UuidKey<T>>
{
  Q_DECL_CONSTEXPR std::size_t operator()(const UuidKey<T>& kagi) const
      noexcept
  {
    return boost::hash<score::uuid_t>()(
        static_cast<const score::uuid_t&>(kagi));
  }
};

template <typename T>
struct hash<const UuidKey<T>>
{
  Q_DECL_CONSTEXPR std::size_t operator()(const UuidKey<T>& kagi) const
      noexcept
  {
    return boost::hash<score::uuid_t>()(
        static_cast<const score::uuid_t&>(kagi));
  }
};
}
