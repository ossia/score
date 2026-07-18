// Unit tests for score-plugin-midi: MIDI message byte encoding / decoding,
// the Midi note model + process serialization round-trips, the ossia midi
// sequencing node (note-on / note-off emission), Patternist pattern parsing,
// and SMF (standard midi file) parsing edge cases (running status,
// note-off-as-note-on-velocity-0).
//
// The plugin is built with -fvisibility=hidden; Midi::Note and the
// serialization template specializations are not exported, so MidiNote.cpp,
// MidiProcess.cpp and PatternParsing.cpp are compiled directly into this test
// (see tests/unit/CMakeLists.txt).

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/TimeValue.hpp>

#include <Midi/MidiNote.hpp>
#include <Midi/MidiProcess.hpp>
#include <Patternist/PatternParsing.hpp>

#include <score/application/ApplicationComponents.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <core/application/ApplicationInterface.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/nodes/midi.hpp>

#include <libremidi/message.hpp>
#include <libremidi/reader.hpp>

#include <QGuiApplication>

#include <catch2/catch_test_macros.hpp>

#include <csignal>
#include <stdexcept>

namespace
{
// In SCORE_DEBUG builds, checkDelimiter() executes SCORE_BREAKPOINT (raises
// SIGTRAP) before throwing "Corrupt save file.". Ignore the signal while
// deliberately feeding corrupt data.
struct ScopedIgnoreSigtrap
{
  void (*prev)(int);
  ScopedIgnoreSigtrap()
      : prev{std::signal(SIGTRAP, SIG_IGN)}
  {
  }
  ~ScopedIgnoreSigtrap() { std::signal(SIGTRAP, prev); }
};

// Minimal application fixture: score's DataStream / JSON deserializers
// reference score::AppComponents() (a global set through
// score::ApplicationInterface) at construction, and Process::load_midi_outlet
// resolves the port factory through it.
struct TestApplication final : public score::ApplicationInterface
{
  // score::Entity's metadata (ColorRef -> Skin) requires a QGuiApplication;
  // run it on the offscreen platform so the test stays headless.
  struct GuiApp
  {
    int argc = 1;
    char arg0[5] = "test";
    char* argv[2] = {arg0, nullptr};
    QGuiApplication app;
    GuiApp()
        : app{(qputenv("QT_QPA_PLATFORM", "offscreen"), argc), argv}
    {
    }
  } m_gui;

  score::ApplicationComponentsData m_compData;
  score::ApplicationComponents m_components{m_compData};
  score::ApplicationSettings m_appSettings;
  score::DocumentList m_documents;
  std::vector<std::unique_ptr<score::SettingsDelegateModel>> m_settings;
  score::ApplicationContext m_ctx{m_appSettings, m_components, m_documents, m_settings};

  TestApplication()
  {
    m_instance = this;

    auto ports = std::make_unique<Process::PortFactoryList>();
    ports->insert(std::make_unique<Process::PortFactory_T<Process::MidiOutlet>>());
    m_compData.factories.emplace(ports->interfaceKey(), std::move(ports));
  }

  const score::ApplicationContext& context() const override { return m_ctx; }
  const score::ApplicationComponents& components() const override
  {
    return m_components;
  }
};

TestApplication& testApp()
{
  static TestApplication app;
  return app;
}

// Decode a MIDI 2.0 UMP channel-voice packet produced by ossia's midi node.
struct DecodedUmp
{
  uint8_t status{};
  uint8_t channel{};
  uint8_t note{};
  uint16_t velocity16{};
};

DecodedUmp decode(const libremidi::ump& u)
{
  return DecodedUmp{
      cmidi2_ump_get_status_code(u.data), cmidi2_ump_get_channel(u.data),
      cmidi2_ump_get_midi2_note_note(u.data),
      cmidi2_ump_get_midi2_note_velocity(u.data)};
}
}

