/**
 * Tests for the tree a `.midimap.json` description builds.
 *
 * What is worth pinning is the translation from the document to the tree:
 * groups becoming levels, a control's declared direction narrowed by the ports
 * that are actually open, the domain a control gets when the document omits its
 * bounds, and the `choice` node that appears beside a control whose values are
 * all named.
 *
 * The second half drives the wire through a loopback port: what comes in moves
 * only the node it addresses, and what is written to a node goes out exactly
 * once -- also while the device is hearing itself, which is what a motorised
 * fader amounts to.
 *
 * A protocol needs a port, so these open a real one. When the machine has none
 * the test says so and passes rather than failing for the absence of hardware.
 */

#include <Protocols/MIDIDevices/MidiDeviceProtocol.hpp>

#include <ossia/network/base/node.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <catch2/catch_all.hpp>

#include <libremidi/libremidi.hpp>

#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace Protocols::MIDIDevices;

namespace
{
const std::string document = R"_({
  "format": "score.midi-device/1",
  "manufacturer": "Test", "model": "Surface",
  "controls": [
    {"name": "Fader 1", "kind": "fader", "group": "Strip 1", "direction": "in",
     "message": {"type": "cc", "channel": 1, "number": 7},
     "value": {"mode": "absolute"}},
    {"name": "Encoder 1", "kind": "encoder", "group": ["Bank B : Equalizer", "Knobs"],
     "direction": "in", "message": {"type": "cc", "channel": 1, "number": 74},
     "value": {"mode": "relative", "encoding": "twos_complement"}},
    {"name": "Wave", "kind": "parameter", "group": "Osc 1 : Shape", "direction": "out",
     "message": {"type": "cc", "channel": 1, "number": 30},
     "value": {"mode": "absolute", "labels": [
       {"from": 0, "to": 0, "name": "Saw"},
       {"from": 1, "to": 1, "name": "Square"},
       {"from": 2, "to": 2, "name": "Triangle"}]}},
    {"name": "Cutoff", "kind": "knob", "direction": "both",
     "message": {"type": "nrpn", "channel": 1, "number": 1, "lsb": 12},
     "value": {"mode": "absolute"}},
    {"name": "Bend", "kind": "wheel", "direction": "in",
     "message": {"type": "pitchbend", "channel": 1},
     "value": {"mode": "absolute", "orientation": "bipolar"}}
  ]
})_";

//! The first backend with an output port. Midi Through is the usual one on
//! Linux; nothing is ever sent to it here.
std::optional<std::pair<libremidi::API, libremidi::output_port>> anyOutput()
{
  libremidi::observer_configuration conf;
  conf.track_hardware = true;
  conf.track_virtual = true;
  conf.track_network = true;

  // A shareable port such as Midi Through proves nothing about releasing an
  // exclusive one, so a real device can be named to test against.
  const char* wanted = std::getenv("SCORE_MIDI_TEST_PORT");
  const char* wantedApi = std::getenv("SCORE_MIDI_TEST_API");

  for(auto api : libremidi::available_apis())
  {
    if(wantedApi && std::string{magic_enum::enum_name(api)}.find(wantedApi)
                        == std::string::npos)
      continue;
    libremidi::observer obs{conf, libremidi::observer_configuration_for(api)};
    for(const auto& p : obs.get_output_ports())
    {
      if(wanted && p.display_name.find(wanted) == std::string::npos)
        continue;
      return std::pair{api, p};
    }
  }
  return std::nullopt;
}

//! find_child is non-const, so the walk is too.
ossia::net::node_base* at(ossia::net::node_base& root, const std::string& path)
{
  auto* node = &root;
  std::size_t pos = 0;
  while(pos <= path.size() && node)
  {
    const auto sep = path.find('/', pos);
    const auto piece
        = path.substr(pos, sep == std::string::npos ? sep : sep - pos);
    node = node->find_child(piece);
    if(sep == std::string::npos)
      break;
    pos = sep + 1;
  }
  return node;
}

std::pair<int, int> domainOf(const ossia::net::parameter_base& p)
{
  const auto& d = p.get_domain();
  return {
      ossia::convert<int>(ossia::get_min(d)), ossia::convert<int>(ossia::get_max(d))};
}
}

TEST_CASE("a description becomes a tree", "[mididevice][midi]")
{
  const auto found = anyOutput();
  if(!found)
  {
    WARN("no MIDI output port on this machine; tree not built");
    SUCCEED();
    return;
  }
  const auto& [api, out] = *found;

  auto map = parseDeviceMap(document);
  REQUIRE(map.has_value());
  REQUIRE(map->controls.size() == 5);

  ProtocolSettings conf;
  conf.api = api;
    conf.output = out;
  conf.devices.push_back({*map, 1});

  // Output only: nothing is ever received, so no node may claim to be readable.
  auto dev = std::make_unique<ossia::net::generic_device>(
      makeProtocol(std::move(conf)), "surface");
  auto& root = dev->get_root_node();

  SECTION("a group becomes a level, and an array member keeps its colon")
  {
    REQUIRE(at(root, "Surface/Strip 1/Fader 1"));

    // "Bank B : Equalizer" was written as one array member, so it is one level.
    REQUIRE(at(root, "Surface/Bank B : Equalizer/Knobs/Encoder 1"));

    // The string form nests on the colon, so this one is two levels.
    REQUIRE(at(root, "Surface/Osc 1/Shape/Wave"));

    // A control with no group sits at the root.
    REQUIRE(at(root, "Surface/Cutoff"));
  }

  SECTION("the open ports narrow what a control claims to do")
  {
    // Output only. A control the document calls `in` cannot be reached at all,
    // so it is readable-but-never-updated rather than writable: writing it
    // would put a message on the cable the device never said it listens to.
    auto* fader = at(root, "Surface/Strip 1/Fader 1");
    REQUIRE(fader);
    REQUIRE(fader->get_parameter());
    CHECK(fader->get_parameter()->get_access() == ossia::access_mode::GET);

    // `both` narrows to what the one open port can do.
    auto* cutoff = at(root, "Surface/Cutoff");
    REQUIRE(cutoff);
    CHECK(cutoff->get_parameter()->get_access() == ossia::access_mode::SET);

    // `out` is what an output port is for.
    auto* wave = at(root, "Surface/Osc 1/Shape/Wave");
    REQUIRE(wave);
    CHECK(wave->get_parameter()->get_access() == ossia::access_mode::SET);
  }

  SECTION("an omitted bound is the message type's natural range")
  {
    auto* fader = at(root, "Surface/Strip 1/Fader 1");
    REQUIRE(fader);
    CHECK(domainOf(*fader->get_parameter()) == std::pair{0, 127});

    // 14-bit, and unsigned: pitch bend is centred on 8192 rather than
    // renumbered.
    auto* bend = at(root, "Surface/Bend");
    REQUIRE(bend);
    CHECK(domainOf(*bend->get_parameter()) == std::pair{0, 16383});

    auto* cutoff = at(root, "Surface/Cutoff");
    REQUIRE(cutoff);
    CHECK(domainOf(*cutoff->get_parameter()) == std::pair{0, 16383});
  }

  SECTION("named values get a choice beside the number")
  {
    auto* wave = at(root, "Surface/Osc 1/Shape/Wave");
    REQUIRE(wave);

    auto* choice = at(root, "Surface/Osc 1/Shape/Wave/choice");
    REQUIRE(choice);
    REQUIRE(choice->get_parameter());
    CHECK(choice->get_parameter()->get_value_type() == ossia::val_type::STRING);

    // A control whose values are not named has no choice node.
    auto* fader = at(root, "Surface/Strip 1/Fader 1");
    REQUIRE(fader);
    CHECK(at(root, "Surface/Strip 1/Fader 1/choice") == nullptr);
  }

  SECTION("a relative encoder starts at the bottom of its range")
  {
    auto* enc = at(root, "Surface/Bank B : Equalizer/Knobs/Encoder 1");
    REQUIRE(enc);

    // An encoder sends deltas, so the node holds the accumulated position and
    // has to have one before the first delta arrives. Checking that it is
    // merely valid proves nothing: a new parameter is already an int 0.
    CHECK(ossia::convert<int>(enc->get_parameter()->value()) == 0);
    CHECK(domainOf(*enc->get_parameter()) == std::pair{0, 127});
  }
}

