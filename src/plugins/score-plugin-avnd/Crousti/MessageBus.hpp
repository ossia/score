#pragma once
#include <score/serialization/DataStreamVisitor.hpp>

#include <boost/pfr.hpp>

#include <avnd/common/tag.hpp>
#include <avnd/concepts/message_bus.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <functional>

namespace oscr
{
//! The argument type of a std::function<void(Arg)>.
template <typename F>
struct function_argument;
template <typename R, typename Arg>
struct function_argument<std::function<R(Arg)>>
{
  using type = Arg;
};

/**
 * Whether the reader of a processor to UI channel has a wake-up pending.
 *
 * The writer calls request() after each write and wakes the reader up (with
 * an execution command) only when it returns true; when the wake-up cannot be
 * sent it calls cancel(), and what it wrote goes with the next wake-up. The
 * reader calls clear() before it reads, so that a write that it misses wakes
 * it up again.
 */
struct drain_request
{
  bool request() noexcept { return !m_pending.exchange(true, std::memory_order_acq_rel); }
  void cancel() noexcept { m_pending.store(false, std::memory_order_release); }
  void clear() noexcept { m_pending.store(false, std::memory_order_release); }

private:
  std::atomic_bool m_pending{false};
};

/**
 * Processor to UI messages too large to travel inside an execution command:
 * a fixed ring with one writing thread (the processor's) and one reading
 * thread (the UI). Nothing allocates once it is built; the reader moves the
 * messages out, so what they own is freed on the UI thread.
 *
 * When the ring is full, push() drops the message and returns false: the UI
 * misses that update and catches up with the next message that fits.
 */
template <typename T, std::size_t N = 16>
struct message_ring : drain_request
{
  static_assert(N > 0 && (N & (N - 1)) == 0);

  bool push(T&& msg) noexcept(std::is_nothrow_move_assignable_v<T>)
  {
    const auto w = m_write.load(std::memory_order_relaxed);
    if(w - m_read.load(std::memory_order_acquire) >= N)
    {
      m_dropped.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    m_slots[w % N] = std::move(msg);
    m_write.store(w + 1, std::memory_order_release);
    return true;
  }

  //! UI thread: calls f with every message written since the last call.
  template <typename F>
  int drain(F&& f)
  {
    this->clear();
    auto r = m_read.load(std::memory_order_relaxed);
    const auto w = m_write.load(std::memory_order_acquire);
    int n = 0;
    for(; r != w; ++r, ++n)
    {
      T msg = std::move(m_slots[r % N]);
      m_read.store(r + 1, std::memory_order_release);
      f(std::move(msg));
    }
    return n;
  }

  std::size_t dropped() const noexcept
  {
    return m_dropped.load(std::memory_order_relaxed);
  }

private:
  std::array<T, N> m_slots{};
  std::atomic<std::size_t> m_write{0};
  std::atomic<std::size_t> m_read{0};
  std::atomic<std::size_t> m_dropped{0};
};

/**
 * The latest of a series of values, from one writing thread to one reading
 * thread, without allocation or lock: a triple buffer. Values the reader has
 * not taken yet are replaced by newer ones.
 *
 * The writer fills back() then publish()es it; the reader take()s the latest
 * published value. Slots are built on the thread that builds the mailbox:
 * give a string slot its capacity there, and write at most that much.
 */
template <typename T>
struct latest_value : drain_request
{
  template <typename F>
  void for_each_slot(F&& f)
  {
    for(auto& s : m_slots)
      f(s);
  }

  T& back() noexcept { return m_slots[m_back]; }

  void publish() noexcept
  {
    m_back = m_middle.exchange(m_back | fresh, std::memory_order_acq_rel) & index_mask;
  }

  //! The latest published value, or nullptr when none was published since the
  //! last call. It stays valid until the next call.
  T* take() noexcept
  {
    this->clear();
    if(!(m_middle.load(std::memory_order_acquire) & fresh))
      return nullptr;
    m_front = m_middle.exchange(m_front, std::memory_order_acq_rel) & index_mask;
    return &m_slots[m_front];
  }

private:
  static constexpr int fresh = 4;
  static constexpr int index_mask = 3;
  std::array<T, 3> m_slots{};
  std::atomic<int> m_middle{1};
  int m_back{0};
  int m_front{2};
};

struct Serializer
{
  DataStreamReader& r;