TEST_CASE("libremidi channel-voice byte encoding", "[midi][message]")
{
  using libremidi::channel_events;
  using libremidi::message_type;

  SECTION("note on: status nibble 0x9, channel in low nibble")
  {
    const auto m = channel_events::note_on(1, 60, 100);
    REQUIRE(m.size() == 3);
    CHECK(m[0] == 0x90);
    CHECK(m[1] == 60);
    CHECK(m[2] == 100);
    CHECK(m.get_message_type() == message_type::NOTE_ON);
    CHECK(m.get_channel() == 1);
    CHECK(m.uses_channel(1));
    CHECK(!m.uses_channel(2));
    CHECK(m.is_note_on_or_off());
  }

  SECTION("channel boundaries: 1..16 map to low nibble 0..15, out-of-range clamps")
  {
    CHECK(channel_events::note_on(1, 0, 0)[0] == 0x90);
    CHECK(channel_events::note_on(16, 0, 0)[0] == 0x9F);
    CHECK(channel_events::note_on(16, 0, 0).get_channel() == 16);
    // Out-of-range channels are clamped, not wrapped
    CHECK(channel_events::note_on(0, 0, 0)[0] == 0x90);
    CHECK(channel_events::note_on(17, 0, 0)[0] == 0x9F);
  }

  SECTION("note off")
  {
    const auto m = channel_events::note_off(3, 64, 33);
    CHECK(m[0] == 0x82);
    CHECK(m[1] == 64);
    CHECK(m[2] == 33);
    CHECK(m.get_message_type() == message_type::NOTE_OFF);
    CHECK(m.get_channel() == 3);
    CHECK(m.is_note_on_or_off());
  }

  SECTION("note on with velocity 0 still has NOTE_ON status byte")
  {
    const auto m = channel_events::note_on(1, 60, 0);
    CHECK(m[0] == 0x90);
    CHECK(m[2] == 0);
    CHECK(m.get_message_type() == message_type::NOTE_ON);
  }

  SECTION("control change")
  {
    const auto m = channel_events::control_change(10, 7, 127);
    REQUIRE(m.size() == 3);
    CHECK(m[0] == 0xB9);
    CHECK(m[1] == 7);
    CHECK(m[2] == 127);
    CHECK(m.get_message_type() == message_type::CONTROL_CHANGE);
    CHECK(m.get_channel() == 10);
  }

  SECTION("program change is a two-byte message")
  {
    const auto m = channel_events::program_change(2, 42);
    REQUIRE(m.size() == 2);
    CHECK(m[0] == 0xC1);
    CHECK(m[1] == 42);
    CHECK(m.get_message_type() == message_type::PROGRAM_CHANGE);
    CHECK(m.get_channel() == 2);
  }

  SECTION("pitch bend: 14-bit value split into 7-bit LSB / MSB")
  {
    const auto center = channel_events::pitch_bend(1, 8192);
    REQUIRE(center.size() == 3);
    CHECK(center[0] == 0xE0);
    CHECK(center[1] == 0x00);
    CHECK(center[2] == 0x40);
    CHECK(center.get_message_type() == message_type::PITCH_BEND);

    const auto minimum = channel_events::pitch_bend(1, 0);
    CHECK(minimum[1] == 0x00);
    CHECK(minimum[2] == 0x00);

    const auto maximum = channel_events::pitch_bend(1, 16383);
    CHECK(maximum[1] == 0x7F);
    CHECK(maximum[2] == 0x7F);

    // 14-bit reassembly round-trips
    const int v = 0x1234;
    const auto m = channel_events::pitch_bend(4, v);
    CHECK(((m[2] & 0x7F) << 7 | (m[1] & 0x7F)) == v);
    CHECK(m.get_channel() == 4);
  }

  SECTION("meta events are not channel messages")
  {
    const auto m = libremidi::meta_events::end_of_track();
    CHECK(m.is_meta_event());
    CHECK(m.get_channel() == 0);
    CHECK(m.get_meta_event_type() == libremidi::meta_event_type::END_OF_TRACK);
  }
}

TEST_CASE("Midi note model", "[midi][model]")
{
  SECTION("NoteData fields and end()")
  {
    Midi::NoteData d{0.25, 0.5, 60, 100};
    CHECK(d.start() == 0.25);
    CHECK(d.duration() == 0.5);
    CHECK(d.end() == 0.75);
    CHECK(d.pitch() == 60);
    CHECK(d.velocity() == 100);
  }

  SECTION("NoteComparator orders by start time")
  {
    Midi::NoteData early{0.1, 0.2, 70, 90};
    Midi::NoteData late{0.5, 0.1, 50, 90};
    Midi::NoteComparator cmp;
    CHECK(cmp(early, late));
    CHECK(!cmp(late, early));
    CHECK(cmp(early, 0.4));
  }

  SECTION("Note scaling multiplies start and duration")
  {
    Midi::Note n{Id<Midi::Note>{1}, Midi::NoteData{0.2, 0.4, 65, 80}, nullptr};
    n.scale(0.5);
    CHECK(n.start() == 0.1);
    CHECK(n.duration() == 0.2);
    CHECK(n.pitch() == 65);
    CHECK(n.velocity() == 80);
  }
}

