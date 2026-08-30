#pragma once
#include <score/serialization/JSONVisitor.hpp>

#include <ossia/math/safe_math.hpp>

#include <QSize>
#include <QString>

#include <cmath>
#include <cstddef>
#include <limits>
#include <type_traits>

namespace score
{

/**
 * @brief Read one field of a JSON object, or leave @p out alone.
 *
 * rapidjson's operator[] and GetString/GetInt/GetDouble are RAPIDJSON_ASSERT:
 * an absent key, or one holding the wrong type, aborts rather than failing.
 * These check presence and type first and report whether the field was usable,
 * which is what a settings object arriving from a script or a saved document
 * needs.
 */
template <typename Obj, std::size_t N>
bool parseJsonField(const Obj& obj, const char (&key)[N], QString& out)
{
  if(auto v = obj.tryGet(key); v && v->obj.IsString())
  {
    out = v->toString();
    return true;
  }
  return false;
}

template <typename Obj, std::size_t N>
bool parseJsonField(const Obj& obj, const char (&key)[N], bool& out)
{
  if(auto v = obj.tryGet(key); v && v->obj.IsBool())
  {
    out = v->obj.GetBool();
    return true;
  }
  return false;
}

//! Narrowing a double that does not fit the destination is undefined, so the
//! range is part of the type check.
template <typename T>
bool parseJsonNumber(double d, T& out) noexcept
{
  if(!ossia::safe_isfinite(d))
    return false;
  if constexpr(!std::is_floating_point_v<T>)
  {
    if(d < double(std::numeric_limits<long long>::min())
       || d > double(std::numeric_limits<long long>::max()))
      return false;
    out = static_cast<T>(static_cast<long long>(d));
  }
  else
  {
    out = static_cast<T>(d);
  }
  return true;
}

template <typename Obj, std::size_t N, typename T>
  requires(
      (std::is_arithmetic_v<T> || std::is_enum_v<T>) && !std::is_same_v<T, bool>)
bool parseJsonField(const Obj& obj, const char (&key)[N], T& out)
{
  auto v = obj.tryGet(key);
  if(!v || !v->obj.IsNumber())
    return false;

  if constexpr(std::is_floating_point_v<T>)
  {
    out = static_cast<T>(v->obj.GetDouble());
    return true;
  }
  else if constexpr(std::is_integral_v<T> && std::is_unsigned_v<T>)
  {
    if(v->obj.IsUint64())
    {
      out = static_cast<T>(v->obj.GetUint64());
      return true;
    }
    if(v->obj.IsInt64())
    {
      out = static_cast<T>(v->obj.GetInt64());
      return true;
    }
  }
  else
  {
    if(v->obj.IsInt64())
    {
      out = static_cast<T>(v->obj.GetInt64());
      return true;
    }
    if(v->obj.IsUint64())
    {
      const auto u = v->obj.GetUint64();
      if(u > uint64_t(std::numeric_limits<long long>::max()))
        return false;
      out = static_cast<T>(static_cast<long long>(u));
      return true;
    }
  }
  return parseJsonNumber(v->obj.GetDouble(), out);
}

template <typename Obj, std::size_t N>
bool parseJsonField(const Obj& obj, const char (&key)[N], QSize& out)
{
  auto v = obj.tryGet(key);
  if(!v || !v->obj.IsArray())
    return false;
  const auto& arr = v->obj.GetArray();
  if(arr.Size() < 2 || !arr[0].IsNumber() || !arr[1].IsNumber())
    return false;

  int w{}, h{};
  if(!parseJsonNumber(arr[0].GetDouble(), w) || !parseJsonNumber(arr[1].GetDouble(), h))
    return false;
  out = QSize{w, h};
  return true;
}

}
