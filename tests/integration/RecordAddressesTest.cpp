// "Record automations from here" and "Record messages from here": the
// addresses selected in the explorer are recorded by the engine from a point
// of a scenario, and placed in a new box there in one undoable step.
//
// What has to hold:
//  - while stopped, nothing records until an address gets a value; that value
//    starts playback from the point and is where the take starts;
//  - as automations: an automation for every number of the addresses that
//    moved (one for a float, one per axis of a vec2), bound to the address
//    and its index, spanning the values it got; values an automation cannot
//    play (strings) as the states of a scenario in the same box; an address
//    that got nothing is not placed;
//  - as messages: a scenario whose states send the values in order;
//  - undo removes the box, redo puts it back; undo is off while recording.

#include <State/Address.hpp>

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceInterface.hpp>

#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Process/Process.hpp>
#include <Process/State/MessageNode.hpp>

#include <Curve/Settings/CurveSettingsModel.hpp>

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Automation/AutomationModel.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Media/Sound/SoundModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Midi/MidiProcess.hpp>
#include <Midi/MidiNote.hpp>
#include <Protocols/MIDI/MIDIProtocolFactory.hpp>
#include <Protocols/MIDI/MIDISpecificSettings.hpp>
#include <QProcess>
#include <libremidi/libremidi.hpp>
#include <optional>
#include <QStandardPaths>
#include <ossia/audio/audio_parameter.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <Audio/Settings/Model.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <score/model/EntitySerialization.hpp>

#include <catch2/catch_approx.hpp>
#include <dr_wav.h>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>
#include <Recording/ApplicationPlugin.hpp>
#include <Recording/Session/RecordSession.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/common/destination_qualifiers.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QPointer>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Process.hpp>
#include <score_test/Project.hpp>

#include "../unit/FakeDeviceProtocol.hpp"

namespace
{
void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
    QApplication::processEvents(QEventLoop::AllEvents, 5);
}

template <typename F>
bool spin_until(F f, int ms = 10000)
{
  QElapsedTimer t;
  t.start();
  while(!f() && t.elapsed() < ms)
    QApplication::processEvents(QEventLoop::AllEvents, 5);
  return f();
}

//! A device of the explorer whose addresses the test sets
struct Addresses
{
  Explorer::DeviceDocumentPlugin& plug;
  QString name{QStringLiteral("rec")};

  Addresses(const score::GUIApplicationContext& ctx, score::Document& doc)
      : plug{doc.context().plugin<Explorer::DeviceDocumentPlugin>()}
  {
    score::test::fake::registerFakeProtocol(ctx);
    CommandDispatcher<>{doc.context().commandStack}.submit(new Explorer::Command::LoadDevice{
        plug, Device::Node{score::test::fake::fakeSettings(name, {}), nullptr}});
    REQUIRE(node());
  }

  Device::Node* node() const
  {
    for(auto& n : plug.rootNode())
      if(n.get<Device::DeviceSettings>().name == name)
        return &n;
    return nullptr;
  }

  State::Address add(const QString& addr, ossia::value v)
  {
    auto* dev = node();
    REQUIRE(dev);
    Device::AddressSettings as;
    as.name = addr;
    as.value = std::move(v);
    as.ioType = ossia::access_mode::BI;
    plug.updateProxy.addAddress(Device::NodePath{*dev}, as, dev->childCount());
    return State::Address{name, {addr}};
  }

  void push(const State::Address& a, ossia::value v)
  {
    auto* dev = plug.list().findDevice(name);
    REQUIRE(dev);
    REQUIRE(dev->getDevice());
    auto* n = ossia::net::find_node(
        dev->getDevice()->get_root_node(), "/" + a.path.front().toStdString());
    REQUIRE(n);
    REQUIRE(n->get_parameter());
    n->get_parameter()->push_value(std::move(v));
  }
};

std::vector<Automation::ProcessModel*> automations(const Scenario::IntervalModel& box)
{
  std::vector<Automation::ProcessModel*> r;
  for(auto& p : box.processes)
    if(auto* a = qobject_cast<Automation::ProcessModel*>(&p))
      r.push_back(a);
  return r;
}