TEST_CASE("the same port can be used again after the device goes away", "[mididevice][midi]")
{
  const auto found = anyOutput();
  if(!found)
  {
    WARN("no MIDI output port on this machine");
    SUCCEED();
    return;
  }
  const auto& [api, out] = *found;

  auto map = parseDeviceMap(document);
  REQUIRE(map.has_value());

  const auto build = [&] {
    ProtocolSettings conf;
    conf.api = api;
        conf.output = out;
    conf.devices.push_back({*map, 1});
    return std::make_unique<ossia::net::generic_device>(
        makeProtocol(std::move(conf)), "surface");
  };

  // Adding a device, removing it and adding another must not leave the port
  // held: the second open is the one a user hits.
  for(int attempt = 0; attempt < 3; attempt++)
  {
    INFO("attempt " << attempt);
    std::unique_ptr<ossia::net::generic_device> dev;
    REQUIRE_NOTHROW(dev = build());
    REQUIRE(dev);
    dev.reset();
  }

  // And while one is still open: a second device on the same port either opens
  // or refuses, but the refusal must not outlive the first device.
  {
    auto first = build();

    // Whether a second writer is allowed is the backend's business -- a
    // sequencer port shares, a raw device does not -- but it must answer
    // rather than block, and either answer must leave the port usable.
    bool opened = false;
    try
    {
      auto second = build();
      opened = true;
    }
    catch(const std::exception&)
    {
    }
    INFO("a second device on the same port " << (opened ? "opened" : "was refused"));
  }
  REQUIRE_NOTHROW(build());
}

TEST_CASE("a port survives a device cycle while an observer is watching",
          "[mididevice][midi]")
{
  const auto found = anyOutput();
  if(!found)
  {
    WARN("no MIDI output port on this machine");
    SUCCEED();
    return;
  }
  const auto& [api, out] = *found;

  auto map = parseDeviceMap(document);
  REQUIRE(map.has_value());

  /*
   * The settings widget keeps an observer open for as long as it is showing,
   * so in the application a device is created, destroyed and created again
   * with one alive the whole time. A backend that shares state between its
   * observer and its ports behaves differently then than in isolation.
   */
  libremidi::observer_configuration watching;
  watching.track_hardware = true;
  watching.track_virtual = true;
  watching.track_network = true;
  libremidi::observer keptAlive{
      watching, libremidi::observer_configuration_for(api)};

  const auto build = [&] {
    ProtocolSettings conf;
    conf.api = api;
        conf.output = out;
    conf.devices.push_back({*map, 1});
    return std::make_unique<ossia::net::generic_device>(
        makeProtocol(std::move(conf)), "surface");
  };

  for(int attempt = 0; attempt < 3; attempt++)
  {
    INFO("attempt " << attempt << " with an observer alive");
    std::unique_ptr<ossia::net::generic_device> dev;
    REQUIRE_NOTHROW(dev = build());
    dev.reset();
  }
}

namespace
{
/**
 * A port that loops back: what is written to the output arrives on the input.
 *
 * The kernel's "Midi Through" is one on Linux, which is what lets a test drive
 * the receive path and read the send path without hardware. Both halves have to
 * come from the same backend, since a port handle means nothing to another.
 */
struct loopback
{
  libremidi::API api{};
  libremidi::input_port in;
  libremidi::output_port out;
};

std::optional<loopback> findLoopback()
{
  libremidi::observer_configuration conf;
  conf.track_hardware = true;
  conf.track_virtual = true;
  conf.track_network = true;

  const char* wantedApi = std::getenv("SCORE_MIDI_TEST_API");

  for(auto api : libremidi::available_apis())
  {
    if(wantedApi && std::string{magic_enum::enum_name(api)}.find(wantedApi)
                        == std::string::npos)
      continue;

    libremidi::observer obs{conf, libremidi::observer_configuration_for(api)};
    for(const auto& i : obs.get_input_ports())
    {
      if(i.display_name.find("Through") == std::string::npos)
        continue;
      for(const auto& o : obs.get_output_ports())
        if(o.display_name.find("Through") != std::string::npos)
          return loopback{api, i, o};
    }
  }
  return std::nullopt;
}

//! Wait for @p pred, so a test never depends on how fast the backend delivers.
template <typename F>
bool waitFor(F&& pred, std::chrono::milliseconds timeout = std::chrono::seconds{2})
{
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while(std::chrono::steady_clock::now() < deadline)
  {
    if(pred())
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds{5});
  }
  return pred();
}

//! A device reading from the loopback, plus a port to inject into it.
struct injector
{
  std::unique_ptr<ossia::net::generic_device> dev;
  std::unique_ptr<libremidi::midi_out> port;

  void send(std::initializer_list<unsigned char> bytes)
  {
    std::vector<unsigned char> v{bytes};
    port->send_message(v.data(), v.size());
  }
};

injector makeReceiver(const loopback& lb, const std::string& doc)
{
  auto map = parseDeviceMap(doc);
  REQUIRE(map.has_value());

  ProtocolSettings conf;
  conf.api = lb.api;
    conf.input = lb.in;
  conf.devices.push_back({*map, 1});

  injector inj;
  inj.dev = std::make_unique<ossia::net::generic_device>(
      makeProtocol(std::move(conf)), "recv");

  libremidi::output_configuration oc{};
  inj.port = std::make_unique<libremidi::midi_out>(oc, lb.api);
  REQUIRE(inj.port->open_port(lb.out) == stdx::error{});
  return inj;
}

int valueOf(ossia::net::node_base& root, const std::string& path)
{
  auto* n = at(root, path);
  REQUIRE(n);
  REQUIRE(n->get_parameter());
  return ossia::convert<int>(n->get_parameter()->value());
}
}

