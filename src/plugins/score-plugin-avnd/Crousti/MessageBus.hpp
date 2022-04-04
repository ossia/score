#pragma once
#include <boost/pfr.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

namespace oscr
{

struct Serializer
{
    DataStreamReader& r;

    template<typename F>
    requires std::is_aggregate_v<F>
    void operator()(const F& f) {
      boost::pfr::for_each_field(f, *this);
    }

    template<typename F>
    requires (std::is_arithmetic_v<F>)
    void operator()(F f) { r.stream().stream << f; }

    template<typename... Args>
    void operator()(const std::variant<Args...>& f) {
      r.stream() << (std::size_t)f.index();
      std::visit([&] (const auto &arg) { r.stream() << arg; }, f);
    }

    void operator()(const auto& f) {
      r.stream() << f;
    }
};

struct MessageBusSender
{
    std::function<void(QByteArray)>& bus;

    template <typename T>
    requires std::is_trivial_v<T>
    void operator()(const T& msg) {
      // Here we can just do a memcpy
      this->bus(QByteArray((const char*)&msg, sizeof(msg)));
    }

    template <typename T>
    requires (!std::is_trivial_v<T>)
    void operator()(const T& msg) {
      // Here we gotta serialize... :D
      QByteArray buf;

      DataStreamReader str{&buf};
      Serializer{str}(msg);

      this->bus(buf);
    }
};



struct Deserializer
{
    DataStreamWriter& r;

    template<typename F>
    requires std::is_aggregate_v<F>
    void operator()(F& f) {
        boost::pfr::for_each_field(f, *this);
    }

    template<typename F>
    requires (std::is_arithmetic_v<F>)
    void operator()(F& f) { r.stream().stream >> f; }

    template<std::size_t I, typename... Args>
    bool write_variant(std::variant<Args...>& f) {
      auto& elt = f.template emplace<I>();
      r.stream() >> elt;
      return true;
    }

    template<typename... Args>
    void operator()(std::variant<Args...>& f) {
      std::size_t index{};
      r.stream() >> index;

      [&] <std::size_t... I> (std::index_sequence<I...>) {
          (((index == I) && write_variant<I>(f)) || ...);
      }(std::make_index_sequence<sizeof...(Args)>{});
    }

    void operator()(auto& f) {
      r.stream() >> f;
    }
};

struct MessageBusReader
{
  const QByteArray& mess;

  template <typename T>
  requires std::is_trivial_v<T>
  void operator()(T& msg) {
    // Here we can just do a memcpy
    memcpy(&msg, mess.data(), mess.size());
  }

  template <typename T>
  requires (!std::is_trivial_v<T>)
  void operator()(T& msg) {
    // Deserialize... :D

    DataStreamWriter str{mess};
    Deserializer{str}(msg);
  }
};


}