TEST_CASE("Midi note serialization round-trip", "[midi][serialization]")
{
  testApp();

  Midi::Note src{Id<Midi::Note>{7}, Midi::NoteData{0.25, 0.5, 61, 101}, nullptr};

  SECTION("DataStream")
  {
    const QByteArray arr = score::marshall<DataStream>(src);
    Midi::Note dst{DataStream::Deserializer{arr}, nullptr};
    CHECK(dst.id().val() == 7);
    CHECK(dst.start() == 0.25);
    CHECK(dst.duration() == 0.5);
    CHECK(dst.pitch() == 61);
    CHECK(dst.velocity() == 101);
  }

  SECTION("JSON")
  {
    JSONReader r;
    r.readFrom(src);
    const rapidjson::Document doc = readJson(r.toByteArray());
    Midi::Note dst{JSONObject::Deserializer{doc}, nullptr};
    CHECK(dst.id().val() == 7);
    CHECK(dst.start() == 0.25);
    CHECK(dst.duration() == 0.5);
    CHECK(dst.pitch() == 61);
    CHECK(dst.velocity() == 101);
  }
}

TEST_CASE("Midi::ProcessModel serialization round-trip", "[midi][serialization]")
{
  testApp();

  Midi::ProcessModel src{
      TimeVal::fromMsecs(5000), Id<Process::ProcessModel>{123}, nullptr};
  src.setChannel(11);
  src.setRange(48, 84);
  src.notes.add(
      new Midi::Note{Id<Midi::Note>{0}, Midi::NoteData{0., 0.25, 36, 64}, &src});
  src.notes.add(
      new Midi::Note{Id<Midi::Note>{1}, Midi::NoteData{0.5, 0.5, 84, 127}, &src});

  auto checkLoaded = [&](Midi::ProcessModel& dst) {
    CHECK(dst.channel() == 11);
    CHECK(dst.range() == std::pair<int, int>{48, 84});
    CHECK(dst.duration() == src.duration());

    REQUIRE(dst.outlet);
    CHECK(dst.outlet->id() == src.outlet->id());
    CHECK(dst.outlet->type() == Process::PortType::Midi);

    REQUIRE(dst.notes.size() == 2);
    auto& n0 = dst.notes.at(Id<Midi::Note>{0});
    CHECK(n0.start() == 0.);
    CHECK(n0.duration() == 0.25);
    CHECK(n0.pitch() == 36);
    CHECK(n0.velocity() == 64);
    auto& n1 = dst.notes.at(Id<Midi::Note>{1});
    CHECK(n1.start() == 0.5);
    CHECK(n1.duration() == 0.5);
    CHECK(n1.pitch() == 84);
    CHECK(n1.velocity() == 127);
  };

  // Processes are serialized polymorphically (through the static type
  // Process::ProcessModel): the visitor bundles [concrete key + entity header
  // + base data + concrete data]. Mirror what deserialize_interface does in
  // the process factories.
  SECTION("DataStream")
  {
    const QByteArray outer
        = score::marshall<DataStream>(static_cast<const Process::ProcessModel&>(src));

    DataStream::Deserializer des{outer};
    QByteArray bundle;
    des.stream() >> bundle;
    DataStream::Deserializer sub{bundle};

    SCORE_DEBUG_CHECK_DELIMITER2(sub);
    UuidKey<Process::ProcessModel> key;
    TSerializer<DataStream, UuidKey<Process::ProcessModel>>::writeTo(sub, key);
    SCORE_DEBUG_CHECK_DELIMITER2(sub);
    CHECK(key == Metadata<ConcreteKey_k, Midi::ProcessModel>::get());

    Midi::ProcessModel dst{sub, nullptr};
    checkLoaded(dst);
  }

  SECTION("JSON")
  {
    JSONReader r;
    r.readFrom(static_cast<const Process::ProcessModel&>(src));
    const rapidjson::Document doc = readJson(r.toByteArray());
    JSONObject::Deserializer des{doc};
    Midi::ProcessModel dst{des, nullptr};
    checkLoaded(dst);
  }
}