Scenario::ProcessModel* subScenario(const Scenario::IntervalModel& box)
{
  for(auto& p : box.processes)
    if(auto* s = qobject_cast<Scenario::ProcessModel*>(&p))
      return s;
  return nullptr;
}

//! The values the states of a scenario send, in time order
std::vector<ossia::value> sent(const Scenario::ProcessModel& sc)
{
  std::vector<std::pair<TimeVal, ossia::value>> v;
  for(auto& st : sc.states)
  {
    const auto date = sc.event(st.eventId()).date();
    for(const auto& m : Process::flatten(st.messages().rootNode()))
      v.emplace_back(date, m.value);
  }
  std::stable_sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.first < b.first; });
  std::vector<ossia::value> r;
  for(auto& e : v)
    r.push_back(e.second);
  return r;
}
}

TEST_CASE(
    "Recording addresses as automations starts on the first value and places a box",
    "[recording][addresses]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    Addresses dev{ctx, *doc};
    const auto x = dev.add("rec_x", 0.f);
    const auto v = dev.add("rec_v", ossia::vec2f{0.f, 0.f});
    const auto s = dev.add("rec_s", std::string{});
    const auto still = dev.add("rec_still", 3.f);
    spin(100);

    auto& curves = ctx.settings<Curve::Settings::Model>();
    curves.setPlayWhileRecording(true);
    auto& rec = ctx.guiApplicationPlugin<Recording::ApplicationPlugin>();
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();

    const auto boxes_before = scenar.intervals.size();
    const int commands_before = doc->commandStack().currentIndex();
    const Scenario::Point pt{TimeVal::fromMsecs(1000), 0.3};
    auto* session = rec.recordAddresses(
        scenar, pt, {x, v, s, still}, Recording::AddressPlacement::Automations);
    REQUIRE(session);
    REQUIRE(session->sources() == 4);

    // Nothing plays until an address gets a value
    spin(300);
    CHECK(!exec.isPlaying());
    bool finished = false;
    // The session is gone once its takes are placed: what it made is kept here
    QPointer<Scenario::IntervalModel> placed_box;
    std::vector<Recording::RecordSession::Take> takes;
    QObject::connect(session, &Recording::RecordSession::finished, [&] {
      finished = true;
      placed_box = session->placedBox();
      takes = session->takes();
    });

    dev.push(x, 0.5f);
    REQUIRE(spin_until([&] { return exec.isPlaying(); }));
    spin(300);
    dev.push(x, 0.2f);
    dev.push(v, ossia::vec2f{1.f, 2.f});
    dev.push(s, std::string{"a"});
    spin(200);
    dev.push(x, 0.9f);
    dev.push(s, std::string{"b"});
    spin(300);

    rec.stopSession();
    REQUIRE(spin_until([&] { return finished; }));
    engine.execution().request_stop();
    spin(200);

    REQUIRE(scenar.intervals.size() == boxes_before + 1);
    auto* box = placed_box.data();
    REQUIRE(box);
    CHECK(box->parent() == &scenar);
    // Where playback started, at the point
    CHECK(std::abs((box->date() - pt.date).msec()) < 50.);
    CHECK(box->duration.defaultDuration().msec() > 500.);

    // x, and v's two axes; nothing for the address that did not move
    const auto autos = automations(*box);
    REQUIRE(autos.size() == 3);
    int x_count = 0, v0 = 0, v1 = 0;
    for(auto* a : autos)
    {
      const auto& addr = a->address();
      if(addr.address == x && addr.qualifiers.get().accessors.empty())
      {
        x_count++;
        CHECK(std::abs(a->min() - 0.2) < 1e-6);
        CHECK(std::abs(a->max() - 0.9) < 1e-6);
      }
      else if(addr.address == v && addr.qualifiers.get().accessors.size() == 1)
      {
        (addr.qualifiers.get().accessors[0] == 0 ? v0 : v1)++;
      }
      CHECK(addr.address != still);
    }
    CHECK(x_count == 1);
    CHECK(v0 == 1);
    CHECK(v1 == 1);

    // The strings, as messages, in order
    auto* sc = subScenario(*box);
    REQUIRE(sc);
    const auto vals = sent(*sc);
    REQUIRE(vals.size() >= 2);
    CHECK(vals[vals.size() - 2] == ossia::value{std::string{"a"}});
    CHECK(vals.back() == ossia::value{std::string{"b"}});

    // One undoable step
    REQUIRE(doc->commandStack().currentIndex() == commands_before + 1);
    doc->commandStack().undo();
    CHECK(scenar.intervals.size() == boxes_before);
    doc->commandStack().redo();
    CHECK(scenar.intervals.size() == boxes_before + 1);
  });
}

