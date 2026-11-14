#pragma once
#include <LV2/Context.hpp>

#include <readerwriterqueue.h>

#include <QDebug>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <deque>
#include <optional>
#include <utility>
#include <vector>

namespace LV2
{

//! Messages of any size from one thread to another, neither of which
//! allocates nor frees once the ring is built: a message is a header (its port,
//! protocol and size) followed by its bytes, laid over consecutive packets, as
//! jalv lays a size-prefixed message in its zix ring.
class message_ring
{
public:
  struct header
  {
    uint32_t index{};
    uint32_t protocol{};
    uint32_t size{};
  };
  static constexpr std::size_t packet_size = 64;

  //! [main thread] Room for `bytes` of messages and their headers
  explicit message_ring(std::size_t bytes)
      : m_queue{std::max<std::size_t>(1, (bytes + packet_size - 1) / packet_size)}
      , m_capacity{m_queue.max_capacity()}
  {
  }

  message_ring(const message_ring&) = delete;
  message_ring& operator=(const message_ring&) = delete;

  static constexpr std::size_t packets_for(uint32_t size) noexcept
  {
    return (sizeof(header) + std::size_t(size) + packet_size - 1) / packet_size;
  }

  //! The largest message the ring holds
  std::size_t max_message_size() const noexcept
  {
    return m_capacity * packet_size - sizeof(header);
  }

  //! [producer] All of the message, or nothing when the ring has no room for it
  bool try_write(
      uint32_t index, uint32_t protocol, const void* data, uint32_t size) noexcept
  {
    const std::size_t n = packets_for(size);
    // The producer never sees fewer packets than the consumer left: the room
    // it computes is at most the actual one, and every try_enqueue succeeds.
    const std::size_t used = m_queue.size_approx();
    if(used > m_capacity || n > m_capacity - used)
      return false;

    packet p;
    const header h{index, protocol, size};
    std::memcpy(p.bytes, &h, sizeof(h));
    std::size_t at = sizeof(h);
    auto* src = static_cast<const uint8_t*>(data);
    std::size_t left = size;
    for(;;)
    {
      const std::size_t k = std::min(left, packet_size - at);
      if(k > 0)
        std::memcpy(p.bytes + at, src, k);
      src += k;
      left -= k;
      [[maybe_unused]] const bool ok = m_queue.try_enqueue(p);
      assert(ok);
      if(left == 0)
        return true;
      at = 0;
    }
  }

  //! [consumer] The next message, once all its packets are in.
  //! `dest(header)` tells where its bytes go: std::nullopt leaves it for a
  //! later call, nullptr drops it. False when nothing was taken.
  template <typename F>
  bool try_read(F&& dest) noexcept
  {
    const packet* p = m_queue.peek();
    if(!p)
      return false;
    header h;
    std::memcpy(&h, p->bytes, sizeof(h));
    const std::size_t n = packets_for(h.size);
    // The consumer never sees more packets than the producer wrote
    if(m_queue.size_approx() < n)
      return false;

    const std::optional<uint8_t*> where = dest(std::as_const(h));
    if(!where)
      return false;
    uint8_t* out = *where;

    std::size_t at = sizeof(h);
    std::size_t left = h.size;
    for(std::size_t i = 0; i < n; i++)
    {
      const std::size_t k = std::min(left, packet_size - at);
      if(out && k > 0)
      {
        std::memcpy(out, p->bytes + at, k);
        out += k;
      }
      left -= k;
      m_queue.pop();
      if(i + 1 < n)
        p = m_queue.peek();
      at = 0;
    }
    return true;
  }

private:
  struct alignas(8) packet
  {
    uint8_t bytes[packet_size];
  };
  // ossia::spsc_queue is a ConcurrentQueue in TSAN builds, which has no peek.
  // With a block size no capacity reaches, the queue is one block, whose
  // size_approx() and max_capacity() hold with no slot set apart.
  moodycamel::ReaderWriterQueue<packet, (std::size_t(1) << 30)> m_queue;
  std::size_t m_capacity{};
};

//! What a plug-in and its UI say to each other, from the main thread to the
//! audio thread and back, in storage allocated up front for the largest
//! sequence of its atom ports: rings of 16 such sequences, as jalv's.
class UiChannel
{
public:
  static constexpr std::size_t sequences_in_ring = 16;

  //! [main thread]
  explicit UiChannel(const LV2Data& data)
      : to_plugin{sequences_in_ring * largest(data, data.midi_in_ports, data.atom_in_ports)}
      , to_ui{sequences_in_ring * largest(data, data.midi_out_ports, data.atom_out_ports)}
      , m_eventTransfer{data.host.atom_eventTransfer}
      , m_forUi(largest(data, data.midi_out_ports, data.atom_out_ports))
  {
    for(int port : data.midi_in_ports)
      m_inputs.emplace_back(port, data.atomBufferSize(port));
    for(int port : data.atom_in_ports)
      m_inputs.emplace_back(port, data.atomBufferSize(port));
  }