TEST_CASE("ossia midi node emits note-on / note-off from note data", "[midi][executor]")
{
  ossia::nodes::midi node{8};
  node.set_channel(5); // wire channel = 4 (0-based)

  // Note A: [100, 300), pitch 60, velocity 100
  node.add_note({ossia::time_value{100}, ossia::time_value{200}, 60, 100});
  // Note B: [250, 450), pitch 72, velocity 90
  node.add_note({ossia::time_value{250}, ossia::time_value{200}, 72, 90});

  ossia::execution_state st; // default: modelToSamplesRatio == 1
  ossia::exec_state_facade fac{&st};
  ossia::graph_node& base = node;

  auto& mp = *node.root_outputs()[0]->target<ossia::midi_port>();
  const ossia::time_signature sig{4, 4};

  // Tick 1: [0, 200) -> note A starts
  base.run(ossia::token_request{ossia::time_value{0}, ossia::time_value{200},
                                ossia::time_value{1000}, ossia::time_value{0}, 1., sig,
                                120.},
           fac);
  REQUIRE(mp.messages.size() == 1);
  {
    const auto m = decode(mp.messages[0]);
    CHECK(m.status == CMIDI2_STATUS_NOTE_ON);
    CHECK(m.channel == 4);
    CHECK(m.note == 60);
    CHECK(m.velocity16 / 0x200 == 100); // MIDI2 16-bit velocity downscales to 100
    CHECK(mp.messages[0].timestamp == 100); // note start relative to tick start
  }
  mp.messages.clear();

  // Tick 2: [200, 400) -> note A ends at 300, note B starts at 250
  base.run(ossia::token_request{ossia::time_value{200}, ossia::time_value{400},
                                ossia::time_value{1000}, ossia::time_value{0}, 1., sig,
                                120.},
           fac);
  REQUIRE(mp.messages.size() == 2);
  {
    const auto off = decode(mp.messages[0]);
    CHECK(off.status == CMIDI2_STATUS_NOTE_OFF);
    CHECK(off.channel == 4);
    CHECK(off.note == 60);
    CHECK(mp.messages[0].timestamp == 100); // 300 - 200

    const auto on = decode(mp.messages[1]);
    CHECK(on.status == CMIDI2_STATUS_NOTE_ON);
    CHECK(on.channel == 4);
    CHECK(on.note == 72);
    CHECK(on.velocity16 / 0x200 == 90);
    CHECK(mp.messages[1].timestamp == 50); // 250 - 200
  }
  mp.messages.clear();

  // Stop: the still-playing note B must be flushed with a note-off
  node.mustStop = true;
  base.run(ossia::token_request{ossia::time_value{400}, ossia::time_value{500},
                                ossia::time_value{1000}, ossia::time_value{0}, 1., sig,
                                120.},
           fac);
  REQUIRE(mp.messages.size() == 1);
  {
    const auto off = decode(mp.messages[0]);
    CHECK(off.status == CMIDI2_STATUS_NOTE_OFF);
    CHECK(off.channel == 4);
    CHECK(off.note == 72);
  }
}