TEST_CASE(
    "Recording addresses as messages places their values as states",
    "[recording][addresses]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    Addresses dev{ctx, *doc};
    const auto x = dev.add("msg_x", 0);
    spin(100);

    ctx.settings<Curve::Settings::Model>().setPlayWhileRecording(true);
    auto& rec = ctx.guiApplicationPlugin<Recording::ApplicationPlugin>();
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();

    auto* session = rec.recordAddresses(
        scenar, {TimeVal::zero(), 0.5}, {x}, Recording::AddressPlacement::Messages);
    REQUIRE(session);
    bool finished = false;
    // The session is gone once its takes are placed: what it made is kept here
    QPointer<Scenario::IntervalModel> placed_box;
    std::vector<Recording::RecordSession::Take> takes;
    QObject::connect(session, &Recording::RecordSession::finished, [&] {
      finished = true;
      placed_box = session->placedBox();
      takes = session->takes();
    });

    dev.push(x, 1);
    REQUIRE(spin_until([&] { return exec.isPlaying(); }));
    spin(200);
    dev.push(x, 2);
    spin(200);
    dev.push(x, 3);
    spin(200);

    // Stopping playback ends it too
    engine.execution().request_stop();
    REQUIRE(spin_until([&] { return finished; }));

    auto* box = placed_box.data();
    REQUIRE(box);
    CHECK(automations(*box).empty());
    auto* sc = subScenario(*box);
    REQUIRE(sc);
    const auto vals = sent(*sc);
    REQUIRE(vals.size() == 3);
    CHECK(vals[0] == ossia::value{1});
    CHECK(vals[1] == ossia::value{2});
    CHECK(vals[2] == ossia::value{3});
  });
}

TEST_CASE("A recording of addresses stopped before any value places nothing", "[recording][addresses]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    Addresses dev{ctx, *doc};
    const auto x = dev.add("none_x", 0.f);
    spin(100);

    auto& rec = ctx.guiApplicationPlugin<Recording::ApplicationPlugin>();
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    const auto boxes_before = scenar.intervals.size();
    const int commands_before = doc->commandStack().currentIndex();
    auto* session = rec.recordAddresses(
        scenar, {TimeVal::zero(), 0.5}, {x}, Recording::AddressPlacement::Automations);
    REQUIRE(session);
    bool finished = false;
    // The session is gone once its takes are placed: what it made is kept here
    QPointer<Scenario::IntervalModel> placed_box;
    std::vector<Recording::RecordSession::Take> takes;
    QObject::connect(session, &Recording::RecordSession::finished, [&] {
      finished = true;
      placed_box = session->placedBox();
      takes = session->takes();
    });
    spin(200);
    rec.stopSession();
    REQUIRE(spin_until([&] { return finished; }));
    CHECK(!exec.isPlaying());
    CHECK(scenar.intervals.size() == boxes_before);
    CHECK(doc->commandStack().currentIndex() == commands_before);
    // Another can start
    CHECK(spin_until([&] { return rec.session() == nullptr; }));
  });
}