  //! The bytes of an input sequence that a message of `size` bytes, a whole
  //! atom, takes: an event, padded.
  static constexpr uint32_t event_size(uint32_t size) noexcept
  {
    return sizeof(int64_t) + ((size + 7u) & ~7u);
  }

  //! What a sequence of `capacity` bytes has room for, in events
  static constexpr uint32_t sequence_room(uint32_t capacity) noexcept
  {
    return capacity > sizeof(LV2_Atom_Sequence) ? capacity - sizeof(LV2_Atom_Sequence)
                                                : 0;
  }

  //! main thread -> audio thread: atoms for the atom inputs, MIDI or not
  message_ring to_plugin;
  //! audio thread -> main thread: the events of the atom outputs, while a UI
  //! listens
  message_ring to_ui;
  //! A UI is open and reads to_ui
  std::atomic_bool listening{false};
  //! What the plug-in said for its UI and to_ui had no room for
  std::atomic<uint32_t> dropped_for_ui{0};

  //! [main thread] An atom the UI writes to an atom input, as a whole atom
  //! of `size` bytes. One the input can never hold is dropped, as jalv drops
  //! what it cannot pass on; one the ring has no room for yet waits on the
  //! main thread, in order, for the plug-in to take those before it.
  void send_to_plugin(Message&& msg)
  {
    const auto it = std::find_if(m_inputs.begin(), m_inputs.end(), [&](const auto& in) {
      return uint32_t(in.first) == msg.index;
    });
    if(it == m_inputs.end() || msg.protocol != m_eventTransfer)
      return;
    if(msg.body.size() < sizeof(LV2_Atom)
       || sizeof(LV2_Atom) + reinterpret_cast<const LV2_Atom*>(msg.body.data())->size
              != msg.body.size())
    {
      qDebug() << "LV2: the UI wrote a malformed atom to port" << msg.index;
      return;
    }
    if(event_size(msg.body.size()) > sequence_room(it->second)
       || msg.body.size() > to_plugin.max_message_size())
    {
      qWarning() << "LV2: a message of" << msg.body.size() << "bytes from the UI to port"
                 << msg.index << "does not fit its" << it->second
                 << "byte buffer: dropped";
      return;
    }

    flush();
    if(!m_waiting.empty() || !write(msg))
      m_waiting.push_back(std::move(msg));
  }

  //! [main thread] Passes on what waits for room in to_plugin.
  //! True when nothing waits any more.
  bool flush()
  {
    while(!m_waiting.empty() && write(m_waiting.front()))
      m_waiting.pop_front();
    return m_waiting.empty();
  }

  //! [main thread] f(port index, protocol, size, bytes) for each message of
  //! the plug-in for its UI
  template <typename F>
  void read_for_ui(F&& f)
  {
    message_ring::header h;
    while(to_ui.try_read([&](const message_ring::header& hd) -> std::optional<uint8_t*> {
      h = hd;
      return hd.size <= m_forUi.size() ? m_forUi.data() : nullptr;
    }))
    {
      if(h.size <= m_forUi.size())
        f(h.index, h.protocol, h.size, (const void*)m_forUi.data());
    }
  }

  //! [main thread] Drops what the plug-in said for a UI that closed
  void clear_for_ui()
  {
    while(to_ui.try_read([](const message_ring::header&) -> std::optional<uint8_t*> {
      return nullptr;
    }))
    {
    }
  }

  //! [audio thread] What the plug-in writes on an atom output, for the UI
  void send_to_ui(uint32_t port_index, const LV2_Atom& atom) noexcept
  {
    if(!to_ui.try_write(
           port_index, m_eventTransfer, &atom, uint32_t(sizeof(LV2_Atom) + atom.size)))
      dropped_for_ui.fetch_add(1, std::memory_order_relaxed);
  }

private:
  bool write(const Message& msg) noexcept
  {
    return to_plugin.try_write(
        msg.index, msg.protocol, msg.body.data(), uint32_t(msg.body.size()));
  }

  template <typename... Ports>
  static std::size_t largest(const LV2Data& data, const Ports&... ports)
  {
    std::size_t res = 0;
    (
        [&] {
      for(int port : ports)
        res = std::max<std::size_t>(res, data.atomBufferSize(port));
    }(),
        ...);
    return res;
  }

  //! The atom inputs, MIDI or not, and the bytes of their buffer
  std::vector<std::pair<int, uint32_t>> m_inputs;
  uint32_t m_eventTransfer{};
  std::deque<Message> m_waiting;
  std::vector<uint8_t> m_forUi;
};

}