TEST_CASE("an incoming note only moves the control it addresses", "[mididevice][midi]")
{
  const auto lb = findLoopback();
  if(!lb)
  {
    WARN("no loopback MIDI port on this machine");
    SUCCEED();
    return;
  }

  auto inj = makeReceiver(*lb, R"_({
    "format": "score.midi-device/1", "model": "Drums",
    "controls": [
      {"name": "Kick",  "kind": "pad", "direction": "in",
       "message": {"type": "note", "channel": 1, "number": 36}, "value": {}},
      {"name": "Snare", "kind": "pad", "direction": "in",
       "message": {"type": "note", "channel": 1, "number": 38}, "value": {}},
      {"name": "Slices", "kind": "pad", "direction": "in",
       "message": {"type": "note", "channel": 1, "range": {"from": 12, "to": 24}},
       "value": {}}
    ]})_");
  auto& root = inj.dev->get_root_node();

  // A control that addresses a bank of notes holds which note, so it starts at
  // the bottom of the bank rather than at a note it cannot name.
  CHECK(valueOf(root, "Drums/Slices") == 12);

  // Note 36 is Kick's own address.
  inj.send({0x90, 36, 100});
  CHECK(waitFor([&] { return valueOf(root, "Drums/Kick") == 100; }));
  CHECK(valueOf(root, "Drums/Snare") == 0);

  // Note 60 addresses none of the three: it is not 36, not 38, and outside
  // 12..24. Nothing may move.
  inj.send({0x90, 60, 100});
  std::this_thread::sleep_for(std::chrono::milliseconds{200});
  CHECK(valueOf(root, "Drums/Snare") == 0);
  CHECK(valueOf(root, "Drums/Slices") == 12);

  // A note inside the range is the range control's value.
  inj.send({0x90, 20, 100});
  CHECK(waitFor([&] { return valueOf(root, "Drums/Slices") == 20; }));
  CHECK(valueOf(root, "Drums/Snare") == 0);

  // A note-off for a note that never sounded must not release another pad.
  inj.send({0x80, 60, 0});
  std::this_thread::sleep_for(std::chrono::milliseconds{200});
  CHECK(valueOf(root, "Drums/Kick") == 100);

  inj.send({0x80, 36, 0});
  CHECK(waitFor([&] { return valueOf(root, "Drums/Kick") == 0; }));
}

TEST_CASE("an incoming poly aftertouch only moves its own note", "[mididevice][midi]")
{
  const auto lb = findLoopback();
  if(!lb)
  {
    SUCCEED();
    return;
  }

  auto inj = makeReceiver(*lb, R"_({
    "format": "score.midi-device/1", "model": "T",
    "controls": [
      {"name": "PressA", "direction": "in",
       "message": {"type": "poly_aftertouch", "channel": 1, "number": 40}, "value": {}},
      {"name": "PressB", "direction": "in",
       "message": {"type": "poly_aftertouch", "channel": 1, "number": 41}, "value": {}}
    ]})_");
  auto& root = inj.dev->get_root_node();

  inj.send({0xA0, 40, 77});
  CHECK(waitFor([&] { return valueOf(root, "T/PressA") == 77; }));
  CHECK(valueOf(root, "T/PressB") == 0);
}

TEST_CASE("a relative encoder accumulates the deltas it receives", "[mididevice][midi]")
{
  const auto lb = findLoopback();
  if(!lb)
  {
    SUCCEED();
    return;
  }

  auto inj = makeReceiver(*lb, R"_({
    "format": "score.midi-device/1", "model": "T",
    "controls": [
      {"name": "Enc", "kind": "encoder", "direction": "in",
       "message": {"type": "cc", "channel": 1, "number": 74},
       "value": {"mode": "relative", "encoding": "twos_complement"}}
    ]})_");
  auto& root = inj.dev->get_root_node();

  // Two's complement: 1 is +1, 127 is -1. The node holds the running total,
  // because nothing on the wire carries a position.
  inj.send({0xB0, 74, 1});
  inj.send({0xB0, 74, 1});
  inj.send({0xB0, 74, 1});
  CHECK(waitFor([&] { return valueOf(root, "T/Enc") == 3; }));

  inj.send({0xB0, 74, 127});
  CHECK(waitFor([&] { return valueOf(root, "T/Enc") == 2; }));

  // It cannot run past the end of its range.
  for(int i = 0; i < 8; i++)
    inj.send({0xB0, 74, 127});
  CHECK(waitFor([&] { return valueOf(root, "T/Enc") == 0; }));
}

TEST_CASE("the same document narrows the other way on an input", "[mididevice][midi]")
{
  const auto lb = findLoopback();
  if(!lb)
  {
    SUCCEED();
    return;
  }

  auto map = parseDeviceMap(document);
  REQUIRE(map.has_value());

  const auto access = [&](bool in, bool out, const std::string& path) {
    ProtocolSettings conf;
    conf.api = lb->api;
        if(in)
      conf.input = lb->in;
    if(out)
      conf.output = lb->out;
    conf.devices.push_back({*map, 1});

    auto dev = std::make_unique<ossia::net::generic_device>(
        makeProtocol(std::move(conf)), "narrow");
    auto* n = at(dev->get_root_node(), path);
    REQUIRE(n);
    REQUIRE(n->get_parameter());
    return n->get_parameter()->get_access();
  };

  // Input only: what the hardware sends is readable, and an instrument
  // parameter there is unreachable rather than writable.
  CHECK(access(true, false, "Surface/Strip 1/Fader 1") == ossia::access_mode::GET);
  CHECK(access(true, false, "Surface/Osc 1/Shape/Wave") == ossia::access_mode::GET);
  CHECK(access(true, false, "Surface/Cutoff") == ossia::access_mode::GET);

  // Both: each control gets exactly what it declares.
  CHECK(access(true, true, "Surface/Strip 1/Fader 1") == ossia::access_mode::GET);
  CHECK(access(true, true, "Surface/Osc 1/Shape/Wave") == ossia::access_mode::SET);
  CHECK(access(true, true, "Surface/Cutoff") == ossia::access_mode::BI);
}

TEST_CASE("a bipolar control starts at the centre the device calls centre",
          "[mididevice][midi]")
{
  const auto found = anyOutput();
  if(!found)
  {
    SUCCEED();
    return;
  }
  const auto& [api, out] = *found;

  auto map = parseDeviceMap(document);
  REQUIRE(map.has_value());

  ProtocolSettings conf;
  conf.api = api;
    conf.output = out;
  conf.devices.push_back({*map, 1});

  auto dev = std::make_unique<ossia::net::generic_device>(
      makeProtocol(std::move(conf)), "centre");

  // 14-bit bipolar: 8192, not 8191. The wire range is 0..16383 and the value
  // a pitch bend wheel rests at is its midpoint.
  auto* bend = at(dev->get_root_node(), "Surface/Bend");
  REQUIRE(bend);
  CHECK(ossia::convert<int>(bend->get_parameter()->value()) == 8192);
}