TEST_CASE("An audio address is recorded to a sound in the new box", "[recording][addresses][audio]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    auto* audio = plug.list().audioDevice();
    REQUIRE(audio);
    const State::Address out{audio->name(), {"out", "main"}};

    auto& rec = ctx.guiApplicationPlugin<Recording::ApplicationPlugin>();
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    const auto boxes_before = scenar.intervals.size();

    auto* session = rec.recordAddresses(
        scenar, {TimeVal::fromMsecs(500), 0.4}, {out},
        Recording::AddressPlacement::Automations);
    REQUIRE(session);
    REQUIRE(session->sources() == 1);
    bool finished = false;
    // The session is gone once its takes are placed: what it made is kept here
    QPointer<Scenario::IntervalModel> placed_box;
    std::vector<Recording::RecordSession::Take> takes;
    QObject::connect(session, &Recording::RecordSession::finished, [&] {
      finished = true;
      placed_box = session->placedBox();
      takes = session->takes();
    });
    // A sound sends no first message: it records from the point at once
    REQUIRE(spin_until([&] { return exec.isPlaying(); }));
    spin(1000);
    rec.stopSession();
    REQUIRE(spin_until([&] { return finished; }));
    engine.execution().request_stop();
    spin(200);

    REQUIRE(scenar.intervals.size() == boxes_before + 1);
    auto* box = placed_box.data();
    REQUIRE(box);
    CHECK(std::abs((box->date() - TimeVal::fromMsecs(500)).msec()) < 60.);
    Process::ProcessModel* sound{};
    for(auto& p : box->processes)
      if(p.concreteKey() == Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get())
        sound = &p;
    REQUIRE(sound);
    CHECK(!sound->loops());
    REQUIRE(takes.size() == 1);
    CHECK(QFile::exists(takes[0].file));
    CHECK(takes[0].file.endsWith(".wav"));
  });
}

TEST_CASE("Addresses recorded into a box: its processes, from where they started", "[recording][addresses][box]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Addresses dev{ctx, *doc};
    const auto x = dev.add("box_x", 0.f);
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    auto* audio = plug.list().audioDevice();
    REQUIRE(audio);
    const State::Address out{audio->name(), {"out", "main"}};
    spin(100);

    // The root of the document, as its node view at the centre shows it
    auto& box = score::test::base_interval(*doc);
    const auto before = box.processes.size();

    auto& rec = ctx.guiApplicationPlugin<Recording::ApplicationPlugin>();
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    engine.execution().request_play_global(true);
    REQUIRE(spin_until([&] { return exec.isPlaying(); }));
    spin(1000);

    auto* session = rec.recordAddressesInto(box, {x, out}, Recording::AddressPlacement::Automations);
    REQUIRE(session);
    REQUIRE(session->sources() == 2);
    bool finished = false;
    std::vector<Recording::RecordSession::Take> takes;
    QObject::connect(session, &Recording::RecordSession::finished, [&] {
      finished = true;
      takes = session->takes();
    });
    spin(300);
    dev.push(x, 0.25f);
    spin(300);
    dev.push(x, 0.75f);
    spin(300);
    rec.stopSession();
    REQUIRE(spin_until([&] { return finished; }));
    engine.execution().request_stop();
    spin(200);

    // An automation and a sound, in the box itself
    REQUIRE(box.processes.size() == before + 2);
    Automation::ProcessModel* autom{};
    Process::ProcessModel* sound{};
    for(auto& p : box.processes)
    {
      if(auto* a = qobject_cast<Automation::ProcessModel*>(&p))
        autom = a;
      else if(p.concreteKey() == Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get())
        sound = &p;
    }
    REQUIRE(autom);
    REQUIRE(sound);
    CHECK(autom->address().address == x);
    CHECK(std::abs(autom->max() - 0.75) < 1e-6);
    // Started a second in: their content too
    CHECK(autom->startOffset() < TimeVal::fromMsecs(-900));
    CHECK(sound->startOffset() < TimeVal::fromMsecs(-900));
    CHECK(!sound->loops());

    // One step
    doc->commandStack().undo();
    CHECK(box.processes.size() == before);
  });
}

namespace
{
void write_dc(const QString& path, int rate, int frames, float value)
{
  drwav_data_format fmt{};
  fmt.container = drwav_container_riff;
  fmt.format = DR_WAVE_FORMAT_IEEE_FLOAT;
  fmt.channels = 1;
  fmt.sampleRate = drwav_uint32(rate);
  fmt.bitsPerSample = 32;
  drwav wav;
  REQUIRE(drwav_init_file_write(&wav, path.toStdString().c_str(), &fmt, nullptr));
  std::vector<float> data(frames, value);
  REQUIRE(drwav_write_pcm_frames(&wav, frames, data.data()) == drwav_uint64(frames));
  drwav_uninit(&wav);
}
}