TEST_CASE("Patternist pattern parsing", "[midi][pattern]")
{
  using namespace Patternist;

  SECTION("numeric lanes with x / - steps")
  {
    const auto pats = parsePatterns(QByteArray("36 x-x-\n38 -x-x\n"));
    REQUIRE(pats.size() == 1);
    const Pattern& p = pats[0];
    CHECK(p.length == 4);
    CHECK(p.division == 16);
    REQUIRE(p.lanes.size() == 2);
    // Lanes are sorted by descending note number
    CHECK(p.lanes[0].note == 38);
    CHECK(
        p.lanes[0].pattern
        == std::vector<Note>{Note::Rest, Note::Note, Note::Rest, Note::Note});
    CHECK(p.lanes[1].note == 36);
    CHECK(
        p.lanes[1].pattern
        == std::vector<Note>{Note::Note, Note::Rest, Note::Note, Note::Rest});
  }

  SECTION("drum names map to General MIDI notes")
  {
    const auto pats = parsePatterns(QByteArray("BD x---\nSD -x--\nCH --x-\n"));
    REQUIRE(pats.size() == 1);
    REQUIRE(pats[0].lanes.size() == 3);
    CHECK(pats[0].lanes[0].note == 42); // CH closed hihat
    CHECK(pats[0].lanes[1].note == 38); // SD snare
    CHECK(pats[0].lanes[2].note == 36); // BD bass drum
  }

  SECTION("all step characters: 0/- rest, 1/x/X/f/F note, 2 legato")
  {
    const auto pats = parsePatterns(QByteArray("36 -0x1XfF2\n"));
    REQUIRE(pats.size() == 1);
    REQUIRE(pats[0].lanes.size() == 1);
    CHECK(
        pats[0].lanes[0].pattern
        == std::vector<Note>{
            Note::Rest, Note::Rest, Note::Note, Note::Note, Note::Note, Note::Note,
            Note::Note, Note::Legato});
  }

  SECTION("repeated note starts a new pattern")
  {
    const auto pats = parsePatterns(QByteArray("36 x---\n36 --x-\n"));
    REQUIRE(pats.size() == 2);
    REQUIRE(pats[0].lanes.size() == 1);
    REQUIRE(pats[1].lanes.size() == 1);
    CHECK(
        pats[0].lanes[0].pattern
        == std::vector<Note>{Note::Note, Note::Rest, Note::Rest, Note::Rest});
    CHECK(
        pats[1].lanes[0].pattern
        == std::vector<Note>{Note::Rest, Note::Rest, Note::Note, Note::Rest});
  }

  SECTION("accent lane (AC) sorts after regular lanes: note 255 as signed char")
  {
    const auto pats = parsePatterns(QByteArray("BD x---\nAC ---x\n"));
    REQUIRE(pats.size() == 1);
    REQUIRE(pats[0].lanes.size() == 2);
    CHECK(pats[0].lanes[0].note == 36);
    CHECK(pats[0].lanes[1].note == 255);
  }

  SECTION("trailing comments after the steps are ignored")
  {
    const auto pats = parsePatterns(QByteArray("36 x-x- this is a kick\n"));
    REQUIRE(pats.size() == 1);
    REQUIRE(pats[0].lanes.size() == 1);
    CHECK(pats[0].lanes[0].pattern.size() == 4);
  }

  SECTION("mismatched lane lengths: lane dropped, but pattern length is updated")
  {
    // NOTE: current behavior quirk. The second lane is rejected because its
    // step count differs from the first lane, but p.length has already been
    // overwritten with the rejected lane's length (PatternParsing.cpp:125).
    const auto pats = parsePatterns(QByteArray("36 x---\n38 xx\n"));
    REQUIRE(pats.size() == 1);
    REQUIRE(pats[0].lanes.size() == 1);
    CHECK(pats[0].lanes[0].note == 36);
    CHECK(pats[0].lanes[0].pattern.size() == 4);
    CHECK(pats[0].length == 2); // documents the quirk: length != lane size
  }

  SECTION("hc means clap (39), not hi conga: second mapping is unreachable")
  {
    // PatternParsing.cpp:76 maps "cp"/"hc" to 39; the later "hc" -> 66 branch
    // at line 96 can never be reached. Documents current behavior.
    const auto pats = parsePatterns(QByteArray("HC x---\n"));
    REQUIRE(pats.size() == 1);
    REQUIRE(pats[0].lanes.size() == 1);
    CHECK(pats[0].lanes[0].note == 39);
  }

  SECTION("garbage input yields no patterns")
  {
    CHECK(parsePatterns(QByteArray("")).empty());
    CHECK(parsePatterns(QByteArray("hello world\nfoo bar baz\n")).empty());
    CHECK(parsePatterns(QByteArray("ZZ qqqq\n")).empty());
  }
}

