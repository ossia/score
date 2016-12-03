#pragma once
#include <QMetaType>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>

#include <iscore_lib_state_export.h>
#include <memory>

namespace ossia
{
struct unit_t;
}
namespace State
{
struct ISCORE_LIB_STATE_EXPORT Unit
{
  Q_GADGET
public:
  Unit() noexcept;
  Unit(const Unit& other) noexcept;
  Unit(Unit&& other) noexcept;
  Unit& operator=(const Unit& other) noexcept;
  Unit& operator=(Unit&& other) noexcept;
  ~Unit();

  Unit(const ossia::unit_t&) noexcept;
  Unit& operator=(const ossia::unit_t&) noexcept;

  operator const ossia::unit_t&() const noexcept;
  operator ossia::unit_t&() noexcept;

  bool operator==(const State::Unit& other) const noexcept;
  bool operator!=(const State::Unit& other) const noexcept;

  const ossia::unit_t& get() const noexcept;
  ossia::unit_t& get() noexcept;

private:
  std::unique_ptr<ossia::unit_t> unit;
};
}

template <>
struct ISCORE_LIB_STATE_EXPORT TSerializer<DataStream, void, ossia::unit_t>
{
  static void readFrom(DataStream::Serializer& s, const ossia::unit_t& var);

  static void writeTo(DataStream::Deserializer& s, ossia::unit_t& var);
};

template <>
struct ISCORE_LIB_STATE_EXPORT TSerializer<DataStream, void, State::Unit>
{
  static void readFrom(DataStream::Serializer& s, const State::Unit& var);

  static void writeTo(DataStream::Deserializer& s, State::Unit& var);
};

Q_DECLARE_METATYPE(State::Unit)
