/**
 * Tests for the `.midimap.json` reader.
 *
 * What is worth pinning is what the reader decides rather than transcribes: the
 * two spellings of a group, the three spellings of a channel, the bounds a
 * document omits because the message type already implies them, and the four
 * relative encodings, which are the one place where a wrong answer silently
 * turns a working knob into a broken one.
 *
 * Expected values come from the format specification (spec/FORMAT.md in the
 * midi-device-maps repository), never from a run of the code.
 */

#include <Protocols/MIDIDevices/MidiDeviceMap.hpp>

#include <catch2/catch_all.hpp>

#include <string>

using namespace Protocols::MIDIDevices;

namespace
{
//! A document with one control, whose fields the caller writes.
std::string doc(const std::string& control, const std::string& head = {})
{
  return R"_({"format":"score.midi-device/1","model":"Test")_" + head
         + R"_(,"controls":[)_" + control + "]}";
}

DeviceMap parse(const std::string& text)
{
  auto m = parseDeviceMap(text);
  REQUIRE(m.has_value());
  return *m;
}

const std::string minimal_control
    = R"_({"name":"Knob 1","kind":"knob","message":{"type":"cc","channel":1,"number":74},)_"
      R"_("value":{"mode":"absolute"}})_";
}

TEST_CASE("A document of another format is not read", "[midimap]")
{
  CHECK(!parseDeviceMap(R"({"format":"score.midi-device/2","controls":[]})"));
  CHECK(!parseDeviceMap(R"({"controls":[]})"));
  CHECK(!parseDeviceMap("not json at all"));
  CHECK(!parseDeviceMap(""));

  // A document with no control is still a document: it records that a source
  // exists and describes nothing.
  auto empty = parse(R"({"format":"score.midi-device/1","controls":[]})");
  CHECK(empty.controls.empty());
  CHECK(empty.warnings.empty());
}

TEST_CASE("A group is read from either spelling", "[midimap]")
{
  const auto one = [](const std::string& group) {
    auto m = parse(doc(
        R"_({"name":"K","message":{"type":"cc","number":1},"value":{},"group":)_" + group
        + "}"));
    REQUIRE(m.controls.size() == 1);
    return m.controls[0].group;
  };

  CHECK(one(R"("Encoders")") == std::vector<std::string>{"Encoders"});
  CHECK(one(R"("Bank B : Equalizer")") == std::vector<std::string>{"Bank B", "Equalizer"});

  // Whitespace around the separator is insignificant.
  CHECK(one(R"("Bank B:Equalizer")") == std::vector<std::string>{"Bank B", "Equalizer"});

  // The array states the levels outright, so a member keeps its own colon: a
  // real section heading is one level, not two.
  const std::vector<std::string> one_level{"Group 1: ( Pan 1 / CH 1 )"};
  CHECK(one(R"_(["Group 1: ( Pan 1 / CH 1 )"])_") == one_level);
  CHECK(one(R"(["Bank B","Equalizer"])") == std::vector<std::string>{"Bank B", "Equalizer"});
}

TEST_CASE("A channel is one, several, or the device's own", "[midimap]")
{
  const auto chans = [](const std::string& channel) {
    auto m = parse(doc(
        R"_({"name":"K","message":{"type":"cc","number":1,"channel":)_" + channel
        + R"_(},"value":{}})_"));
    REQUIRE(m.controls.size() == 1);
    return m.controls[0].message.channels;
  };

  CHECK(chans("3") == std::vector<int>{3});
  CHECK(chans("null").empty());
  CHECK(chans("[1,2,3]") == std::vector<int>{1, 2, 3});

  // "Every channel but 10", the shape an instrument's name set routinely takes.
  CHECK(chans("[1,2,3,4,5,6,7,8,9,11,12,13,14,15,16]").size() == 15);

  // A channel outside 1-16 addresses nothing, and the control goes with it: an
  // empty channel list means "whichever the device is set to", so reading a
  // typo as one would quietly widen the control to every channel.
  const auto dropped = [](const std::string& channel) {
    auto m = parse(doc(
        R"_({"name":"K","message":{"type":"cc","number":1,"channel":)_" + channel
        + R"_(},"value":{}})_"));
    return m.controls.empty() && m.warnings.size() == 1;
  };
  CHECK(dropped("17"));
  CHECK(dropped("0"));
  CHECK(dropped("[]"));
  CHECK(dropped("[0,17,99]"));

  // A list that names some real channels keeps them, and only them.
  CHECK(chans("[0,1,17]") == std::vector<int>{1});
}