namespace
{
/**
 * A device with both of its ports on the loopback, so that it hears every
 * message it sends, plus a tap that records everything on the wire and a port
 * to inject from. What the tap counts is the whole truth: a message sent twice
 * or not at all shows up as a count, not as a guess about timing.
 */
struct bidir
{
  bidir(const loopback& lb, const std::string& doc, bool withInput = true)
  {
    libremidi::input_configuration ic{};
    ic.on_message = [this](const libremidi::message& m) {
      std::lock_guard lock{m_mutex};
      m_seen.push_back(m);
    };
    tap = std::make_unique<libremidi::midi_in>(ic, lb.api);
    REQUIRE(tap->open_port(lb.in) == stdx::error{});

    libremidi::output_configuration oc{};
    inject = std::make_unique<libremidi::midi_out>(oc, lb.api);
    REQUIRE(inject->open_port(lb.out) == stdx::error{});

    auto map = parseDeviceMap(doc);
    REQUIRE(map.has_value());

    ProtocolSettings conf;
    conf.api = lb.api;
        if(withInput)
      conf.input = lb.in;
    conf.output = lb.out;
    conf.devices.push_back({*map, 1});
    dev = std::make_unique<ossia::net::generic_device>(
        makeProtocol(std::move(conf)), "bidir");
  }

  ~bidir()
  {
    // The device first: its input callback must be gone before the tap's.
    dev.reset();
    tap.reset();
  }

  //! The description's own level of the tree, which every control sits under.
  std::string deviceNode{"Wire"};

  ossia::net::parameter_base& param(const std::string& path)
  {
    auto* n = at(dev->get_root_node(), deviceNode + "/" + path);
    REQUIRE(n);
    REQUIRE(n->get_parameter());
    return *n->get_parameter();
  }

  void send(std::initializer_list<unsigned char> bytes)
  {
    std::vector<unsigned char> v{bytes};
    inject->send_message(v.data(), v.size());
  }

  std::size_t count()
  {
    std::lock_guard lock{m_mutex};
    return m_seen.size();
  }

  //! How many times @p bytes went over the wire.
  std::size_t count(std::initializer_list<unsigned char> bytes)
  {
    std::lock_guard lock{m_mutex};
    return std::count_if(m_seen.begin(), m_seen.end(), [&](const auto& m) {
      return std::equal(m.bytes.begin(), m.bytes.end(), bytes.begin(), bytes.end());
    });
  }

  /**
   * The number of messages on the wire once @p expected have arrived and
   * nothing has followed them for a while. Waiting for "at least" and then
   * checking "exactly" is what tells one message from two.
   */
  std::size_t settledCount(std::size_t expected)
  {
    waitFor([&] { return count() >= expected; }, std::chrono::seconds{5});
    std::this_thread::sleep_for(std::chrono::milliseconds{150});
    return count();
  }

  std::unique_ptr<ossia::net::generic_device> dev;
  std::unique_ptr<libremidi::midi_in> tap;
  std::unique_ptr<libremidi::midi_out> inject;

private:
  std::mutex m_mutex;
  std::vector<libremidi::message> m_seen;
};

//! Wave is written and has names; Level is a motorised fader, written and
//! heard; Mode is only heard and has names.
const std::string wireDocument = R"_({
  "format": "score.midi-device/1", "model": "Wire",
  "controls": [
    {"name": "Wave", "kind": "parameter", "direction": "out",
     "message": {"type": "cc", "channel": 1, "number": 30},
     "value": {"mode": "absolute", "labels": [
       {"from": 0, "to": 0, "name": "Saw"},
       {"from": 1, "to": 1, "name": "Square"},
       {"from": 2, "to": 2, "name": "Triangle"}]}},
    {"name": "Level", "kind": "fader", "direction": "both",
     "message": {"type": "cc", "channel": 1, "number": 7},
     "value": {"mode": "absolute"}},
    {"name": "Mode", "kind": "switch", "direction": "in",
     "message": {"type": "cc", "channel": 1, "number": 20},
     "value": {"mode": "absolute", "labels": [
       {"from": 0, "to": 0, "name": "A"},
       {"from": 1, "to": 1, "name": "B"},
       {"from": 2, "to": 2, "name": "C"}]}}
  ]})_";
}

TEST_CASE("a named control sends once whichever of its two nodes is written",
          "[mididevice][midi]")
{
  const auto lb = findLoopback();
  if(!lb)
  {
    WARN("no loopback MIDI port on this machine");
    SUCCEED();
    return;
  }

  bidir w{*lb, wireDocument};
  auto& wave = w.param("Wave");
  auto& choice = w.param("Wave/choice");

  // The number: one message, and the name follows.
  wave.push_value(1);
  CHECK(w.settledCount(1) == 1);
  CHECK(w.count({0xB0, 30, 1}) == 1);
  CHECK(ossia::convert<std::string>(choice.value()) == "Square");

  // The name: one message, and the number follows. Setting the number is what
  // makes the name apply; it must not also count as a second write.
  choice.push_value(std::string{"Triangle"});
  CHECK(w.settledCount(2) == 2);
  CHECK(w.count({0xB0, 30, 2}) == 1);
  CHECK(valueOf(w.dev->get_root_node(), "Wire/Wave") == 2);

  // A name the control does not have goes nowhere.
  choice.push_value(std::string{"Sine"});
  CHECK(w.settledCount(2) == 2);
}

TEST_CASE("a write made while another control is being updated is not dropped",
          "[mididevice][midi]")
{
  const auto lb = findLoopback();
  if(!lb)
  {
    SUCCEED();
    return;
  }

  bidir w{*lb, wireDocument};
  auto& level = w.param("Level");

  // A mapping reacting to one control by driving another runs inside the
  // first control's notification: that is when the second write happens.
  std::atomic<int> reactions{0};

  SECTION("from a write: the twin of a named control is being mirrored")
  {
    w.param("Wave/choice").add_callback([&](const ossia::value&) {
      if(reactions.fetch_add(1) == 0)
        level.push_value(64);
    });

    w.param("Wave").push_value(1);
    CHECK(w.settledCount(2) == 2);
    CHECK(w.count({0xB0, 30, 1}) == 1);
    CHECK(w.count({0xB0, 7, 64}) == 1);
  }

  SECTION("from the wire: a control is being updated by the device")
  {
    w.param("Mode").add_callback([&](const ossia::value&) {
      if(reactions.fetch_add(1) == 0)
        level.push_value(100);
    });

    // The injected message itself is on the wire, then Level's.
    w.send({0xB0, 20, 1});
    CHECK(w.settledCount(2) == 2);
    CHECK(w.count({0xB0, 7, 100}) == 1);
    CHECK(valueOf(w.dev->get_root_node(), "Wire/Mode") == 1);
    CHECK(ossia::convert<std::string>(w.param("Mode/choice").value()) == "B");
  }
}

