#pragma once
#include <score/serialization/DataStreamVisitor.hpp>

#include <boost/pfr.hpp>

#include <avnd/common/tag.hpp>
#include <avnd/concepts/message_bus.hpp>

namespace oscr
{

struct Serializer
{
  DataStreamReader& r;

  template <typename F>
  requires std::is_aggregate_v<F>
  void operator()(const F& f) { boost::pfr::for_each_field(f, *this); }

  template <typename F>
  requires(std::is_arithmetic_v<F>) void operator()(const F& f)
  {
    r.stream().stream << f;
  }

  template <typename... Args>
  void operator()(const std::variant<Args...>& f)
  {
    r.stream() << (int)f.index();
    std::visit([&](const auto& arg) { r.stream() << arg; }, f);
  }

  void operator()(const auto& f) { r.stream() << f; }
};

struct MessageBusSender
{
  std::function<void(QByteArray)>& bus;

  template <typename T>
  requires std::is_trivial_v<T>
  void operator()(const T& msg)
  {
    // Here we can just do a memcpy
    this->bus(QByteArray((const char*)&msg, sizeof(msg)));
  }

  template <typename T>
    requires(!std::is_trivial_v<T> && avnd::relocatable<T>)
  void operator()(const T& msg)
  {
    QByteArray b(msg.size(), Qt::Uninitialized);
    auto dst = reinterpret_cast<T*>(b.data());
    new(dst) T(msg);

    this->bus(std::move(b));
  }

  template <typename T>
    requires(!std::is_trivial_v<T> && avnd::relocatable<T>)
  void operator()(T&& msg)
  {
    QByteArray b(sizeof(msg), Qt::Uninitialized);
    auto dst = reinterpret_cast<T*>(b.data());
    std::construct_at(dst, std::move(msg));

    this->bus(std::move(b));
  }

  template <typename T>
    requires(!std::is_trivial_v<T> && !avnd::relocatable<T>)
  void operator()(const T& msg)
  {
    // Here we gotta serialize... :D
    QByteArray buf;

    DataStreamReader str{&buf};
    Serializer{str}(msg);

    this->bus(std::move(buf));
  }

  template <typename T>
  void operator()(const std::shared_ptr<T>& msg)
  {
    SCORE_ASSERT(msg);
    return (*this)(std::move(*msg));
  }
  template <typename T>
  void operator()(std::unique_ptr<T> msg)
  {
    SCORE_ASSERT(msg);
    return (*this)(std::move(*msg));
  }
};

struct Deserializer
{
  DataStreamWriter& r;

  template <typename F>
  requires std::is_aggregate_v<F>
  void operator()(F& f) { boost::pfr::for_each_field(f, *this); }

  template <typename F>
  requires(std::is_arithmetic_v<F>) void operator()(F& f) { r.stream().stream >> f; }

  template <std::size_t I, typename... Args>
  bool write_variant(std::variant<Args...>& f)
  {
    auto& elt = f.template emplace<I>();
    r.stream() >> elt;
    return true;
  }

  template <typename... Args>
  void operator()(std::variant<Args...>& f)
  {
    int index{};
    r.stream() >> index;

    [&]<std::size_t... I>(std::index_sequence<I...>)
    {
      (((index == I) && write_variant<I>(f)) || ...);
    }
    (std::make_index_sequence<sizeof...(Args)>{});
  }

  void operator()(auto& f) { r.stream() >> f; }
};

struct MessageBusReader
{
  QByteArray& mess;

  template <typename T>
  requires std::is_trivial_v<T>
  void operator()(T& msg)
  {
    // Here we can just do a memcpy
    memcpy(&msg, mess.data(), mess.size());
  }
  template <typename T>
    requires(!std::is_trivial_v<T> && avnd::relocatable<T>)
  void operator()(T& msg)
  {
    auto src = reinterpret_cast<T*>(mess.data());
    msg = std::move(*src);
    std::destroy_at(src);
  }

  template <typename T>
    requires(!std::is_trivial_v<T> && !avnd::relocatable<T>)
  void operator()(T& msg)
  {
    // Deserialize... :D

    DataStreamWriter str{mess};
    Deserializer{str}(msg);
  }
};

}