TEST_CASE("Omitted bounds mean the message type's natural range", "[midimap]")
{
  CHECK(naturalRange(MessageType::CC) == std::pair{0, 127});
  CHECK(naturalRange(MessageType::Note) == std::pair{0, 127});
  CHECK(naturalRange(MessageType::Program) == std::pair{0, 127});
  CHECK(naturalRange(MessageType::CC14) == std::pair{0, 16383});
  CHECK(naturalRange(MessageType::NRPN) == std::pair{0, 16383});
  CHECK(naturalRange(MessageType::RPN) == std::pair{0, 16383});

  // Pitch bend is 14-bit unsigned like the others, centred on 8192 and marked
  // bipolar, rather than renumbered: bounds are the values on the wire.
  CHECK(naturalRange(MessageType::PitchBend) == std::pair{0, 16383});

  Value v;
  CHECK(resolvedRange(v, MessageType::CC) == std::pair{0, 127});
  v.max = 64;
  CHECK(resolvedRange(v, MessageType::CC) == std::pair{0, 64});
  v.min = 32;
  CHECK(resolvedRange(v, MessageType::CC) == std::pair{32, 64});
}

TEST_CASE("The four relative encodings disagree about the same byte", "[midimap]")
{
  // One step up and one step down, as each encoding writes them.
  CHECK(decodeRelative(1, Encoding::TwosComplement) == 1);
  CHECK(decodeRelative(127, Encoding::TwosComplement) == -1);

  CHECK(decodeRelative(1, Encoding::SignedBit) == 1);
  CHECK(decodeRelative(65, Encoding::SignedBit) == -1);

  CHECK(decodeRelative(65, Encoding::SignedBit2) == 1);
  CHECK(decodeRelative(1, Encoding::SignedBit2) == -1);

  CHECK(decodeRelative(65, Encoding::BinaryOffset) == 1);
  CHECK(decodeRelative(63, Encoding::BinaryOffset) == -1);

  // Neutral.
  CHECK(decodeRelative(0, Encoding::TwosComplement) == 0);
  CHECK(decodeRelative(0, Encoding::SignedBit) == 0);
  CHECK(decodeRelative(64, Encoding::BinaryOffset) == 0);

  // The byte that motivates the distinction: 65 is +1, -1 or +1 depending only
  // on which encoding the document names.
  CHECK(decodeRelative(65, Encoding::TwosComplement) == -63);
  CHECK(decodeRelative(65, Encoding::SignedBit) == -1);
  CHECK(decodeRelative(65, Encoding::SignedBit2) == 1);
  CHECK(decodeRelative(65, Encoding::BinaryOffset) == 1);
}

TEST_CASE("A relative control without an encoding is dropped", "[midimap]")
{
  auto m = parse(doc(
      R"_({"name":"K","message":{"type":"cc","number":1},"value":{"mode":"relative"}})_"));
  CHECK(m.controls.empty());
  REQUIRE(m.warnings.size() == 1);
  CHECK(m.warnings[0].find("relative") != std::string::npos);

  auto ok = parse(doc(
      R"_({"name":"K","message":{"type":"cc","number":1},)_"
      R"_("value":{"mode":"relative","encoding":"signed_bit_2"}})_"));
  REQUIRE(ok.controls.size() == 1);
  CHECK(ok.controls[0].value.mode == ValueMode::Relative);
  CHECK(ok.controls[0].value.encoding == Encoding::SignedBit2);
}