TEST_CASE("concurrent writes and receipts neither echo nor go missing",
          "[mididevice][midi]")
{
  const auto lb = findLoopback();
  if(!lb)
  {
    SUCCEED();
    return;
  }

  bidir w{*lb, wireDocument};
  auto& wave = w.param("Wave");
  auto& level = w.param("Level");

  /*
   * One thread writes, the wire delivers, both to controls with names so that
   * every event also mirrors a twin, and Level is hit from both sides at once.
   * Short bursts with a pause keep within what the loopback carries: the
   * PipeWire bridge drops what it cannot fit in a cycle, and a loss of that
   * kind would look like a swallowed write.
   */
  constexpr int rounds = 200;
  constexpr int burst = 3;

  std::thread writer{[&] {
    for(int r = 0; r < rounds; r++)
    {
      for(int i = 0; i < burst; i++)
      {
        wave.push_value((r + i) % 3);
        level.push_value((r * burst + i) % 128);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
  }};

  for(int r = 0; r < rounds; r++)
  {
    for(int i = 0; i < burst; i++)
    {
      w.send({0xB0, 20, static_cast<unsigned char>((r + i) % 3)});
      w.send({0xB0, 7, static_cast<unsigned char>((r * burst + i) % 128)});
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{1});
  }
  writer.join();

  // Every write and every injection, once: a receipt that came back out would
  // add to this, a write that was swallowed would take from it.
  const std::size_t expected = 4 * rounds * burst;
  CHECK(w.settledCount(expected) == expected);
}

TEST_CASE("a relative control sends the step from where it really was",
          "[mididevice][midi]")
{
  const auto lb = findLoopback();
  if(!lb)
  {
    SUCCEED();
    return;
  }

  // Output only: a step that came back in would be taken as the device moving.
  bidir w{
      *lb, R"_({
    "format": "score.midi-device/1", "model": "T",
    "controls": [
      {"name": "Enc", "kind": "encoder", "direction": "both",
       "message": {"type": "cc", "channel": 1, "number": 74},
       "value": {"mode": "relative", "encoding": "twos_complement"}}
    ]})_",
      false};
  w.deviceNode = "T";
  auto& enc = w.param("Enc");

  enc.push_value(10);
  CHECK(w.settledCount(1) == 1);
  CHECK(w.count({0xB0, 74, 10}) == 1);

  // From 10 to 7 is -3, which two's complement writes as 125.
  enc.push_value(7);
  CHECK(w.settledCount(2) == 2);
  CHECK(w.count({0xB0, 74, 125}) == 1);

  // Nothing moved, nothing to say.
  enc.push_value(7);
  CHECK(w.settledCount(2) == 2);
}

TEST_CASE("an NRPN arrives as the four messages that carry it", "[mididevice][midi]")
{
  const auto lb = findLoopback();
  if(!lb)
  {
    SUCCEED();
    return;
  }

  auto inj = makeReceiver(*lb, R"_({
    "format": "score.midi-device/1", "model": "T",
    "controls": [
      {"name": "Wide", "kind": "parameter", "direction": "in",
       "message": {"type": "nrpn", "channel": 1, "number": 1, "lsb": 2},
       "value": {"mode": "absolute"}},
      {"name": "Narrow", "kind": "parameter", "direction": "in",
       "message": {"type": "nrpn", "channel": 1, "number": 1, "lsb": 3},
       "value": {"mode": "absolute", "min": 0, "max": 127}},
      {"name": "Reg", "kind": "parameter", "direction": "in",
       "message": {"type": "rpn", "channel": 1, "number": 0, "lsb": 0},
       "value": {"mode": "absolute"}}
    ]})_");
  auto& root = inj.dev->get_root_node();

  // Select NRPN 1/2, then write it. A 14-bit parameter is only complete once
  // the data LSB has arrived.
  inj.send({0xB0, 99, 1});
  inj.send({0xB0, 98, 2});
  inj.send({0xB0, 6, 0x40});
  inj.send({0xB0, 38, 0x01});
  CHECK(waitFor([&] { return valueOf(root, "T/Wide") == ((0x40 << 7) | 1); }));

  // The selection stays latched: a further data entry writes the same
  // parameter without naming it again.
  inj.send({0xB0, 6, 0x10});
  inj.send({0xB0, 38, 0x02});
  CHECK(waitFor([&] { return valueOf(root, "T/Wide") == ((0x10 << 7) | 2); }));

  // A 7-bit parameter takes its value from the data MSB alone.
  inj.send({0xB0, 99, 1});
  inj.send({0xB0, 98, 3});
  inj.send({0xB0, 6, 99});
  CHECK(waitFor([&] { return valueOf(root, "T/Narrow") == 99; }));

  // Selecting 1/3 must not have moved 1/2.
  CHECK(valueOf(root, "T/Wide") == ((0x10 << 7) | 2));

  // An RPN is a different address space from an NRPN with the same numbers.
  inj.send({0xB0, 101, 0});
  inj.send({0xB0, 100, 0});
  inj.send({0xB0, 6, 0x02});
  inj.send({0xB0, 38, 0x00});
  CHECK(waitFor([&] { return valueOf(root, "T/Reg") == (0x02 << 7); }));
  CHECK(valueOf(root, "T/Narrow") == 99);
}

TEST_CASE("two devices on one port keep their controls apart", "[mididevice][midi]")
{
  const auto lb = findLoopback();
  if(!lb)
  {
    SUCCEED();
    return;
  }

  // The same description twice, as two instruments of one model on one cable
  // set to different channels -- the case a single tree could not express.
  const std::string one = R"_({"format":"score.midi-device/1","model":"Synth",
    "controls":[{"name":"Cutoff","direction":"in",
      "message":{"type":"cc","number":74},"value":{}}]})_";

  auto map = parseDeviceMap(one);
  REQUIRE(map.has_value());

  ProtocolSettings conf;
  conf.api = lb->api;
  conf.input = lb->in;
  conf.devices.push_back({*map, 1});
  conf.devices.push_back({*map, 5});

  auto dev = std::make_unique<ossia::net::generic_device>(
      makeProtocol(std::move(conf)), "chain");
  auto& root = dev->get_root_node();

  // Two of the same model cannot share a level, so the second is uniquified
  // rather than merged into the first.
  REQUIRE(root.children().size() == 2);
  const auto first = "Synth/Cutoff";
  auto* second = at(root, "Synth.1/Cutoff");
  REQUIRE(at(root, first));
  REQUIRE(second);

  libremidi::output_configuration oc{};
  auto port = std::make_unique<libremidi::midi_out>(oc, lb->api);
  REQUIRE(port->open_port(lb->out) == stdx::error{});
  const auto send = [&](std::initializer_list<unsigned char> b) {
    std::vector<unsigned char> v{b};
    port->send_message(v.data(), v.size());
  };

  // The description states no channel, so each device answers on the channel
  // its slot gives it and ignores the other's.
  send({0xB0, 74, 100});
  CHECK(waitFor([&] { return valueOf(root, first) == 100; }));
  CHECK(ossia::convert<int>(second->get_parameter()->value()) == 0);

  send({0xB4, 74, 60});
  CHECK(waitFor(
      [&] { return ossia::convert<int>(second->get_parameter()->value()) == 60; }));
  CHECK(valueOf(root, first) == 100);
}