TEST_CASE("A path received by a Sound's File inlet plays that file, the document keeping its own", "[recording][sound][file-inlet]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const int rate = ctx.settings<Audio::Settings::Model>().getRate();
    const QString a = dir.filePath("quiet.wav");
    const QString b = dir.filePath("loud.wav");
    write_dc(a, rate, rate * 20, 0.25f);
    write_dc(b, rate, rate * 20, 0.75f);

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    Addresses dev{ctx, *doc};
    const auto path = dev.add("path", ossia::value{std::string{}});

    auto& scenar = score::test::base_scenario(*doc);
    Media::Sound::ProcessModel* sound{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
      auto& box = m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(10000), 0.1});
      sound = static_cast<Media::Sound::ProcessModel*>(m.createProcessInNewSlot(
          box, Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get(), a));
      m.commit();
    }
    REQUIRE(sound);
    REQUIRE(sound->fileInlet);
    REQUIRE(sound->inlets().size() == 1);
    sound->fileInlet->setAddress(State::AddressAccessor{path});

    // What it plays, recorded
    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    Recording::RecordOptions opt;
    opt.target = Recording::RecordOptions::Target::FileOnly;
    Recording::RecordSession session{doc->context(), opt};
    REQUIRE(session.addSource(*sound->outlet));
    bool finished = false;
    QObject::connect(&session, &Recording::RecordSession::finished, [&] { finished = true; });
    REQUIRE(session.start());
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    REQUIRE(spin_until([&] { return exec.isPlaying(); }));
    spin(800);
    dev.push(path, ossia::value{b.toStdString()});
    spin(1200);
    session.stop();
    REQUIRE(spin_until([&] { return finished; }));
    engine.execution().request_stop();
    spin(200);

    REQUIRE(session.takes().size() == 1);
    drwav wav;
    REQUIRE(drwav_init_file(&wav, session.takes()[0].file.toStdString().c_str(), nullptr));
    const int ch = wav.channels;
    std::vector<float> v(std::size_t(wav.totalPCMFrameCount) * ch);
    drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, v.data());
    drwav_uninit(&wav);
    const std::size_t frames = v.size() / ch;
    REQUIRE(frames > std::size_t(rate));
    // The document's file first, then the one received
    CHECK(std::abs(v[(rate / 4) * ch]) == Catch::Approx(0.25).margin(0.02));
    CHECK(std::abs(v[(frames - rate / 4) * ch]) == Catch::Approx(0.75).margin(0.02));
    // The document is as it was
    CHECK(sound->userFilePath() == a);

    // Documents from before the inlet load with one
    JSONReader r;
    r.readFrom(static_cast<const Process::ProcessModel&>(*sound));
    auto json = readJson(r.toByteArray());
    REQUIRE(json.HasMember("FileInlet"));
    json.RemoveMember("FileInlet");
    JSONWriter w{json};
    auto old = std::make_unique<Media::Sound::ProcessModel>(w, sound->parent());
    REQUIRE(old->fileInlet);
    CHECK(old->inlets().size() == 1);
    CHECK(old->userFilePath() == a);
  });
}

namespace
{
//! A 16-bit AIFF, which the Sound process reads in memory where it maps a WAV
void write_dc_aiff(const QString& path, int rate, int frames, double value)
{
  QByteArray d;
  const auto be = [&](quint64 v, int bytes) {
    for(int i = bytes - 1; i >= 0; i--)
      d.append(char((v >> (8 * i)) & 0xFF));
  };
  d.append("FORM");
  be(4 + 8 + 18 + 8 + 8 + quint64(frames) * 2, 4);
  d.append("AIFF");
  d.append("COMM");
  be(18, 4);
  be(1, 2);
  be(quint64(frames), 4);
  be(16, 2);
  // The rate as an 80-bit extended float
  int exponent = 16383 + 63;
  quint64 mantissa = quint64(rate);
  while(!(mantissa & (quint64{1} << 63)))
  {
    mantissa <<= 1;
    exponent--;
  }
  be(quint64(exponent), 2);
  be(mantissa, 8);
  d.append("SSND");
  be(8 + quint64(frames) * 2, 4);
  be(0, 4);
  be(0, 4);
  const auto sample = quint16(qint16(value * 32767.));
  for(int i = 0; i < frames; i++)
    be(sample, 2);
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  REQUIRE(f.write(d) == d.size());
}
}