TEST_CASE("A malformed control costs itself, not the document", "[midimap]")
{
  auto m = parse(doc(
      minimal_control + R"_(,{"name":"Bad","message":{"type":"sysex"},"value":{}})_"
      + R"_(,{"message":{"type":"cc","number":2},"value":{}})_"));

  REQUIRE(m.controls.size() == 1);
  CHECK(m.controls[0].name == "Knob 1");
  CHECK(m.warnings.size() == 2);
}

TEST_CASE("A program change carries its bank and its patch names", "[midimap]")
{
  auto m = parse(doc(
      R"_({"name":"Program","kind":"parameter","direction":"out",)_"
      R"_("message":{"type":"program","channel":null,"bank":{"msb":0,"lsb":12}},)_"
      R"_("value":{"mode":"absolute","labels":[{"from":0,"to":0,"name":"Warm Pad"},)_"
      R"_({"from":1,"to":1,"name":"Bright Lead"}]}})_"));

  REQUIRE(m.controls.size() == 1);
  const auto& c = m.controls[0];
  CHECK(c.kind == Kind::Parameter);
  CHECK(c.direction == Direction::Out);
  CHECK(c.message.type == MessageType::Program);
  CHECK(c.message.channels.empty());
  CHECK(c.message.bank.msb == 0);
  CHECK(c.message.bank.lsb == 12);

  // A program change addresses nothing: its data byte is the value.
  CHECK(c.message.number == -1);

  REQUIRE(c.value.labels.size() == 2);
  CHECK(c.value.labels[0].name == "Warm Pad");
  CHECK(c.value.labels[1].from == 1);
}

TEST_CASE("A condition is one program or several", "[midimap]")
{
  const auto when = [](const std::string& w) {
    auto m = parse(doc(
        R"_({"name":"Snare","message":{"type":"note","number":38},"value":{},"when":)_" + w
        + "}"));
    REQUIRE(m.controls.size() == 1);
    return m.controls[0].when;
  };

  auto one = when(R"({"program":12,"bank":{"lsb":0}})");
  REQUIRE(one.size() == 1);
  CHECK(one[0].program == 12);
  CHECK(one[0].bank.lsb == 0);
  CHECK(one[0].bank.msb == -1);

  // The array exists because a drum kit's note names are routinely shared by
  // dozens of patches.
  auto many = when(R"([{"program":112},{"program":113},{"program":114}])");
  REQUIRE(many.size() == 3);
  CHECK(many[2].program == 114);
  CHECK(many[0].bank.empty());

  CHECK(when("{}").empty());
}

TEST_CASE("A note control addresses one note or a range", "[midimap]")
{
  auto single = parse(doc(
      R"_({"name":"Kick","message":{"type":"note","number":36},"value":{}})_"));
  REQUIRE(single.controls.size() == 1);
  CHECK(single.controls[0].message.number == 36);
  CHECK(!single.controls[0].message.hasRange());

  auto range = parse(doc(
      R"_({"name":"Slices","message":{"type":"note","range":{"from":12,"to":60}},"value":{}})_"));
  REQUIRE(range.controls.size() == 1);
  CHECK(range.controls[0].message.hasRange());
  CHECK(range.controls[0].message.rangeFrom == 12);
  CHECK(range.controls[0].message.rangeTo == 60);
}

TEST_CASE("A preset is a name, a sequence, or both", "[midimap]")
{
  const auto preset = [](const std::string& p) {
    return parse(doc(minimal_control, R"(,"preset":)" + p)).preset;
  };

  CHECK(preset(R"("Mode 1")").name == "Mode 1");
  CHECK(preset(R"("Mode 1")").sysex.empty());

  auto obj = preset(R"({"name":"User mode 1","sysex":"f0 00 20 6b 7f f7"})");
  CHECK(obj.name == "User mode 1");
  REQUIRE(obj.sysex.size() == 1);

  // A device that needs a sequence: sending only the first message enters
  // nothing.
  auto seq = preset(R"({"sysex":["f0 43 10 f7","f0 43 11 f7"]})");
  CHECK(seq.name.empty());
  CHECK(seq.sysex.size() == 2);

  // A byte that depends on the individual unit rather than the model.
  auto devid = preset(R"({"sysex":"f0 42 ?? 2b 4e f7"})");
  REQUIRE(devid.sysex.size() == 1);
  CHECK(devid.sysex[0].find("??") != std::string::npos);

  CHECK(preset(R"("")").empty());
}

