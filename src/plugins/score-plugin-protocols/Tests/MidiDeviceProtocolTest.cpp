/**
 * Tests for the tree a `.midimap.json` description builds.
 *
 * What is worth pinning is the translation from the document to the tree:
 * groups becoming levels, a control's declared direction narrowed by the ports
 * that are actually open, the domain a control gets when the document omits its
 * bounds, and the `choice` node that appears beside a control whose values are
 * all named.
 *
 * A protocol needs a port, so these open a real one. When the machine has none
 * the test says so and passes rather than failing for the absence of hardware.
 */

#include <Protocols/MIDIDevices/MidiDeviceProtocol.hpp>

#include <ossia/network/base/node.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <catch2/catch_all.hpp>

#include <libremidi/libremidi.hpp>

#include <magic_enum/magic_enum.hpp>

#include <chrono>
#include <cstdlib>
#include <memory>
#include <thread>
#include <string>

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
  conf.channel = 1;
  conf.output = out;
  conf.map = *map;

  // Output only: nothing is ever received, so no node may claim to be readable.
  auto dev = std::make_unique<ossia::net::generic_device>(
      makeProtocol(std::move(conf)), "surface");
  auto& root = dev->get_root_node();

  SECTION("a group becomes a level, and an array member keeps its colon")
  {
    REQUIRE(at(root, "Strip 1/Fader 1"));

    // "Bank B : Equalizer" was written as one array member, so it is one level.
    REQUIRE(at(root, "Bank B : Equalizer/Knobs/Encoder 1"));

    // The string form nests on the colon, so this one is two levels.
    REQUIRE(at(root, "Osc 1/Shape/Wave"));

    // A control with no group sits at the root.
    REQUIRE(at(root, "Cutoff"));
  }

  SECTION("the open ports narrow what a control claims to do")
  {
    // Output only. A control the document calls `in` cannot be reached at all,
    // so it is readable-but-never-updated rather than writable: writing it
    // would put a message on the cable the device never said it listens to.
    auto* fader = at(root, "Strip 1/Fader 1");
    REQUIRE(fader);
    REQUIRE(fader->get_parameter());
    CHECK(fader->get_parameter()->get_access() == ossia::access_mode::GET);

    // `both` narrows to what the one open port can do.
    auto* cutoff = at(root, "Cutoff");
    REQUIRE(cutoff);
    CHECK(cutoff->get_parameter()->get_access() == ossia::access_mode::SET);

    // `out` is what an output port is for.
    auto* wave = at(root, "Osc 1/Shape/Wave");
    REQUIRE(wave);
    CHECK(wave->get_parameter()->get_access() == ossia::access_mode::SET);
  }

  SECTION("an omitted bound is the message type's natural range")
  {
    auto* fader = at(root, "Strip 1/Fader 1");
    REQUIRE(fader);
    CHECK(domainOf(*fader->get_parameter()) == std::pair{0, 127});

    // 14-bit, and unsigned: pitch bend is centred on 8192 rather than
    // renumbered.
    auto* bend = at(root, "Bend");
    REQUIRE(bend);
    CHECK(domainOf(*bend->get_parameter()) == std::pair{0, 16383});

    auto* cutoff = at(root, "Cutoff");
    REQUIRE(cutoff);
    CHECK(domainOf(*cutoff->get_parameter()) == std::pair{0, 16383});
  }

  SECTION("named values get a choice beside the number")
  {
    auto* wave = at(root, "Osc 1/Shape/Wave");
    REQUIRE(wave);

    auto* choice = wave->find_child("choice");
    REQUIRE(choice);
    REQUIRE(choice->get_parameter());
    CHECK(choice->get_parameter()->get_value_type() == ossia::val_type::STRING);

    // A control whose values are not named has no choice node.
    auto* fader = at(root, "Strip 1/Fader 1");
    REQUIRE(fader);
    CHECK(fader->find_child("choice") == nullptr);
  }

  SECTION("a relative encoder starts somewhere rather than nowhere")
  {
    auto* enc = at(root, "Bank B : Equalizer/Knobs/Encoder 1");
    REQUIRE(enc);

    // An encoder sends deltas, so the node holds the accumulated position and
    // has to have one before the first delta arrives.
    CHECK(enc->get_parameter()->value().valid());
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
    conf.channel = 1;
    conf.output = out;
    conf.map = *map;
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
    try
    {
      auto second = build();
    }
    catch(const std::exception&)
    {
      // Some backends allow only one writer; that is the backend's business.
    }
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
    conf.channel = 1;
    conf.output = out;
    conf.map = *map;
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
  conf.channel = 1;
  conf.input = lb.in;
  conf.map = *map;

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

  // Note 36 is Kick's own address.
  inj.send({0x90, 36, 100});
  CHECK(waitFor([&] { return valueOf(root, "Kick") == 100; }));
  CHECK(valueOf(root, "Snare") == 0);

  // Note 60 addresses none of the three: it is not 36, not 38, and outside
  // 12..24. Nothing may move.
  inj.send({0x90, 60, 100});
  std::this_thread::sleep_for(std::chrono::milliseconds{200});
  CHECK(valueOf(root, "Snare") == 0);
  CHECK(valueOf(root, "Slices") == 0);

  // A note inside the range is the range control's value.
  inj.send({0x90, 20, 100});
  CHECK(waitFor([&] { return valueOf(root, "Slices") == 20; }));
  CHECK(valueOf(root, "Snare") == 0);

  // A note-off for a note that never sounded must not release another pad.
  inj.send({0x80, 60, 0});
  std::this_thread::sleep_for(std::chrono::milliseconds{200});
  CHECK(valueOf(root, "Kick") == 100);

  inj.send({0x80, 36, 0});
  CHECK(waitFor([&] { return valueOf(root, "Kick") == 0; }));
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
  CHECK(waitFor([&] { return valueOf(root, "PressA") == 77; }));
  CHECK(valueOf(root, "PressB") == 0);
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
  CHECK(waitFor([&] { return valueOf(root, "Enc") == 3; }));

  inj.send({0xB0, 74, 127});
  CHECK(waitFor([&] { return valueOf(root, "Enc") == 2; }));

  // It cannot run past the end of its range.
  for(int i = 0; i < 8; i++)
    inj.send({0xB0, 74, 127});
  CHECK(waitFor([&] { return valueOf(root, "Enc") == 0; }));
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
    conf.channel = 1;
    if(in)
      conf.input = lb->in;
    if(out)
      conf.output = lb->out;
    conf.map = *map;

    auto dev = std::make_unique<ossia::net::generic_device>(
        makeProtocol(std::move(conf)), "narrow");
    auto* n = at(dev->get_root_node(), path);
    REQUIRE(n);
    REQUIRE(n->get_parameter());
    return n->get_parameter()->get_access();
  };

  // Input only: what the hardware sends is readable, and an instrument
  // parameter there is unreachable rather than writable.
  CHECK(access(true, false, "Strip 1/Fader 1") == ossia::access_mode::GET);
  CHECK(access(true, false, "Osc 1/Shape/Wave") == ossia::access_mode::GET);
  CHECK(access(true, false, "Cutoff") == ossia::access_mode::GET);

  // Both: each control gets exactly what it declares.
  CHECK(access(true, true, "Strip 1/Fader 1") == ossia::access_mode::GET);
  CHECK(access(true, true, "Osc 1/Shape/Wave") == ossia::access_mode::SET);
  CHECK(access(true, true, "Cutoff") == ossia::access_mode::BI);
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
  conf.channel = 1;
  conf.output = out;
  conf.map = *map;

  auto dev = std::make_unique<ossia::net::generic_device>(
      makeProtocol(std::move(conf)), "centre");

  // 14-bit bipolar: 8192, not 8191. The wire range is 0..16383 and the value
  // a pitch bend wheel rests at is its midpoint.
  auto* bend = at(dev->get_root_node(), "Bend");
  REQUIRE(bend);
  CHECK(ossia::convert<int>(bend->get_parameter()->value()) == 8192);
}