TEST_CASE("A Sound whose file changes kind while playing keeps the cable into its File inlet", "[recording][sound][file-inlet][cable]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const int rate = ctx.settings<Audio::Settings::Model>().getRate();
    const QString a = dir.filePath("mapped.wav");
    const QString b = dir.filePath("in-memory.aiff");
    write_dc(a, rate, rate * 20, 0.25f);
    write_dc_aiff(b, rate, rate * 20, 0.75);

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    auto& sm = score::IDocument::get<Scenario::ScenarioDocumentModel>(*doc);
    Media::Sound::ProcessModel* sound{};
    Automation::ProcessModel* autom{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
      auto& box = m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(10000), 0.1});
      sound = static_cast<Media::Sound::ProcessModel*>(m.createProcessInNewSlot(
          box, Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get(), a));
      autom = static_cast<Automation::ProcessModel*>(m.createProcessInNewSlot(
          box, Metadata<ConcreteKey_k, Automation::ProcessModel>::get(), {}));
      REQUIRE(sound);
      REQUIRE(autom);
      m.createCable(sm, *autom->outlet, *sound->fileInlet, Process::CableType::ImmediateGlutton);
      m.commit();
    }
    REQUIRE(sound->fileInlet->cables().size() == 1);
    const auto cable = sound->fileInlet->cables()[0].find(doc->context()).id();
    REQUIRE(sound->outlet->cables().empty());

    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    engine.execution().request_play_global(true);
    REQUIRE(spin_until([&] { return exec.isPlaying(); }));
    spin(500);

    // The cable's edge ends at the node the inlet is registered with
    auto& setup = exec.context().setup;
    const auto edge_reaches_inlet = [&] {
      auto e = setup.m_cables.find(cable);
      auto i = setup.inlets.find(sound->fileInlet.get());
      return e != setup.m_cables.end() && i != setup.inlets.end() && e->second
             && e->second->in_node == i->second.first;
    };
    REQUIRE(edge_reaches_inlet());
    const auto before = setup.inlets.find(sound->fileInlet.get())->second.first;

    // Another kind of file: another node plays it
    sound->setFile(b);
    REQUIRE(spin_until([&] {
      auto i = setup.inlets.find(sound->fileInlet.get());
      return i != setup.inlets.end() && i->second.first != before;
    }));
    spin(300);
    CHECK(edge_reaches_inlet());

    engine.execution().request_stop();
    spin(200);
  });
}

TEST_CASE("A file chosen for a Sound takes over from the one its File control asked for", "[recording][sound][file-inlet]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const int rate = ctx.settings<Audio::Settings::Model>().getRate();
    const QString a = dir.filePath("first.wav");
    const QString b = dir.filePath("from-control.wav");
    const QString c = dir.filePath("chosen.wav");
    write_dc(a, rate, rate * 2, 0.25f);
    write_dc(b, rate, rate * 2, 0.5f);
    write_dc(c, rate, rate * 2, 0.75f);

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    Media::Sound::ProcessModel* sound{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
      auto& box = m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(10000), 0.1});
      sound = static_cast<Media::Sound::ProcessModel*>(m.createProcessInNewSlot(
          box, Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get(), a));
      m.commit();
    }
    REQUIRE(sound);
    sound->fileInlet->setValue(b.toStdString());
    // What the command changing its file does
    sound->setFile(c);
    CHECK(sound->userFilePath() == c);
    const auto* asked = sound->fileInlet->value().target<std::string>();
    REQUIRE(asked);
    CHECK(asked->empty());
  });
}