TEST_CASE("Attribution survives the read", "[midimap]")
{
  auto m = parse(doc(
      minimal_control,
      R"_(,"source":{"format":"midnam","license":{"spdx":"NOASSERTION",)_"
      R"_("redistribution":"factual-documentation","notices":["Digidesign"]},)_"
      R"_("authors":[{"name":"Jane Doe","from":"git"},)_"
      R"_({"from":"declared","verbatim":"Mark of the Unicorn - converted from FreeMIDI"}]})_"));

  CHECK(m.source.format == "midnam");
  CHECK(m.source.license == "NOASSERTION");
  CHECK(m.source.redistribution == "factual-documentation");
  REQUIRE(m.source.notices.size() == 1);
  CHECK(m.source.notices[0] == "Digidesign");

  // A declared author may carry only `verbatim`: a string naming three parties
  // has no one name to pick.
  REQUIRE(m.source.authors.size() == 2);
  CHECK(m.source.authors[0] == "Jane Doe");
  CHECK(m.source.authors[1] == "Mark of the Unicorn - converted from FreeMIDI");
}

TEST_CASE("The document's own fields are read", "[midimap]")
{
  auto m = parse(
      R"_({"format":"score.midi-device/1")_"
      R"_(,"manufacturer":"Donner","model":"DMK25 SpacLine")_"
      R"_(,"requires":"All buttons must be set to mode 1.")_"
      R"_(,"description":"Bindings for the DMK25.")_"
      R"_(,"match":["Donner DMK25 SpacLine","DMK25 SpacLine"])_"
      R"_(,"controls":[]})_");

  CHECK(m.manufacturer == "Donner");
  CHECK(m.label() == "Donner: DMK25 SpacLine");
  CHECK(m.requirement == "All buttons must be set to mode 1.");
  CHECK(m.match.size() == 2);

  DeviceMap anonymous;
  anonymous.model = "Generic Remote";
  CHECK(anonymous.label() == "Generic Remote");
}

TEST_CASE("Value flags are recorded, not corrected", "[midimap]")
{
  auto m = parse(doc(
      R"_({"name":"Fader","message":{"type":"cc","number":7},)_"
      R"_("value":{"mode":"absolute","orientation":"bipolar","inverted":true,)_"
      R"_("pickup":true,"momentary":false,"sends":[0,127],"default":64}})_"));

  REQUIRE(m.controls.size() == 1);
  const auto& v = m.controls[0].value;
  CHECK(v.bipolar);
  CHECK(v.inverted);
  CHECK(v.pickup);
  REQUIRE(v.momentary.has_value());
  CHECK(*v.momentary == false);
  CHECK(v.sends == std::vector<int>{0, 127});
  CHECK(v.def == 64);

  // Omitted means unknown, which is not the same as false.
  auto silent = parse(doc(minimal_control));
  CHECK(!silent.controls[0].value.momentary.has_value());
  CHECK(!silent.controls[0].value.inverted);
}