TEST_CASE("note names stay out of the tree, note controls do not",
          "[mididevice][midi]")
{
  const auto found = anyOutput();
  if(!found)
  {
    SUCCEED();
    return;
  }
  const auto& [api, out] = *found;

  auto map = parseDeviceMap(R"_({
    "format": "score.midi-device/1", "model": "Kit",
    "controls": [
      {"name": "Cutoff", "kind": "parameter", "direction": "out",
       "message": {"type": "cc", "channel": 1, "number": 74}, "value": {}},

      {"name": "Bass Drum", "kind": "pad", "direction": "out",
       "message": {"type": "note", "channel": 10, "number": 36}, "value": {}},
      {"name": "Snare", "kind": "pad", "direction": "out",
       "message": {"type": "note", "channel": 10, "number": 38}, "value": {}},

      {"name": "PadLed", "kind": "button", "direction": "out",
       "message": {"type": "note", "channel": 1, "number": 20}, "value": {}},
      {"name": "PlayPad", "kind": "pad", "direction": "in",
       "message": {"type": "note", "channel": 1, "number": 21}, "value": {}}
    ]})_");
  REQUIRE(map.has_value());
  REQUIRE(map->controls.size() == 5);

  ProtocolSettings conf;
  conf.api = api;
  conf.output = out;
  conf.devices.push_back({*map, 1});

  auto dev = std::make_unique<ossia::net::generic_device>(
      makeProtocol(std::move(conf)), "kit");
  auto& root = dev->get_root_node();

  // A pad the host would play on an instrument is a note name: documentation
  // of what note 36 means on this kit, not a control.
  CHECK(at(root, "Kit/Bass Drum") == nullptr);
  CHECK(at(root, "Kit/Snare") == nullptr);

  // Everything else that happens to be a note stays: an LED is driven, and a
  // pad the hardware sends from is read.
  CHECK(at(root, "Kit/PadLed") != nullptr);
  CHECK(at(root, "Kit/PlayPad") != nullptr);
  CHECK(at(root, "Kit/Cutoff") != nullptr);

  // The reader still carries them; only the tree leaves them out.
  CHECK(std::count_if(
            map->controls.begin(), map->controls.end(),
            [](const Control& c) { return isNoteName(c); })
        == 2);
}

TEST_CASE("one named value is still worth naming", "[mididevice][midi]")
{
  const auto found = anyOutput();
  if(!found)
  {
    SUCCEED();
    return;
  }
  const auto& [api, out] = *found;

  auto map = parseDeviceMap(R"_({
    "format": "score.midi-device/1", "model": "Synth",
    "controls": [
      {"name": "Single", "kind": "parameter", "direction": "out",
       "message": {"type": "program", "channel": 1, "bank": {"msb": 0, "lsb": 25}},
       "value": {"labels": [{"from": 0, "to": 0, "name": "Grand Piano"}]}},
      {"name": "Ranged", "kind": "knob", "direction": "out",
       "message": {"type": "cc", "channel": 1, "number": 20},
       "value": {"labels": [{"from": 0, "to": 63, "name": "Off"},
                            {"from": 64, "to": 127, "name": "On"}]}},
      {"name": "Plain", "kind": "knob", "direction": "out",
       "message": {"type": "cc", "channel": 1, "number": 21}, "value": {}}
    ]})_");
  REQUIRE(map.has_value());

  ProtocolSettings conf;
  conf.api = api;
  conf.output = out;
  conf.devices.push_back({*map, 1});

  auto dev = std::make_unique<ossia::net::generic_device>(
      makeProtocol(std::move(conf)), "synth");
  auto& root = dev->get_root_node();

  // A bank holding one patch names it, and that name is the only thing the
  // document says about it.
  auto* single = at(root, "Synth/Single/choice");
  REQUIRE(single);
  REQUIRE(single->get_parameter());
  CHECK(single->get_parameter()->get_value_type() == ossia::val_type::STRING);

  // A label spanning a range has no single value to send back, so those still
  // get no choice however many there are.
  CHECK(at(root, "Synth/Ranged/choice") == nullptr);
  CHECK(at(root, "Synth/Plain/choice") == nullptr);
}

namespace
{
//! The same tap-and-inject arrangement as @ref bidir, for a port carrying a
//! raw channel instead of a description.
struct raw_bidir
{
  raw_bidir(const loopback& lb, GenericChannel channel)
      : raw_bidir{lb, std::vector<GenericChannel>{channel}}
  {
  }

  raw_bidir(const loopback& lb, std::vector<GenericChannel> channels)
  {
    libremidi::input_configuration ic{};
    ic.on_message = [this](const libremidi::message& m) {
      std::lock_guard lock{m_mutex};
      m_seen.push_back(m);
    };
    tap = std::make_unique<libremidi::midi_in>(ic, lb.api);
    REQUIRE(tap->open_port(lb.in) == stdx::error{});

    libremidi::output_configuration oc{};
    inject = std::make_unique<libremidi::midi_out>(oc, lb.api);
    REQUIRE(inject->open_port(lb.out) == stdx::error{});

    ProtocolSettings conf;
    conf.api = lb.api;
    conf.input = lb.in;
    conf.output = lb.out;
    conf.generic = std::move(channels);
    dev = std::make_unique<ossia::net::generic_device>(
        makeProtocol(std::move(conf)), "raw");
  }

  ~raw_bidir()
  {
    dev.reset();
    tap.reset();
  }

  ossia::net::parameter_base& param(const std::string& path)
  {
    auto* n = at(dev->get_root_node(), path);
    REQUIRE(n);
    REQUIRE(n->get_parameter());
    return *n->get_parameter();
  }

  void send(std::initializer_list<unsigned char> bytes)
  {
    std::vector<unsigned char> v{bytes};
    inject->send_message(v.data(), v.size());
  }

  std::size_t count(std::initializer_list<unsigned char> bytes)
  {
    std::lock_guard lock{m_mutex};
    return std::count_if(m_seen.begin(), m_seen.end(), [&](const auto& m) {
      return std::equal(m.bytes.begin(), m.bytes.end(), bytes.begin(), bytes.end());
    });
  }

  std::unique_ptr<ossia::net::generic_device> dev;
  std::unique_ptr<libremidi::midi_in> tap;
  std::unique_ptr<libremidi::midi_out> inject;

private:
  std::mutex m_mutex;
  std::vector<libremidi::message> m_seen;
};
}