namespace
{
// Minimal SMF format-0 file: 96 ticks per beat, one track.
std::vector<uint8_t> make_test_smf()
{
  std::vector<uint8_t> v;
  auto push = [&](std::initializer_list<uint8_t> l) { v.insert(v.end(), l); };

  // Header
  push({'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0, 96});

  std::vector<uint8_t> track{
      // delta 0: note on ch1, pitch 60, vel 100
      0x00, 0x90, 0x3C, 0x64,
      // delta 48: running status (no status byte): pitch 60, vel 0 == note off
      0x30, 0x3C, 0x00,
      // delta 0: running status: pitch 64, vel 80
      0x00, 0x40, 0x50,
      // delta 48: explicit note off ch1, pitch 64
      0x30, 0x80, 0x40, 0x00,
      // delta 0: end of track
      0x00, 0xFF, 0x2F, 0x00};

  push({'M', 'T', 'r', 'k', 0, 0, 0, (uint8_t)track.size()});
  v.insert(v.end(), track.begin(), track.end());
  return v;
}
}

TEST_CASE("SMF parsing: running status and vel-0 note-off", "[midi][smf]")
{
  libremidi::reader reader;
  const auto res = reader.parse(make_test_smf());
  REQUIRE(res != libremidi::reader::invalid);
  REQUIRE(reader.tracks.size() == 1);
  CHECK(reader.ticksPerBeat == 96);

  std::vector<libremidi::track_event> channel_events;
  for(const auto& ev : reader.tracks[0])
    if(!ev.m.empty() && !ev.m.is_meta_event())
      channel_events.push_back(ev);

  REQUIRE(channel_events.size() == 4);

  using libremidi::message_type;
  // Event 0: note on 60, vel 100
  CHECK(channel_events[0].tick == 0);
  CHECK(channel_events[0].m.get_message_type() == message_type::NOTE_ON);
  CHECK(channel_events[0].m.get_channel() == 1);
  CHECK(channel_events[0].m.bytes[1] == 60);
  CHECK(channel_events[0].m.bytes[2] == 100);

  // Event 1: running status reconstructed as a NOTE_ON with velocity 0
  CHECK(channel_events[1].tick == 48);
  CHECK(channel_events[1].m.get_message_type() == message_type::NOTE_ON);
  CHECK(channel_events[1].m.bytes[1] == 60);
  CHECK(channel_events[1].m.bytes[2] == 0);

  // Event 2: running status, second note
  CHECK(channel_events[2].tick == 0);
  CHECK(channel_events[2].m.get_message_type() == message_type::NOTE_ON);
  CHECK(channel_events[2].m.bytes[1] == 64);
  CHECK(channel_events[2].m.bytes[2] == 0x50);

  // Event 3: explicit note off
  CHECK(channel_events[3].tick == 48);
  CHECK(channel_events[3].m.get_message_type() == message_type::NOTE_OFF);
  CHECK(channel_events[3].m.bytes[1] == 64);
}

TEST_CASE("fuzz: malformed inputs are handled gracefully", "[midi][fuzz]")
{
  testApp();

  SECTION("pattern parser on binary garbage")
  {
    QByteArray garbage;
    uint32_t seed = 0xC0FFEE;
    for(int i = 0; i < 4096; i++)
    {
      seed = seed * 1664525u + 1013904223u;
      char c = char(seed >> 24);
      garbage.append(i % 37 == 0 ? '\n' : c);
    }
    const auto pats = Patternist::parsePatterns(garbage);
    // Whatever is (not) parsed, every produced pattern must be self-consistent.
    for(const auto& p : pats)
      for(const auto& lane : p.lanes)
        CHECK(!lane.pattern.empty());
    SUCCEED("no crash on garbage pattern input");
  }

  SECTION("SMF reader on corrupt data")
  {
    const std::vector<std::vector<uint8_t>> inputs{
        {},
        {'M', 'T', 'h'},
        {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0},
        {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0, 96, 'M', 'T', 'r', 'k', 0xFF,
         0xFF, 0xFF, 0xFF, 0x90},
        {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF}};

    for(const auto& bytes : inputs)
    {
      try
      {
        libremidi::reader r;
        (void)r.parse(bytes);
      }
      catch(...)
      {
        // exceptions are acceptable, crashes are not
      }
    }
    SUCCEED("no crash on corrupt SMF data");
  }

  SECTION("truncated DataStream note buffers throw instead of crashing")
  {
    // NB: this deliberately covers Midi::Note (IdentifiedObject) and
    // Midi::NoteData (plain struct). Entity-based objects such as
    // Midi::ProcessModel cannot be covered here: their deserializing
    // constructor is noexcept, and ModelMetadata's checkDelimiter() throw on
    // corrupt data then goes straight to std::terminate (crash-fast design).
    Midi::Note src{Id<Midi::Note>{3}, Midi::NoteData{0.1, 0.5, 60, 100}, nullptr};
    const QByteArray arr = score::marshall<DataStream>(src);

    ScopedIgnoreSigtrap guard;
    for(int len = 0; len < arr.size(); len += 2)
    {
      const QByteArray cut = arr.left(len);
      try
      {
        Midi::Note dst{DataStream::Deserializer{cut}, nullptr};
        // Partial reads may succeed with default values; that is fine:
        // velocity/pitch stay in the uint8 domain by construction.
      }
      catch(const std::exception&)
      {
        // "Corrupt save file." from checkDelimiter is the expected failure mode
      }

      // NoteData is delimiter-free: truncation must never throw and always
      // yields defined values.
      const Midi::NoteData d = score::unmarshall<Midi::NoteData>(cut);
      CHECK(d.duration() >= 0.);
    }
    SUCCEED("no crash on truncated note buffers");
  }
}