TEST_CASE("The header is read without the controls", "[midimap]")
{
  const std::string head
      = R"_({"format":"score.midi-device/1","manufacturer":"Akai","model":"MPK 225",)_"
        R"_("preset":{"name":"Preset 6","sysex":["f0 47 ?? f7"]},)_"
        R"_("match":["Akai MPK 225","MPK 225"],)_";
  const std::string full = head + R"_("controls":[)_" + minimal_control + "]}";

  auto h = parseDeviceMapHeader(full);
  REQUIRE(h.has_value());
  CHECK(h->label() == "Akai: MPK 225");
  CHECK(h->preset.name == "Preset 6");
  REQUIRE(h->preset.sysex.size() == 1);
  CHECK(h->match.size() == 2);

  // The point of the header read: a prefix that stops anywhere inside the
  // controls is enough, so a listing never pays for an instrument's patch
  // names.
  // From one byte into the header onwards, so the cut lands inside `match`,
  // inside the `"controls"` key itself and everywhere between -- not only past
  // the point where the abort trigger is already complete.
  for(std::size_t n = 1; n < full.size(); n++)
  {
    auto partial = parseDeviceMapHeader(std::string_view{full}.substr(0, n));
    if(!partial)
      continue;

    // Whatever it reports having read must be what the document says, never a
    // fragment of it.
    INFO("prefix of " << n << " bytes");
    CHECK(partial->model == "MPK 225");
    CHECK(partial->manufacturer == "Akai");
    CHECK(partial->preset.name == "Preset 6");
  }

  // And once the controls begin, it must always succeed.
  for(std::size_t n = head.size() + 12; n < full.size(); n++)
  {
    INFO("prefix of " << n << " bytes");
    REQUIRE(parseDeviceMapHeader(std::string_view{full}.substr(0, n)).has_value());
  }

  // A prefix that stops before the header is done says so rather than
  // reporting half a device.
  CHECK(!parseDeviceMapHeader(std::string_view{full}.substr(0, 40)));
  CHECK(!parseDeviceMapHeader(R"({"format":"score.midi-device/2","controls":[]})"));

  // A document with no controls at all still has a header.
  auto bare = parseDeviceMapHeader(R"({"format":"score.midi-device/1","model":"X"})");
  REQUIRE(bare.has_value());
  CHECK(bare->model == "X");
}

TEST_CASE("a scale says what the numbers mean, not what is sent", "[midimap]")
{
  const auto scaleOf = [](const std::string& scale) {
    auto m = parse(doc(
        R"_({"name":"Pan","message":{"type":"cc","number":10},)_"
        R"_("value":{"mode":"absolute","scale":)_" + scale + "}}"));
    REQUIRE(m.controls.size() == 1);
    return m.controls[0].value.scale;
  };

  // The commonest case: a knob sends 0-127 and the parameter reads -64..+63.
  auto bipolar = scaleOf(R"({"min":-64,"max":63})");
  CHECK(bipolar.min == -64);
  CHECK(bipolar.max == 63);
  CHECK(bipolar.unit.empty());
  CHECK(!bipolar.empty());

  auto pct = scaleOf(R"({"min":0,"max":100,"unit":"%"})");
  CHECK(pct.unit == "%");

  // A scale whose ends the source names rather than numbers.
  auto named = scaleOf(R"({"from":"Left","to":"Right"})");
  CHECK(named.from == "Left");
  CHECK(named.to == "Right");
  CHECK(!named.min.has_value());
  CHECK(!named.empty());

  // Counting backwards is a scale, not an error: min may exceed max.
  auto reversed = scaleOf(R"({"min":100,"max":0})");
  CHECK(reversed.min == 100);
  CHECK(reversed.max == 0);

  // Absent, and present but saying nothing, are both "no scale".
  auto none = parse(doc(minimal_control));
  CHECK(none.controls[0].value.scale.empty());
  CHECK(scaleOf(R"({"unit":"dB"})").empty());

  // The wire range is untouched by it: a scale describes, it does not convert.
  auto m = parse(doc(
      R"_({"name":"Pan","message":{"type":"cc","number":10},)_"
      R"_("value":{"mode":"absolute","min":0,"max":127,)_"
      R"_("scale":{"min":-64,"max":63}}})_"));
  CHECK(resolvedRange(m.controls[0].value, MessageType::CC) == std::pair{0, 127});
}