TEST_CASE("a raw channel is one level with the five message kinds",
          "[mididevice][midi]")
{
  const auto found = anyOutput();
  if(!found)
  {
    WARN("no MIDI output port on this machine; tree not built");
    SUCCEED();
    return;
  }

  ProtocolSettings conf;
  conf.api = found->first;
  conf.output = found->second;
  conf.generic.push_back({3, false});
  conf.generic.push_back({10, true});

  auto dev = std::make_unique<ossia::net::generic_device>(
      makeProtocol(std::move(conf)), "raw");
  auto& root = dev->get_root_node();

  for(const auto* name : {"on", "off", "control", "program", "pitchbend"})
  {
    auto* n = at(root, std::string{"3/"} + name);
    REQUIRE(n);
    REQUIRE(n->get_parameter());
  }

  // The number is half of the value, so the node is a list of two.
  CHECK(at(root, "3/on")->get_parameter()->get_value_type() == ossia::val_type::LIST);
  CHECK(at(root, "3/program")->get_parameter()->get_value_type() == ossia::val_type::INT);

  // A wheel rests at its midpoint, as a described one does.
  CHECK(ossia::convert<int>(at(root, "3/pitchbend")->get_parameter()->value()) == 8192);

  // Plain: nothing below the five.
  CHECK(at(root, "3/on/60") == nullptr);
  CHECK(at(root, "3/control/7") == nullptr);

  // Expanded: one node per number, and a program change has nothing left to
  // say once its number is the address.
  REQUIRE(at(root, "10/on/60"));
  REQUIRE(at(root, "10/control/7"));
  REQUIRE(at(root, "10/program/3"));
  CHECK(at(root, "10/on/127"));
  CHECK(at(root, "10/on/128") == nullptr);
  CHECK(
      at(root, "10/program/3")->get_parameter()->get_value_type()
      == ossia::val_type::IMPULSE);
}

TEST_CASE("a raw channel sends what it is written", "[mididevice][midi]")
{
  const auto lb = findLoopback();
  if(!lb)
  {
    WARN("no loopback MIDI port on this machine");
    SUCCEED();
    return;
  }

  raw_bidir w{*lb, GenericChannel{2, true}};

  w.param("2/on").push_value(std::vector<ossia::value>{60, 100});
  CHECK(waitFor([&] { return w.count({0x91, 60, 100}) == 1; }));

  // Writing the pair moves the node that addresses that one note, so the two
  // ways of saying it agree.
  CHECK(ossia::convert<int>(w.param("2/on/60").value()) == 100);

  w.param("2/on/64").push_value(80);
  CHECK(waitFor([&] { return w.count({0x91, 64, 80}) == 1; }));

  // A note on at no velocity is how most hardware releases a note.
  w.param("2/on/64").push_value(0);
  CHECK(waitFor([&] { return w.count({0x81, 64, 0}) == 1; }));

  w.param("2/off").push_value(std::vector<ossia::value>{60, 0});
  CHECK(waitFor([&] { return w.count({0x81, 60, 0}) == 1; }));

  w.param("2/control").push_value(std::vector<ossia::value>{7, 90});
  CHECK(waitFor([&] { return w.count({0xB1, 7, 90}) == 1; }));

  w.param("2/control/10").push_value(20);
  CHECK(waitFor([&] { return w.count({0xB1, 10, 20}) == 1; }));

  w.param("2/program").push_value(5);
  CHECK(waitFor([&] { return w.count({0xC1, 5}) == 1; }));

  w.param("2/program/9").push_value(ossia::impulse{});
  CHECK(waitFor([&] { return w.count({0xC1, 9}) == 1; }));

  w.param("2/pitchbend").push_value(0);
  CHECK(waitFor([&] { return w.count({0xE1, 0, 0}) == 1; }));

  w.param("2/pitchbend").push_value(16383);
  CHECK(waitFor([&] { return w.count({0xE1, 127, 127}) == 1; }));
}

TEST_CASE("a raw channel follows the wire", "[mididevice][midi]")
{
  const auto lb = findLoopback();
  if(!lb)
  {
    WARN("no loopback MIDI port on this machine");
    SUCCEED();
    return;
  }

  raw_bidir w{*lb, GenericChannel{5, true}};

  const auto list = [&](const std::string& path) {
    return ossia::convert<std::vector<ossia::value>>(w.param(path).value());
  };

  w.send({0x94, 60, 100});
  CHECK(waitFor([&] { return ossia::convert<int>(w.param("5/on/60").value()) == 100; }));
  CHECK(list("5/on") == std::vector<ossia::value>{60, 100});

  // A note on at no velocity arrives as the note off it is.
  w.send({0x94, 60, 0});
  CHECK(waitFor([&] { return list("5/off") == std::vector<ossia::value>{60, 0}; }));

  w.send({0xB4, 7, 42});
  CHECK(waitFor([&] { return ossia::convert<int>(w.param("5/control/7").value()) == 42; }));
  CHECK(list("5/control") == std::vector<ossia::value>{7, 42});

  w.send({0xC4, 11});
  CHECK(waitFor([&] { return ossia::convert<int>(w.param("5/program").value()) == 11; }));

  w.send({0xE4, 0x00, 0x60});
  CHECK(waitFor(
      [&] { return ossia::convert<int>(w.param("5/pitchbend").value()) == (0x60 << 7); }));

  // Another channel is not this one.
  w.send({0xB5, 7, 1});
  std::this_thread::sleep_for(std::chrono::milliseconds{150});
  CHECK(ossia::convert<int>(w.param("5/control/7").value()) == 42);
}

TEST_CASE("a level of the tree is a MIDI port of its own", "[mididevice][midi]")
{
  const auto lb = findLoopback();
  if(!lb)
  {
    WARN("no loopback MIDI port on this machine");
    SUCCEED();
    return;
  }

  raw_bidir w{*lb, std::vector<GenericChannel>{{4, false}, {9, false}}};

  auto& root = w.dev->get_root_node();
  auto* four = at(root, "4");
  REQUIRE(four);

  ossia::execution_state st;
  st.register_device(w.dev.get());
  st.apply_device_changes();

  auto in = std::make_unique<ossia::midi_inlet>();
  in->address = four;
  st.register_port(*in);

  // A tick empties the queue, so what arrived has to be kept as it comes.
  std::vector<libremidi::ump> got;
  const auto tick = [&] {
    st.begin_tick();
    auto& port = *in->target<ossia::midi_port>();
    port.messages.clear();
    st.copy_from_global_node(*four, *in);
    for(const auto& m : port.messages)
      got.push_back(m);
    return got.size();
  };

  // A loopback is shared, so anything already on it is drained and forgotten
  // before the messages this test is about are sent.
  std::this_thread::sleep_for(std::chrono::milliseconds{200});
  tick();
  got.clear();

  // Everything the wire carries on that channel, whether or not a control of
  // some description is reading the same message.
  w.send({0x93, 60, 100});
  w.send({0xB3, 7, 42});

  // Another channel is another device: this level does not hear it.
  w.send({0xB8, 7, 1});

  REQUIRE(waitFor([&] { return tick() >= 2; }, std::chrono::seconds{3}));

  // And nothing more: waiting for "at least" then checking "exactly" is what
  // tells two messages from three.
  std::this_thread::sleep_for(std::chrono::milliseconds{200});
  tick();
  REQUIRE(got.size() == 2);
  for(const auto& m : got)
    CHECK(m.get_channel() == 4);

  st.unregister_port(*in);

  // And the other way: what is written to a level goes out of the port.
  auto out = std::make_unique<ossia::midi_outlet>();
  out->address = four;
  out->scope = ossia::port::scope_t::global;

  libremidi::ump note;
  note.data[0] = 0x20932A64;
  out->target<ossia::midi_port>()->messages.push_back(note);
  out->write(st);

  CHECK(waitFor([&] { return w.count({0x93, 0x2A, 0x64}) == 1; }));

  // A level sends on its own channel whatever channel the message was written
  // on, so that a port reads back what it writes.
  out->target<ossia::midi_port>()->messages.clear();
  libremidi::ump elsewhere;
  elsewhere.data[0] = 0x20902B65;
  out->target<ossia::midi_port>()->messages.push_back(elsewhere);
  out->write(st);

  CHECK(waitFor([&] { return w.count({0x93, 0x2B, 0x65}) == 1; }));
  CHECK(w.count({0x90, 0x2B, 0x65}) == 0);

  // A port that propagates nothing sends nothing.
  out->scope = ossia::port::scope_t::none;
  out->target<ossia::midi_port>()->messages.clear();
  out->target<ossia::midi_port>()->messages.push_back(note);
  out->write(st);

  std::this_thread::sleep_for(std::chrono::milliseconds{200});
  CHECK(w.count({0x93, 0x2A, 0x64}) == 1);
}

