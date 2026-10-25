// execution_state and the MIDI devices that go away under it.
//
// The execution unregisters a port from its own queue, on the audio thread:
// by the time it does, the device the port's address points into may be
// destroyed - a document closes its devices right after its execution. And a
// device removed while playing is destroyed right after it is unregistered,
// before the next tick. Neither may read the device again.
//
// The devices and protocols here live in pages that are made inaccessible
// once destroyed, so that reading them faults instead of reading what the
// allocator happened to leave there.

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/protocols/midi/midi_stream.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <memory>
#include <new>

#if __has_include(<sys/mman.h>)
#include <sys/mman.h>
#define STALE_MIDI_TEST 1

namespace
{
//! Large enough for a device or a protocol
constexpr std::size_t page_block = 1 << 16;

void* poisonable_alloc()
{
  void* p = mmap(
      nullptr, page_block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if(p == MAP_FAILED)
    throw std::bad_alloc{};
  return p;
}

//! Kept mapped, never readable again: a use after free faults.
void poison(void* p) noexcept
{
  mprotect(p, page_block, PROT_NONE);
}

struct FakeMidiProtocol final
    : ossia::net::protocol_base
    , ossia::net::midi::midi_stream
{
  FakeMidiProtocol()
      : protocol_base{flags{}}
  {
  }

  static void* operator new(std::size_t n)
  {
    REQUIRE(n <= page_block);
    return poisonable_alloc();
  }
  static void operator delete(void* p) noexcept { poison(p); }

  bool pull(ossia::net::parameter_base&) override { return false; }
  bool push(const ossia::net::parameter_base&, const ossia::value&) override
  {
    return false;
  }
  bool push_raw(const ossia::net::full_parameter_data&) override { return false; }
  bool observe(ossia::net::parameter_base&, bool) override { return false; }
  bool update(ossia::net::node_base&) override { return false; }

  libremidi::midi_in* midi_in() const noexcept override { return nullptr; }
  void enable_registration() override { }
  void push_value(const libremidi::ump&) override { }
  void push_value(const libremidi::message&) override { }
};

//! A device with a MIDI stream, whose root carries the parameter ports bind to.
struct PoisonableDevice
{
  static_assert(sizeof(ossia::net::generic_device) <= page_block);
  void* memory = poisonable_alloc();
  ossia::net::generic_device* dev = new(memory) ossia::net::generic_device{
      std::make_unique<FakeMidiProtocol>(), "midi"};
  ossia::net::parameter_base* param
      = dev->get_root_node().create_parameter(ossia::val_type::INT);

  void destroy()
  {
    dev->~generic_device();
    poison(memory);
    dev = nullptr;
    param = nullptr;
  }
};
}
#endif

TEST_CASE(
    "Unregistering a MIDI inlet does not read the device it was bound to",
    "[execution][devices][midi]")
{
#if defined(STALE_MIDI_TEST)
  PoisonableDevice d;
  REQUIRE(d.param);

  ossia::execution_state st;
  st.register_device(d.dev);
  st.apply_device_changes();

  auto in = std::make_unique<ossia::midi_inlet>();
  in->address = d.param;
  st.register_port(*in);
  st.begin_tick();

  // The document closes its devices: the execution's queued unregistration
  // runs afterwards, with the address still set.
  st.clear_devices();
  d.destroy();

  st.unregister_port(*in);
  SUCCEED();
#else
  SKIP("needs mprotect");
#endif
}

TEST_CASE(
    "A removed MIDI device's stream is not read on the next tick",
    "[execution][devices][midi]")
{
#if defined(STALE_MIDI_TEST)
  PoisonableDevice d;
  REQUIRE(d.param);

  ossia::execution_state st;
  st.register_device(d.dev);
  st.apply_device_changes();

  auto in = std::make_unique<ossia::midi_inlet>();
  in->address = d.param;
  st.register_port(*in);
  st.begin_tick();

  // What Execution::DocumentPlugin does on a device removal while playing:
  // unregister it while it is alive, reset the ports bound to it without
  // unregistering them, and the device is destroyed before the next tick.
  st.unregister_device(d.dev);
  in->address = {};
  d.destroy();

  st.begin_tick();
  st.begin_tick();
  st.unregister_port(*in);
  SUCCEED();
#else
  SKIP("needs mprotect");
#endif
}
