#pragma once
#include <libremidi/detail/conversion.hpp>
#include <libremidi/ump.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace Process
{
/**
 * Whole system-exclusive messages for a plug-in host to hand a plug-in.
 *
 * A MIDI port carries a system-exclusive message as the SysEx7 packets it is
 * made of. A VST, VST3 or CLAP plug-in takes it as one event that points at
 * the whole F0 ... F7 message, and may read it until its process call
 * returns. feed() joins the packets of one input port (one `stream` per
 * port, which keeps a message whose packets span two ticks) and copies each
 * whole message into storage the node owns: nothing is allocated on the
 * audio thread. The host calls clear() once per process call, before it makes
 * the events of the next one.
 *
 * Between two clear(), up to `capacity` bytes of messages are held, each of
 * up to `max_message` bytes, F0 and F7 included. A message past either limit
 * is not delivered and dropped() counts it.
 */
class MidiSysexArena
{
public:
  static constexpr std::size_t max_message = 4096;
  static constexpr std::size_t capacity = 16384;
  using stream = libremidi::ump_to_midi1_stream<max_message>;

  static bool isSysex(const libremidi::ump& packet) noexcept
  {
    return packet.get_type() == libremidi::midi2::message_type::SYSEX7;
  }

  //! The whole message `packet` ends, held until clear(). Empty when it ends
  //! none, or when it is not a SysEx7 packet.
  std::span<const uint8_t> feed(stream& s, const libremidi::ump& packet) noexcept
  {
    if(!isSysex(packet))
      return {};

    const auto dropped_before = s.dropped();
    const auto message = s.feed(packet);
    m_dropped += s.dropped() - dropped_before;
    if(message.empty())
      return {};

    if(message.size() > capacity - m_used)
    {
      ++m_dropped;
      return {};
    }

    uint8_t* const dst = m_bytes.data() + m_used;
    std::copy(message.begin(), message.end(), dst);
    m_used += message.size();
    return {dst, message.size()};
  }

  void clear() noexcept { m_used = 0; }

  std::size_t dropped() const noexcept { return m_dropped; }

private:
  std::array<uint8_t, capacity> m_bytes;
  std::size_t m_used{};
  std::size_t m_dropped{};
};
}