TEST_CASE("a parameter number writes the value it means", "[mididevice][midi]")
{
  const auto lb = findLoopback();
  if(!lb)
  {
    WARN("no loopback MIDI port on this machine");
    SUCCEED();
    return;
  }

  auto inj = makeReceiver(*lb, R"_({
    "format": "score.midi-device/1", "model": "Synth",
    "controls": [
      {"name": "Cutoff", "kind": "knob", "direction": "in",
       "message": {"type": "nrpn", "channel": 1, "number": 1, "lsb": 2},
       "value": {"min": 0, "max": 16383}},
      {"name": "Depth", "kind": "knob", "direction": "in",
       "message": {"type": "nrpn", "channel": 1, "number": 1, "lsb": 3},
       "value": {"min": 0, "max": 127}}
    ]})_");
  auto& root = inj.dev->get_root_node();

  const auto select = [&](int lsb) {
    inj.send({0xB0, 99, 1});
    inj.send({0xB0, 98, (unsigned char)lsb});
  };

  // A 14-bit parameter whose device sends only the coarse half means that half
  // shifted up, not a value a hundred and twenty-eight times too small.
  select(2);
  inj.send({0xB0, 6, 64});
  CHECK(waitFor([&] { return valueOf(root, "Synth/Cutoff") == 8192; }));

  // And then the fine half completes it, without passing through anything else.
  inj.send({0xB0, 38, 3});
  CHECK(waitFor([&] { return valueOf(root, "Synth/Cutoff") == 8195; }));

  // A 7-bit parameter keeps the coarse half when the device sends both, rather
  // than ending on the fine one.
  select(3);
  inj.send({0xB0, 6, 100});
  CHECK(waitFor([&] { return valueOf(root, "Synth/Depth") == 100; }));
  inj.send({0xB0, 38, 0});
  std::this_thread::sleep_for(std::chrono::milliseconds{200});
  CHECK(valueOf(root, "Synth/Depth") == 100);

  // Selecting another parameter drops the data entry held for the last: a
  // fine-only write after it must not be given the previous coarse half.
  select(2);
  const int before = valueOf(root, "Synth/Cutoff");
  inj.send({0xB0, 38, 5});
  std::this_thread::sleep_for(std::chrono::milliseconds{200});
  CHECK(valueOf(root, "Synth/Cutoff") == before);
}

TEST_CASE("a coarse control change is latched, not consumed", "[mididevice][midi]")
{
  const auto lb = findLoopback();
  if(!lb)
  {
    WARN("no loopback MIDI port on this machine");
    SUCCEED();
    return;
  }

  auto inj = makeReceiver(*lb, R"_({
    "format": "score.midi-device/1", "model": "Fader",
    "controls": [
      {"name": "Level", "kind": "fader", "direction": "in",
       "message": {"type": "cc14", "channel": 1, "number": 1, "lsb": 33},
       "value": {"min": 0, "max": 16383}}
    ]})_");
  auto& root = inj.dev->get_root_node();

  inj.send({0xB0, 1, 64});
  inj.send({0xB0, 33, 0});
  CHECK(waitFor([&] { return valueOf(root, "Fader/Level") == 8192; }));

  // A slow move sends the coarse half once and then a run of fine halves
  // inside that step; every one of them is a value, not a message to drop.
  inj.send({0xB0, 33, 1});
  CHECK(waitFor([&] { return valueOf(root, "Fader/Level") == 8193; }));
  inj.send({0xB0, 33, 2});
  CHECK(waitFor([&] { return valueOf(root, "Fader/Level") == 8194; }));
}

TEST_CASE("a device need not have a level of its own", "[mididevice]")
{
  const auto found = anyOutput();
  if(!found)
  {
    WARN("no MIDI output port on this machine; tree not built");
    SUCCEED();
    return;
  }
  const auto& [api, out] = *found;

  // A model named the way an instrument patch names itself: the level it would
  // add is a sentence, and a port carrying one device does not need it.
  const auto doc = R"_({
    "format": "score.midi-device/1",
    "model": "07 2nd Violins Sus KS C0-G#0 Ni DIV B",
    "controls": [
      {"name": "Cutoff", "kind": "knob", "group": ["Filter"], "direction": "in",
       "message": {"type": "cc", "channel": 1, "number": 74}, "value": {}}
    ]})_";

  auto map = parseDeviceMap(doc);
  REQUIRE(map.has_value());

  SECTION("with the level, the description names a step of every address")
  {
    ProtocolSettings conf;
    conf.api = api;
    conf.output = out;
    conf.devices.push_back({*map, 1, true});

    auto dev = std::make_unique<ossia::net::generic_device>(
        makeProtocol(std::move(conf)), "kit");
    auto& root = dev->get_root_node();

    CHECK(at(root, "07 2nd Violins Sus KS C0-G_0 Ni DIV B/Filter/Cutoff"));
    CHECK(!at(root, "Filter/Cutoff"));
  }

  SECTION("without it, the controls sit on the root")
  {
    ProtocolSettings conf;
    conf.api = api;
    conf.output = out;
    conf.devices.push_back({*map, 1, false});

    auto dev = std::make_unique<ossia::net::generic_device>(
        makeProtocol(std::move(conf)), "kit");
    auto& root = dev->get_root_node();

    CHECK(at(root, "Filter/Cutoff"));
    CHECK(!at(root, "07 2nd Violins Sus KS C0-G_0 Ni DIV B"));

    // And the root is then the level that stands for the channel, so it still
    // works as a MIDI port of its own.
    CHECK(root.children().size() == 1);
  }
}