TEST_CASE("A sound card input is recorded earlier by the latency set", "[recording][addresses][latency]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // Two input channels: the audio engine starts again with them
    auto& audio_settings = ctx.settings<Audio::Settings::Model>();
    if(audio_settings.getDefaultIn() < 2)
    {
      audio_settings.setDefaultIn(2);
      spin(2000);
    }
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    auto* audio = plug.list().audioDevice();
    REQUIRE(audio);
    auto* odev = audio->getDevice();
    REQUIRE(odev);
    auto* in = ossia::net::find_node(odev->get_root_node(), "/in/main");
    auto* in_param = in ? dynamic_cast<ossia::mapped_audio_parameter*>(in->get_parameter())
                        : nullptr;
    if(!in_param || in_param->mapping.empty())
      SKIP("no input channel on the audio device of this run (SCORE_AUDIO_BACKEND)");
    const State::Address addr{audio->name(), {"in", "main"}};

    Scenario::IntervalModel* box{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
      box = &m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(10000), 0.1});
      m.commit();
    }
    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    const auto run = [&](int latency) -> std::pair<int64_t, int64_t> {
      // Playing, steadily: the first playback of a test app is stopped by
      // the audio engine starting
      bool steady = false;
      for(int i = 0; i < 5 && !steady; i++)
      {
        if(!exec.isPlaying())
          engine.execution().request_play_global(true);
        spin(1500);
        steady = exec.isPlaying();
      }
      REQUIRE(steady);
      Recording::RecordOptions opt;
      opt.start = Recording::RecordOptions::Start::Now;
      opt.inputLatencyMs = latency;
      Recording::RecordSession session{doc->context(), opt};
      REQUIRE(session.addAddress(addr));
      session.placeInBox(*box, Recording::AddressPlacement::Automations, {});
      bool finished = false;
      QObject::connect(&session, &Recording::RecordSession::finished, [&] { finished = true; });
      REQUIRE(session.start());
      spin(1200);
      session.stop();
      REQUIRE(spin_until([&] { return finished; }));
      engine.execution().request_stop();
      spin(300);
      Process::ProcessModel* sound{};
      for(auto& p : box->processes)
        if(p.concreteKey() == Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get())
          sound = &p;
      REQUIRE(sound);
      REQUIRE(session.takes().size() == 1);
      const auto r = std::make_pair(sound->startOffset().impl, session.takes()[0].startDate);
      doc->commandStack().undo();
      return r;
    };
    // The file starts where its first sample was heard: without latency, at
    // the take's start; with 100 ms, 100 ms before it
    const auto [off0, start0] = run(0);
    const auto [off1, start1] = run(100);
    CHECK(off0 + start0 == 0);
    CHECK(std::abs((off1 + start1) - 70560000) < 705600);
  });
}

namespace
{
//! "client:port" of an ALSA sequencer port whose name contains `name`
QString alsaPort(const QString& name)
{
  QProcess p;
  // Ports other programs write to: score's inputs
  p.start("aconnect", {"-o", "-l"});
  if(!p.waitForFinished(3000))
    return {};
  QString client;
  for(const auto& line : QString::fromUtf8(p.readAllStandardOutput()).split('\n'))
  {
    if(line.startsWith("client "))
      client = line.section(' ', 1, 1).chopped(1);
    else if(line.contains(name))
      return client + ":" + line.trimmed().section(' ', 0, 0);
  }
  return {};
}
}