  template <typename F>
  void operator()(const F& f)
  {
    if constexpr(std::is_arithmetic_v<F>)
      r.stream().stream << f;
    else if constexpr(std::is_aggregate_v<F>)
      avnd::pfr::for_each_field(f, *this);
    else if constexpr(avnd::list_ish<F>)
    {
      r.stream().stream << (int64_t)std::ssize(f);
      for(const auto& val : f)
      {
        (*this)(val);
      }
    }
    else
      r.stream() << f;
  }

  template <typename... Args>
  void operator()(const std::variant<Args...>& f)
  {
    r.stream().stream << (int)f.index();
    std::visit(*this, f);
  }
};

struct MessageBusSender
{
  std::function<void(QByteArray)>& bus;

  // Case for zero-argument message bus (e.g. just a ping)
  void operator()() { this->bus(QByteArray{}); }

  template <typename T>
    requires std::is_trivial_v<T>
  void operator()(const T& msg)
  {
    // Here we can just do a memcpy
    this->bus(QByteArray((const char*)&msg, sizeof(msg)));
  }

  template <typename T>
    requires(
        !std::is_trivial_v<T> && avnd::relocatable<T>
        && alignof(T) <= alignof(max_align_t))
  void operator()(const T& msg)
  {
    QByteArray b(msg.size(), Qt::Uninitialized);
    auto dst = reinterpret_cast<T*>(b.data());
    new(dst) T(msg);

    this->bus(std::move(b));
  }

  template <typename T>
    requires(
        !std::is_trivial_v<T> && avnd::relocatable<T>
        && alignof(T) <= alignof(max_align_t))
  void operator()(T&& msg)
  {
    QByteArray b(sizeof(msg), Qt::Uninitialized);
    auto dst = reinterpret_cast<T*>(b.data());
    std::construct_at(dst, std::move(msg));

    this->bus(std::move(b));
  }

  template <typename T>
    requires(
        !std::is_trivial_v<T>
        && (!avnd::relocatable<T> || (alignof(T) > alignof(max_align_t))))
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
  void operator()(F& f)
  {
    avnd::pfr::for_each_field(f, *this);
  }

  template <typename F>
    requires(std::is_arithmetic_v<F>)
  void operator()(F& f)
  {
    r.stream().stream >> f;
  }

  template <typename F>
    requires avnd::list_ish<F>
  void operator()(F& f)
  {
    int64_t sz;
    r.stream().stream >> sz;
    SCORE_ASSERT(sz >= 0);
    for(int64_t i = 0; i < sz; i++)
    {
      using type = typename F::value_type;
      type val;
      (*this)(val);
      f.push_back(std::move(val));
    }
  }

  template <std::size_t I, typename... Args>
  bool write_variant(std::variant<Args...>& f)
  {
    auto& elt = f.template emplace<I>();
    (*this)(elt);
    return true;
  }

  template <typename... Args>
  void operator()(std::variant<Args...>& f)
  {
    int index{};
    r.stream().stream >> index;

    SCORE_ASSERT(index >= 0);
    SCORE_ASSERT(index < sizeof...(Args));
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

  // Case for zero-argument message bus (e.g. just a ping)
  void operator()() { }

  template <typename T>
  requires std::is_trivial_v<T>
  void operator()(T& msg)
  {
    // Here we can just do a memcpy
    memcpy(&msg, mess.data(), mess.size());
  }
  template <typename T>
    requires(
        !std::is_trivial_v<T> && avnd::relocatable<T>
        && alignof(T) <= alignof(max_align_t))
  void operator()(T& msg)
  {
    auto src = reinterpret_cast<T*>(mess.data());
    msg = std::move(*src);
    std::destroy_at(src);
  }

  template <typename T>
    requires(
        !std::is_trivial_v<T>
        && (!avnd::relocatable<T> || (alignof(T) > alignof(max_align_t))))
  void operator()(T& msg)
  {
    // Deserialize... :D

    DataStreamWriter str{mess};
    Deserializer{str}(msg);
  }
};

}