TEST_CASE("an encoder's step survives the round trip", "[midimap]")
{
  for(auto e : {Encoding::TwosComplement, Encoding::SignedBit, Encoding::SignedBit2,
                Encoding::BinaryOffset})
  {
    INFO("encoding " << int(e));

    // What every encoding must agree on: one step each way, and standing still.
    CHECK(decodeRelative(encodeRelative(1, e), e) == 1);
    CHECK(decodeRelative(encodeRelative(-1, e), e) == -1);
    CHECK(decodeRelative(encodeRelative(0, e), e) == 0);

    for(int d = -63; d <= 63; d++)
    {
      const int byte = encodeRelative(d, e);
      INFO("delta " << d << " byte " << byte);
      REQUIRE(byte >= 0);
      REQUIRE(byte <= 127);
      CHECK(decodeRelative(byte, e) == d);
    }
  }

  // A step larger than one byte can carry is clamped, never wrapped: a wrap
  // would send the encoder the opposite way.
  for(auto e : {Encoding::TwosComplement, Encoding::SignedBit, Encoding::SignedBit2,
                Encoding::BinaryOffset})
  {
    CHECK(decodeRelative(encodeRelative(1000, e), e) > 0);
    CHECK(decodeRelative(encodeRelative(-1000, e), e) < 0);
  }
}

TEST_CASE("the two readers agree about the same bytes", "[midimap]")
{
  // Key order is not part of the format. A document that states its controls
  // before its own fields is unusual, not invalid, and must not become
  // invisible in a library for it.
  const std::string controlsFirst
      = R"_({"controls":[)_" + minimal_control
        + R"_(],"format":"score.midi-device/1","manufacturer":"Acme","model":"Zed"})_";

  auto head = parseDeviceMapHeader(controlsFirst);
  REQUIRE(head.has_value());
  CHECK(head->model == "Zed");
  CHECK(head->manufacturer == "Acme");

  auto full = parseDeviceMap(controlsFirst);
  REQUIRE(full.has_value());
  CHECK(full->model == "Zed");
  CHECK(full->controls.size() == 1);

  // A repeated key: rapidjson keeps the first, so the header reader must too,
  // or a document is listed under one identity and opens as another.
  const auto both = [](const std::string& doc) {
    auto h = parseDeviceMapHeader(doc);
    auto f = parseDeviceMap(doc);
    return std::pair{h.has_value(), f.has_value()};
  };

  CHECK(both(R"_({"format":"score.midi-device/1","format":"score.midi-device/2",)_"
             R"_("model":"X","controls":[]})_")
        == std::pair{true, true});

  CHECK(both(R"_({"format":"score.midi-device/2","format":"score.midi-device/1",)_"
             R"_("model":"X","controls":[]})_")
        == std::pair{false, false});

  auto dup = parseDeviceMapHeader(
      R"_({"format":"score.midi-device/1","model":"First","model":"Second",)_"
      R"_("controls":[]})_");
  REQUIRE(dup.has_value());
  CHECK(dup->model == "First");
}

TEST_CASE("a modifier layer is read, not discarded", "[midimap]")
{
  auto m = parse(doc(
      R"_({"name":"ShiftPlay","message":{"type":"cc","channel":1,"number":74},)_"
      R"_("value":{},"layer":{"name":"Shift","of":"Deck1/Play"}})_"));

  REQUIRE(m.controls.size() == 1);
  CHECK(m.controls[0].layer.name == "Shift");
  CHECK(m.controls[0].layer.of == "Deck1/Play");
  CHECK(!m.controls[0].layer.empty());

  // A modifier that selects a whole-surface mode pairs with no single control.
  auto whole = parse(doc(
      R"_({"name":"ShiftTouch","message":{"type":"cc","channel":1,"number":75},)_"
      R"_("value":{},"layer":{"name":"Shift"}})_"));
  REQUIRE(whole.controls.size() == 1);
  CHECK(whole.controls[0].layer.name == "Shift");
  CHECK(whole.controls[0].layer.of.empty());

  CHECK(parse(doc(minimal_control)).controls[0].layer.empty());
}