TEST_CASE("A MIDI device of the explorer is recorded as notes", "[recording][addresses][midi-device]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    if(QStandardPaths::findExecutable("aseqsend").isEmpty()
       || QStandardPaths::findExecutable("aconnect").isEmpty())
      SKIP("needs ALSA's aseqsend and aconnect");
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& devices = doc->context().plugin<Explorer::DeviceDocumentPlugin>();

    // A MIDI input of its own, which notes are sent to from outside
    Device::DeviceSettings dev;
    dev.name = "keys";
    dev.protocol = Protocols::MIDIInputProtocolFactory::static_concreteKey();
    Protocols::MIDISpecificSettings ms;
    ms.io = Protocols::MIDISpecificSettings::IO::In;
    ms.handle.api = libremidi::API::ALSA_SEQ;
    ms.virtualPort = true;
    ms.handle.port_name = "score-record-test";
    ms.handle.display_name = "score-record-test";
    dev.deviceSpecificSettings = QVariant::fromValue(ms);
    CommandDispatcher<>{doc->context().commandStack}.submit(
        new Explorer::Command::LoadDevice{devices, Device::Node{dev, nullptr}});
    spin(500);
    const QString port = alsaPort("score-record-test");
    if(port.isEmpty())
      SKIP("the MIDI input did not appear in ALSA");

    auto& scenar = score::test::base_scenario(*doc);
    Scenario::IntervalModel* box{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
      box = &m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(20000), 0.1});
      m.commit();
    }
    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    bool steady = false;
    for(int i = 0; i < 5 && !steady; i++)
    {
      if(!exec.isPlaying())
        engine.execution().request_play_global(true);
      spin(1500);
      steady = exec.isPlaying();
    }
    REQUIRE(steady);

    Recording::RecordOptions opt;
    opt.start = Recording::RecordOptions::Start::Now;
    Recording::RecordSession session{doc->context(), opt};
    REQUIRE(session.addAddress(State::Address{"keys", {}}));
    session.placeInBox(*box, Recording::AddressPlacement::Automations, {});
    bool finished = false;
    QObject::connect(&session, &Recording::RecordSession::finished, [&] { finished = true; });
    REQUIRE(session.start());
    spin(400);
    QProcess::execute("aseqsend", {"-p", port, "90", "3C", "64"});
    spin(500);
    QProcess::execute("aseqsend", {"-p", port, "80", "3C", "00"});
    spin(400);
    session.stop();
    REQUIRE(spin_until([&] { return finished; }));
    engine.execution().request_stop();
    spin(200);

    Midi::ProcessModel* midi{};
    for(auto& p : box->processes)
      if(auto* m = qobject_cast<Midi::ProcessModel*>(&p))
        midi = m;
    INFO(session.warnings().join(" | ").toStdString());
    REQUIRE(midi);
    REQUIRE(midi->notes.size() == 1);
    CHECK(midi->notes.begin()->pitch() == 60);
  });
}

TEST_CASE("Addresses recorded for a number of bars loop as a port's take does", "[recording][addresses][bars]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& scenar = score::test::base_scenario(*doc);
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    auto* audio = plug.list().audioDevice();
    REQUIRE(audio);
    const State::Address out{audio->name(), {"out", "main"}};
    Scenario::IntervalModel* box{};
    {
      Scenario::Command::Macro m{new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
      auto& from = m.createState(scenar, scenar.startEvent().id(), 0.1);
      box = &m.createIntervalAfter(scenar, from.id(), {TimeVal::fromMsecs(20000), 0.1});
      m.commit();
    }
    auto& engine = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
    auto& exec = doc->context().plugin<Execution::DocumentPlugin>();
    bool steady = false;
    for(int i = 0; i < 5 && !steady; i++)
    {
      if(!exec.isPlaying())
        engine.execution().request_play_global(true);
      spin(1500);
      steady = exec.isPlaying();
    }
    REQUIRE(steady);

    auto& rec = ctx.guiApplicationPlugin<Recording::ApplicationPlugin>();
    auto opt = rec.defaultOptions();
    opt.startRate = 1.;
    opt.lengthBars = 1.;
    opt.loopFixedLength = true;
    opt.target = Recording::RecordOptions::Target::ThisBox;
    auto* session = rec.recordAddressesInto(
        *box, {out}, Recording::AddressPlacement::Automations, {}, opt);
    REQUIRE(session);
    bool finished = false;
    QObject::connect(session, &Recording::RecordSession::finished, [&] { finished = true; });
    // A bar is 2 s at 120: it ends by itself
    for(int i = 0; i < 16 && !finished; i++)
    {
      spin(500);
    }
    REQUIRE(finished);
    engine.execution().request_stop();
    spin(300);
    Process::ProcessModel* sound{};
    for(auto& p : box->processes)
      if(p.concreteKey() == Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get())
        sound = &p;
    REQUIRE(sound);
    CHECK(sound->loops());
    CHECK(sound->loopDuration().impl == int64_t(4 * 352800000LL));
  });
}
