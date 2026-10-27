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
#include <ossia/dataflow/graph/graph_utils.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/common/path.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/protocols/midi/midi_stream.hpp>

#include <libremidi/libremidi.hpp>

#include <catch2/catch_test_macros.hpp>

#include <charconv>
#include <cstddef>
#include <memory>
#include <new>
#include <set>
#include <string>

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

  // get_new_values() only drains a stream that has an input to time it against.
  libremidi::midi_in* midi_in() const noexcept override { return input.get(); }
  void enable_registration() override { }
  void push_value(const libremidi::ump&) override { }
  void push_value(const libremidi::message&) override { }

  //! Children of the root named "1" to "16" stand for that channel.
  std::optional<int> stream_channel(const ossia::net::node_base& n) const noexcept override
  {
    if(!n.get_parent() || n.get_parent()->get_parent())
      return std::nullopt;
    const auto& name = n.get_name();
    int ch{};
    auto [ptr, ec] = std::from_chars(name.data(), name.data() + name.size(), ch);
    if(ec != std::errc{} || ptr != name.data() + name.size() || ch < 1 || ch > 16)
      return std::nullopt;
    return ch;
  }

  std::unique_ptr<libremidi::midi_in> input = std::make_unique<libremidi::midi_in>(
      libremidi::input_configuration{
          .on_message = [](auto&&...) { }, .on_warning = [](auto&&...) { }},
      libremidi::dummy_configuration{});
};

/**
 * A device with a MIDI stream. Its root carries a parameter, channel 1 is a
 * node without one, and that node's child "on" has one.
 */
struct PoisonableDevice
{
  static_assert(sizeof(ossia::net::generic_device) <= page_block);
  explicit PoisonableDevice(std::string name = "midi")
      : dev{new(memory) ossia::net::generic_device{
          std::make_unique<FakeMidiProtocol>(), std::move(name)}}
  {
  }

  void* memory = poisonable_alloc();
  ossia::net::generic_device* dev{};
  FakeMidiProtocol* proto = &static_cast<FakeMidiProtocol&>(dev->get_protocol());
  ossia::net::parameter_base* param
      = dev->get_root_node().create_parameter(ossia::val_type::INT);
  ossia::net::node_base* channel1 = dev->get_root_node().create_child("1");
  ossia::net::node_base* channel2 = dev->get_root_node().create_child("2");
  ossia::net::parameter_base* note
      = channel1->create_child("on")->create_parameter(ossia::val_type::INT);

  //! A note on, told apart from the others by its number.
  void send(int note_number, int channel_0_15 = 0)
  {
    REQUIRE(proto->receive_ump(libremidi::ump{
        0x20900000u | uint32_t(channel_0_15) << 16 | uint32_t(note_number) << 8 | 100u}));
  }

  //! What the execution left in the stream: nonzero when it does not collect it.
  std::size_t pending() const { return proto->messages.size_approx(); }

  void flush()
  {
    libremidi::ump m;
    while(proto->messages.try_dequeue(m))
      ;
  }

  void destroy()
  {
    dev->~generic_device();
    poison(memory);
    dev = nullptr;
    proto = nullptr;
    param = nullptr;
    channel1 = nullptr;
    channel2 = nullptr;
    note = nullptr;
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

// What a MIDI inlet hears for each kind of address it can be bound to, through
// the pull a graph does before running a node, and when a device's stream is
// collected at all: a stream nothing registered is left full after a tick.
#if defined(STALE_MIDI_TEST)
namespace
{
//! The note numbers an inlet pulls from its address on this tick.
std::multiset<int> heard(ossia::execution_state& st, ossia::inlet& in)
{
  auto& port = *in.target<ossia::midi_port>();
  port.messages.clear();
  ossia::graph_util::pull_from_parameter(in, st);
  std::multiset<int> notes;
  for(const auto& m : port.messages)
    notes.insert(int((m.data[0] >> 8) & 0x7F));
  port.messages.clear();
  return notes;
}

//! What score's set_destination does on the execution thread.
void rebind(ossia::execution_state& st, ossia::inlet& in, ossia::destination_t addr)
{
  st.unregister_port(in);
  in.address = std::move(addr);
  st.register_port(in);
}

std::unique_ptr<ossia::midi_inlet> inlet_on(ossia::destination_t addr)
{
  auto in = std::make_unique<ossia::midi_inlet>();
  in->address = std::move(addr);
  return in;
}

ossia::traversal::path pattern(std::string_view p)
{
  auto path = ossia::traversal::make_path(p);
  REQUIRE(path);
  return *path;
}

using notes = std::multiset<int>;
}

TEST_CASE("A MIDI inlet bound to a parameter hears its device", "[execution][midi]")
{
  PoisonableDevice d{"midiA"};
  ossia::execution_state st;
  st.register_device(d.dev);
  st.apply_device_changes();

  for(auto* param : {d.param, d.note})
  {
    auto in = inlet_on(param);
    st.register_port(*in);

    // A parameter hears the whole stream, whatever its channel.
    d.send(60, 0);
    d.send(61, 5);
    st.begin_tick();
    CHECK(heard(st, *in) == notes{60, 61});
    CHECK(d.pending() == 0);

    st.unregister_port(*in);
    d.send(62);
    st.begin_tick();
    CHECK(heard(st, *in).empty());
    CHECK(d.pending() == 1);
    d.flush();
  }
}

TEST_CASE("A MIDI inlet bound to a node hears its device or its channel", "[execution][midi]")
{
  PoisonableDevice d{"midiA"};
  ossia::execution_state st;
  st.register_device(d.dev);
  st.apply_device_changes();

  SECTION("the root: every channel")
  {
    auto in = inlet_on(&d.dev->get_root_node());
    st.register_port(*in);
    d.send(60, 0);
    d.send(61, 1);
    st.begin_tick();
    CHECK(heard(st, *in) == notes{60, 61});

    st.unregister_port(*in);
    d.send(62);
    st.begin_tick();
    CHECK(heard(st, *in).empty());
    CHECK(d.pending() == 1);
  }

  SECTION("a channel without a parameter: that channel only")
  {
    auto in = inlet_on(d.channel2);
    st.register_port(*in);
    d.send(60, 0);
    d.send(61, 1);
    st.begin_tick();
    CHECK(heard(st, *in) == notes{61});
    CHECK(d.pending() == 0);

    st.unregister_port(*in);
    d.send(62, 1);
    st.begin_tick();
    CHECK(heard(st, *in).empty());
    CHECK(d.pending() == 1);
  }
}

TEST_CASE(
    "A MIDI inlet bound to a pattern registers nothing and hears what others collect",
    "[execution][midi]")
{
  PoisonableDevice d{"midiA"};
  ossia::execution_state st;
  st.register_device(d.dev);
  st.apply_device_changes();

  // Matches the channel nodes "1" and "2".
  auto in = inlet_on(pattern("midiA:/[12]"));
  st.register_port(*in);

  d.send(60, 0);
  st.begin_tick();
  CHECK(heard(st, *in).empty());
  CHECK(d.pending() == 1);
  d.flush();

  // Once another inlet has the stream collected, each matched node hears its
  // own channel.
  auto other = inlet_on(d.param);
  st.register_port(*other);
  d.send(60, 0);
  d.send(61, 1);
  d.send(62, 2);
  st.begin_tick();
  CHECK(heard(st, *in) == notes{60, 61});
  CHECK(heard(st, *other) == notes{60, 61, 62});

  // Unregistering the pattern inlet takes nothing from the other one.
  st.unregister_port(*in);
  d.send(63);
  st.begin_tick();
  CHECK(heard(st, *other) == notes{63});

  st.unregister_port(*other);
  d.send(64);
  st.begin_tick();
  CHECK(d.pending() == 1);
}

TEST_CASE(
    "A MIDI inlet without an address or on a device without MIDI registers nothing",
    "[execution][midi]")
{
  PoisonableDevice d{"midiA"};
  ossia::net::generic_device plain{"plain"};
  auto* plain_param = plain.get_root_node().create_parameter(ossia::val_type::INT);

  ossia::execution_state st;
  st.register_device(d.dev);
  st.register_device(&plain);
  st.apply_device_changes();

  auto used = inlet_on(d.param);
  st.register_port(*used);

  for(ossia::destination_t addr :
      {ossia::destination_t{}, ossia::destination_t{plain_param},
       ossia::destination_t{&plain.get_root_node()}})
  {
    auto in = inlet_on(addr);
    st.register_port(*in);
    d.send(60);
    st.begin_tick();
    CHECK(heard(st, *in).empty());
    CHECK(heard(st, *used) == notes{60});

    // Unregistering it does not take the stream from the inlet that has it.
    st.unregister_port(*in);
    d.send(61);
    st.begin_tick();
    CHECK(heard(st, *used) == notes{61});
  }

  st.unregister_port(*used);
  d.send(62);
  st.begin_tick();
  CHECK(d.pending() == 1);
}

TEST_CASE("Rebinding a MIDI inlet moves it from device to device", "[execution][midi]")
{
  PoisonableDevice a{"midiA"};
  PoisonableDevice b{"midiB"};
  ossia::execution_state st;
  st.register_device(a.dev);
  st.register_device(b.dev);
  st.apply_device_changes();

  auto in = inlet_on({});
  st.register_port(*in);

  // Whether each device's MIDI reaches the inlet, and whether it is collected.
  const auto check = [&](bool from_a, bool from_b) {
    a.send(10);
    b.send(20);
    st.begin_tick();
    notes expected;
    if(from_a)
      expected.insert(10);
    if(from_b)
      expected.insert(20);
    CHECK(heard(st, *in) == expected);
    CHECK(a.pending() == (from_a ? 0 : 1));
    CHECK(b.pending() == (from_b ? 0 : 1));
    a.flush();
    b.flush();
  };

  rebind(st, *in, a.note);
  check(true, false);
  rebind(st, *in, b.param);
  check(false, true);
  rebind(st, *in, &a.dev->get_root_node());
  check(true, false);
  rebind(st, *in, {});
  check(false, false);
  rebind(st, *in, a.note);
  check(true, false);

  // An inlet on the other device keeps it collected through all of the above.
  auto keep = inlet_on(b.param);
  st.register_port(*keep);
  rebind(st, *in, b.note);
  rebind(st, *in, a.param);
  a.send(10);
  b.send(20);
  st.begin_tick();
  CHECK(heard(st, *in) == notes{10});
  CHECK(heard(st, *keep) == notes{20});

  st.unregister_port(*in);
  st.unregister_port(*keep);
  check(false, false);
}

TEST_CASE("Two MIDI inlets on one device each hold its stream", "[execution][midi]")
{
  PoisonableDevice d{"midiA"};
  ossia::execution_state st;
  st.register_device(d.dev);
  st.apply_device_changes();

  auto in1 = inlet_on(d.param);
  auto in2 = inlet_on(d.channel1);
  st.register_port(*in1);
  st.register_port(*in2);

  d.send(60);
  st.begin_tick();
  CHECK(heard(st, *in1) == notes{60});
  CHECK(heard(st, *in2) == notes{60});

  st.unregister_port(*in1);
  d.send(61);
  st.begin_tick();
  CHECK(heard(st, *in2) == notes{61});

  st.unregister_port(*in2);
  d.send(62);
  st.begin_tick();
  CHECK(d.pending() == 1);
}

TEST_CASE("Registering a MIDI inlet twice counts it once", "[execution][midi]")
{
  PoisonableDevice d{"midiA"};
  ossia::execution_state st;
  st.register_device(d.dev);
  st.apply_device_changes();

  auto in1 = inlet_on(d.param);
  auto in2 = inlet_on(d.note);
  st.register_port(*in1);
  st.register_port(*in1);
  st.register_port(*in2);

  st.unregister_port(*in1);
  d.send(60);
  st.begin_tick();
  CHECK(heard(st, *in2) == notes{60});

  st.unregister_port(*in2);
  d.send(61);
  st.begin_tick();
  CHECK(d.pending() == 1);
}

TEST_CASE(
    "MIDI inlets left on a removed device hear the device they are rebound to",
    "[execution][devices][midi]")
{
  PoisonableDevice a{"midiA"};
  ossia::execution_state st;
  st.register_device(a.dev);
  st.apply_device_changes();

  auto in1 = inlet_on(a.param);
  auto in2 = inlet_on(a.channel1);
  st.register_port(*in1);
  st.register_port(*in2);
  a.send(60);
  st.begin_tick();
  CHECK(heard(st, *in1) == notes{60});

  // Execution::DocumentPlugin's removal: the ports are reset without being
  // unregistered, the device is destroyed before the next tick.
  st.unregister_device(a.dev);
  in1->address = {};
  in2->address = {};
  a.destroy();
  st.begin_tick();
  st.begin_tick();

  PoisonableDevice c{"midiA"};
  st.register_device(c.dev);
  st.begin_tick();
  rebind(st, *in1, c.param);
  rebind(st, *in2, c.channel1);

  c.send(70);
  st.begin_tick();
  CHECK(heard(st, *in1) == notes{70});
  CHECK(heard(st, *in2) == notes{70});

  st.unregister_port(*in1);
  c.send(71);
  st.begin_tick();
  CHECK(heard(st, *in2) == notes{71});

  st.unregister_port(*in2);
  c.send(72);
  st.begin_tick();
  CHECK(c.pending() == 1);
}

TEST_CASE("MIDI inlets rebind after the execution state is reset", "[execution][midi]")
{
  PoisonableDevice d{"midiA"};
  ossia::execution_state st;
  st.register_device(d.dev);
  st.apply_device_changes();

  auto in = inlet_on(d.param);
  st.register_port(*in);
  d.send(60);
  st.begin_tick();
  CHECK(heard(st, *in) == notes{60});

  st.reset();
  st.register_device(d.dev);
  st.apply_device_changes();
  rebind(st, *in, d.note);
  d.send(61);
  st.begin_tick();
  CHECK(heard(st, *in) == notes{61});

  st.unregister_port(*in);
  d.send(62);
  st.begin_tick();
  CHECK(d.pending() == 1);
}

TEST_CASE(
    "A MIDI device registered and removed before a tick is never read",
    "[execution][devices][midi]")
{
  PoisonableDevice keep{"midiB"};
  ossia::execution_state st;
  st.register_device(keep.dev);
  st.apply_device_changes();
  auto kept = inlet_on(keep.param);
  st.register_port(*kept);

  // Ports resolve against the edition devices, so one can bind to a device
  // the execution has not taken in yet.
  PoisonableDevice d{"midiA"};
  st.register_device(d.dev);
  auto in = inlet_on(d.param);
  st.register_port(*in);
  st.unregister_device(d.dev);
  in->address = {};
  d.destroy();

  keep.send(60);
  st.begin_tick();
  CHECK(heard(st, *kept) == notes{60});
  keep.send(61);
  st.begin_tick();
  CHECK(heard(st, *kept) == notes{61});
  st.unregister_port(*in);

  st.unregister_port(*kept);
  keep.send(62);
  st.begin_tick();
  CHECK(keep.pending() == 1);
}

TEST_CASE(
    "A MIDI stream removed and registered again in one batch keeps its inlets",
    "[execution][devices][midi]")
{
  // The execution only knows streams by address: a device created where a
  // removed one was is indistinguishable from the same device coming back.
  // Its inlets can bind before the batch is applied, since ports resolve
  // against the edition devices and execution commands run before the tick.
  PoisonableDevice d{"midiA"};
  ossia::execution_state st;
  st.register_device(d.dev);
  st.apply_device_changes();

  auto in = inlet_on(d.param);
  st.unregister_device(d.dev);
  st.register_device(d.dev);
  st.register_port(*in);

  d.send(60);
  st.begin_tick();
  CHECK(heard(st, *in) == notes{60});
  CHECK(d.pending() == 0);
}
#endif
