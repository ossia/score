// A document saved when a control of an avendish process was another kind of
// control (here LFO v3's Period, once a float slider, now a time chooser),
// loaded with the current object, in JSON and in the binary format: the
// control comes back as the declared kind with its value (as seconds), its
// address, and its id -- which the document's cables are restored against.

#include <State/Address.hpp>

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Dataflow/Commands/EditConnection.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/document/Document.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <Avnd/Factories.hpp>
#include <Crousti/Executor.hpp>
#include <Crousti/ProcessModel.hpp>
#include <Fx/LFO_v3.hpp>
#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

namespace
{
const QString lfo_uuid = QStringLiteral("0b1b1816-c33e-4796-a16d-5aab27fe600f");

using Lfo = Nodes::LFO::v3::Node;

//! LFO v3 as it was saved when its Period was a float slider: the same uuid
//! and the same ports, but for that one, and without Lock to bars and One shot.
struct OldLfo
{
  halp_meta(name, "LFO")
  halp_meta(c_name, "LFO")
  halp_meta(category, "Control/Generators")
  halp_meta(author, "ossia score")
  halp_meta(uuid, "0b1b1816-c33e-4796-a16d-5aab27fe600f");

  struct
  {
    halp::hslider_f32<"Period", halp::range{0.01, 60., 1.}> period;
    decltype(Lfo::ins::shape) shape;
    decltype(Lfo::ins::retrigger) retrigger;
    decltype(Lfo::ins::ampl) ampl;
    decltype(Lfo::ins::offset) offset;
    decltype(Lfo::ins::jitter) jitter;
    decltype(Lfo::ins::phase) phase;
    decltype(Lfo::ins::waveform) waveform;
  } inputs;
  decltype(Lfo::outputs) outputs;

  using tick = halp::tick_flicks;
  void operator()(const tick&) { }
};

Process::ProcessModel* roundtrip_datastream(
    const Process::ProcessModel& proc, const score::DocumentContext& dctx,
    QObject* parent)
{
  auto& pl = dctx.app.interfaces<Process::ProcessFactoryList>();
  const QByteArray bytes = DataStreamReader::marshall(proc);
  DataStream::Deserializer des{bytes};
  return deserialize_interface(pl, des, dctx, parent);
}

Process::ProcessModel* roundtrip_json(
    const Process::ProcessModel& proc, const score::DocumentContext& dctx,
    QObject* parent)
{
  auto& pl = dctx.app.interfaces<Process::ProcessFactoryList>();
  JSONReader reader;
  reader.readFrom(proc);
  const auto doc = readJson(reader.toByteArray());
  JSONObject::Deserializer des{doc};
  return deserialize_interface(pl, des, dctx, parent);
}
}

TEST_CASE("A float control that became a time chooser keeps its value", "[avnd][upgrade]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    auto& itv = score::test::base_interval(*doc);

    // The current object, as the source of a cable into the old one
    auto* source = score::test::add_process(*doc, lfo_uuid, {});
    if(!source)
      SKIP("LFO v3 is not built");

    auto* old = new oscr::ProcessModel<OldLfo>{
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{7777}, dctx, &itv};
    itv.processes.add(old);

    auto* period = qobject_cast<Process::FloatSlider*>(old->inlets()[0]);
    REQUIRE(period);
    period->setValue(2.5f);
    const State::AddressAccessor address{State::Address{"dev", {"lfo", "period"}}};
    period->setAddress(address);
    auto* shape = qobject_cast<Process::ControlInlet*>(old->inlets()[1]);
    REQUIRE(shape);
    shape->setValue(0.8f);

    auto& model = score::IDocument::get<Scenario::ScenarioDocumentModel>(*doc);
    CommandDispatcher<>{dctx.commandStack}.submit<Dataflow::CreateCable>(
        model, Id<Process::Cable>{1}, Process::CableType::ImmediateGlutton,
        *source->outlets()[0], *period);
    REQUIRE(period->cables().size() == 1);
    const auto period_id = period->id();
    // The cable is the document's: on load it finds its sink by that id
    auto& cable = model.cables.at(Id<Process::Cable>{1});
    REQUIRE(!cable.sink().unsafePath().vec().empty());
    CHECK(cable.sink().unsafePath().vec().back().id() == period_id.val());

    for(auto roundtrip : {&roundtrip_json, &roundtrip_datastream})
    {
      auto* loaded = roundtrip(*old, dctx, &doc->model());
      REQUIRE(loaded);
      // The ports added since: Lock to bars and One shot
      REQUIRE(loaded->inlets().size() == old->inlets().size() + 2);
      auto added = [&](const QString& name) -> Process::ControlInlet* {
        for(auto* in : loaded->inlets())
          if(in->name() == name)
            return qobject_cast<Process::ControlInlet*>(in);
        return nullptr;
      };

      auto* chooser = qobject_cast<Process::TimeChooser*>(loaded->inlets()[0]);
      REQUIRE(chooser);
      CHECK(chooser->id() == period_id);
      // Seconds, free: what the slider meant
      CHECK(chooser->value() == ossia::value{ossia::vec2f{2.5f, 0.f}});
      CHECK(chooser->address() == address);

      // The controls that did not change are untouched
      auto* loaded_shape = qobject_cast<Process::ControlInlet*>(loaded->inlets()[1]);
      REQUIRE(loaded_shape);
      CHECK(ossia::convert<float>(loaded_shape->value()) == 0.8f);

      // A synced LFO saved before the lock counts its cycles from the start,
      // as it did then.
      auto* lock = added(QStringLiteral("Lock to bars"));
      REQUIRE(lock);
      CHECK(ossia::convert<bool>(lock->value()) == false);
      // An LFO saved before One shot keeps cycling.
      auto* one_shot = added(QStringLiteral("One shot"));
      REQUIRE(one_shot);
      CHECK(ossia::convert<bool>(one_shot->value()) == false);
      delete loaded;
    }
  });
}

namespace
{
//! A rate in Hz, as documents of this object saved it...
struct RateSpec
{
  halp_meta(name, "Rate to period")
  halp_meta(c_name, "rate_to_period_test")
  halp_meta(uuid, "5a4e8f0c-7d0b-4a51-9d7e-6c1f4b2e9a13");
  struct
  {
    halp::hslider_f32<"Rate", halp::range{0.01, 40., 5.}> rate;
  } inputs;
  struct
  {
  } outputs;
  void operator()() { }
};

//! ...which became a period: the field converts what was saved.
struct PeriodSpec
{
  halp_meta(name, "Rate to period")
  halp_meta(c_name, "rate_to_period_test")
  halp_meta(uuid, "5a4e8f0c-7d0b-4a51-9d7e-6c1f4b2e9a13");
  struct
  {
    struct : halp::time_chooser<"Period", halp::range{0.025, 100., 0.2}>
    {
      static float upgrade_value(float hz) { return hz > 0.f ? 1.f / hz : 0.2f; }
    } period;
  } inputs;
  struct
  {
  } outputs;
  void operator()() { }
};
}

TEST_CASE("A control whose unit changed converts the saved value", "[avnd][upgrade]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();

    oscr::ProcessModel<RateSpec> old{
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{7778}, dctx, nullptr};
    auto* rate = qobject_cast<Process::FloatSlider*>(old.inlets()[0]);
    REQUIRE(rate);
    rate->setValue(4.f); // Hz

    JSONReader reader;
    reader.readFrom(static_cast<const Process::ProcessModel&>(old));
    const auto json = readJson(reader.toByteArray());
    JSONObject::Deserializer des{json};
    auto* loaded = new oscr::ProcessModel<PeriodSpec>{des, &doc->model()};
    auto* period = qobject_cast<Process::TimeChooser*>(loaded->inlets()[0]);
    REQUIRE(period);
    CHECK(period->id() == rate->id());
    CHECK(period->value() == ossia::value{ossia::vec2f{0.25f, 0.f}});
    delete loaded;
  });
}

#include <AvndProcesses/EntityToMidi.hpp>

TEST_CASE("Entity to MIDI: its times were milliseconds", "[avnd][upgrade]")
{
  using In = decltype(avnd_tools::EntityToMidi::inputs);
  // The hook the upgrade picks for each control that was a spinbox in ms
  CHECK(decltype(In::min_note)::upgrade_value(80.f) == 0.08f);
  CHECK(decltype(In::max_note)::upgrade_value(1500.f) == 1.5f);
  CHECK(decltype(In::trigger_duration)::upgrade_value(200.f) == 0.2f);
  CHECK(decltype(In::glide)::upgrade_value(60.f) == 0.06f);
  CHECK(decltype(In::max_hold)::upgrade_value(250.f) == 0.25f);
}

namespace
{
//! A knob with a mapper has a port type of its own, derived from the object and
//! the field...
struct MappedKnobSpec
{
  halp_meta(name, "Mapped knob to time chooser")
  halp_meta(c_name, "mapped_knob_to_time_chooser_test")
  halp_meta(uuid, "6b5f9a1d-8e1c-4b62-8e8f-7d205c3fab24");
  struct
  {
    struct : halp::knob_f32<"Decay", halp::range{0.005, 20., 0.35}>
    {
      using mapper = halp::log_mapper<std::ratio<85, 100>>;
    } decay;
    halp::knob_f32<"Level", halp::range{0., 2., 1.}> level;
  } inputs;
  struct
  {
  } outputs;
  void operator()() { }
};

//! ...which does not exist any more once it is a time chooser.
struct TimeChooserSpec
{
  halp_meta(name, "Mapped knob to time chooser")
  halp_meta(c_name, "mapped_knob_to_time_chooser_test")
  halp_meta(uuid, "6b5f9a1d-8e1c-4b62-8e8f-7d205c3fab24");
  struct
  {
    halp::time_chooser<"Decay", halp::range{0.005, 20., 0.35}> decay;
    halp::knob_f32<"Level", halp::range{0., 2., 1.}> level;
  } inputs;
  struct
  {
  } outputs;
  void operator()() { }
};
}

TEST_CASE(
    "A mapped knob, a port type of its own, that became a time chooser keeps its "
    "value",
    "[avnd][upgrade]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();

    oscr::ProcessModel<MappedKnobSpec> old{
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{7779}, dctx, nullptr};
    auto* decay = qobject_cast<Process::ControlInlet*>(old.inlets()[0]);
    REQUIRE(decay);
    // Not one of the generic port types: nothing registers it
    CHECK(!ctx.interfaces<Process::PortFactoryList>().get(decay->concreteKey()));
    decay->setValue(3.2f);
    const State::AddressAccessor address{State::Address{"dev", {"decay"}}};
    decay->setAddress(address);
    auto* level = qobject_cast<Process::ControlInlet*>(old.inlets()[1]);
    REQUIRE(level);
    level->setValue(1.5f);

    SECTION("JSON")
    {
      JSONReader reader;
      reader.readFrom(static_cast<const Process::ProcessModel&>(old));
      const auto json = readJson(reader.toByteArray());
      JSONObject::Deserializer des{json};
      auto* loaded = new oscr::ProcessModel<TimeChooserSpec>{des, &doc->model()};
      REQUIRE(loaded->inlets().size() == 2);

      auto* chooser = qobject_cast<Process::TimeChooser*>(loaded->inlets()[0]);
      REQUIRE(chooser);
      CHECK(chooser->id() == decay->id());
      CHECK(chooser->value() == ossia::value{ossia::vec2f{3.2f, 0.f}});
      CHECK(chooser->address() == address);

      auto* loaded_level = qobject_cast<Process::ControlInlet*>(loaded->inlets()[1]);
      REQUIRE(loaded_level);
      CHECK(loaded_level->value() == ossia::value{1.5f});
      delete loaded;
    }

    SECTION("Binary")
    {
      // The binary format carries no field names: the unknown port is read up
      // to its Port base and rebuilt as the declared control, at its default.
      const QByteArray bytes
          = DataStreamReader::marshall(static_cast<const Process::ProcessModel&>(old));
      DataStream::Deserializer outer{bytes};
      QByteArray process;
      outer.stream() >> process;
      DataStream::Deserializer des{process};
      SCORE_DEBUG_CHECK_DELIMITER2(des);
      UuidKey<Process::ProcessModel> key;
      TSerializer<DataStream, UuidKey<Process::ProcessModel>>::writeTo(des, key);
      SCORE_DEBUG_CHECK_DELIMITER2(des);
      REQUIRE(key == static_cast<const Process::ProcessModel&>(old).concreteKey());
      auto* loaded = new oscr::ProcessModel<TimeChooserSpec>{des, &doc->model()};
      REQUIRE(loaded->inlets().size() == 2);

      auto* chooser = qobject_cast<Process::TimeChooser*>(loaded->inlets()[0]);
      REQUIRE(chooser);
      CHECK(chooser->id() == decay->id());
      CHECK(chooser->address() == address);

      auto* loaded_level = qobject_cast<Process::ControlInlet*>(loaded->inlets()[1]);
      REQUIRE(loaded_level);
      CHECK(loaded_level->value() == ossia::value{1.5f});
      delete loaded;
    }
  });
}

#include <Process/Preset.hpp>

#include <Engine/Node/CommonWidgets.hpp>

#include <QDir>
#include <QFile>

namespace
{
const QString audio_looper_uuid = QStringLiteral("a0ad4227-ac3d-448b-a19b-19581ed4e2c6");
const QString midi_looper_uuid = QStringLiteral("0c0b7d3e-5a52-4b8f-9d1f-6e3a2b8c4f71");

struct InletsAccess : Process::ProcessModel
{
  static Process::Inlets& of(Process::ProcessModel& p) { return p.*(&InletsAccess::m_inlets); }
};

Process::ControlInlet* inlet_named(const Process::ProcessModel& p, const QString& name)
{
  for(auto* in : p.inlets())
    if(in->name() == name)
      return qobject_cast<Process::ControlInlet*>(in);
  return nullptr;
}

std::unique_ptr<Process::ProcessModel>
make_looper(const score::DocumentContext& dctx, const QString& uuid)
{
  auto& pl = dctx.app.interfaces<Process::ProcessFactoryList>();
  auto* factory = pl.get(UuidKey<Process::ProcessModel>::fromString(uuid));
  if(!factory)
    return {};
  return std::unique_ptr<Process::ProcessModel>{factory->make(
      TimeVal::fromMsecs(1000), QString{}, Id<Process::ProcessModel>{4242}, dctx,
      nullptr)};
}

//! The looper as it was saved when Quantif was the list of note values: its
//! port a combobox of them, set to `value`.
void make_legacy_quantif(Process::ProcessModel& looper, float value)
{
  auto& inlets = InletsAccess::of(looper);
  auto it = ossia::find_if(inlets, [](auto* in) { return in->name() == "Quantif"; });
  REQUIRE(it != inlets.end());
  std::vector<std::pair<QString, ossia::value>> notes;
  for(const auto& [name, v] : Control::Widgets::notes)
    notes.emplace_back(QString::fromUtf8(name), ossia::value{v});
  auto* combo = new Process::ComboBox{notes, 0.25f, "Quantif", (*it)->id(), &looper};
  combo->setValue(value);
  delete *it;
  *it = combo;
}

ossia::vec2f grid_of(const Process::ProcessModel& looper)
{
  auto* quantif = qobject_cast<Process::GridChooser*>(inlet_named(looper, "Quantif"));
  REQUIRE(quantif);
  const auto* v = quantif->value().target<ossia::vec2f>();
  REQUIRE(v);
  return *v;
}

// {value, unit}: unit 0 seconds (0 is None), 1 a note value, 2 bars
struct legacy_case
{
  float saved;
  ossia::vec2f grid;
};
const legacy_case legacy_cases[] = {
    {0.f, {0.f, 0.f}},           // None
    {1.f, {1.f, 2.f}},           // Whole was a bar through the engine's grid
    {0.25f, {0.25f, 1.f}},       // 4th
    {0.5f, {0.5f, 1.f}},         // Half
    {3.f / 16.f, {3.f / 16.f, 1.f}}, // Dotted 8th
    {0.75f, {0.75f, 1.f}},       // Dotted Half
};
}

TEST_CASE("The loopers' Quantif: a score saved with the note list", "[avnd][upgrade][looper]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    for(const auto& uuid : {audio_looper_uuid, midi_looper_uuid})
    {
      for(const auto& c : legacy_cases)
      {
        auto looper = make_looper(dctx, uuid);
        if(!looper)
          SKIP("the loopers are not built");
        make_legacy_quantif(*looper, c.saved);
        const auto id = inlet_named(*looper, "Quantif")->id();
        for(auto roundtrip : {&roundtrip_json, &roundtrip_datastream})
        {
          std::unique_ptr<Process::ProcessModel> loaded{
              roundtrip(*looper, dctx, nullptr)};
          REQUIRE(loaded);
          INFO(uuid.toStdString() << " " << c.saved);
          CHECK(inlet_named(*loaded, "Quantif")->id() == id);
          CHECK(grid_of(*loaded) == c.grid);
        }
      }
    }
  });
}

TEST_CASE("The loopers' Quantif: a preset saved with the note list", "[avnd][upgrade][looper]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    for(const auto& uuid : {audio_looper_uuid, midi_looper_uuid})
    {
      auto looper = make_looper(dctx, uuid);
      if(!looper)
        SKIP("the loopers are not built");
      const int id = inlet_named(*looper, "Quantif")->id().val();
      for(const auto& c : legacy_cases)
      {
        INFO(uuid.toStdString() << " " << c.saved);
        Process::Preset p;
        p.key.key = looper->concreteKey();
        p.data = QStringLiteral("[[%1,{\"Float\":%2}]]")
                     .arg(id)
                     .arg(double(c.saved), 0, 'g', 9)
                     .toUtf8();
        looper->loadPreset(p);
        CHECK(grid_of(*looper) == c.grid);
      }

      // A preset saved since keeps what it says, Loop included
      Process::Preset p;
      p.key.key = looper->concreteKey();
      p.data = QStringLiteral("[[%1,{\"Vec2f\":[0.5,3.0]}]]").arg(id).toUtf8();
      looper->loadPreset(p);
      CHECK(grid_of(*looper) == ossia::vec2f{0.5f, 3.f});
      // And what it saves is what it loads
      looper->loadPreset(looper->savePreset());
      CHECK(grid_of(*looper) == ossia::vec2f{0.5f, 3.f});
    }

#if defined(SCORE_TEST_PRESETS_ROOT)
    // The library's presets: quantized to the bar unless they say otherwise
    struct expected
    {
      const char* folder;
      const QString& uuid;
    };
    for(const auto& [folder, uuid] :
        {expected{"Looper (audio)", audio_looper_uuid},
         expected{"Looper (MIDI)", midi_looper_uuid}})
    {
      auto looper = make_looper(dctx, uuid);
      REQUIRE(looper);
      QDir dir{QStringLiteral(SCORE_TEST_PRESETS_ROOT) + "/" + folder};
      const auto files = dir.entryList({"*.scp"}, QDir::Files, QDir::Name);
      REQUIRE(!files.empty());
      for(const auto& f : files)
      {
        QFile file{dir.filePath(f)};
        REQUIRE(file.open(QIODevice::ReadOnly));
        auto preset = Process::Preset::fromJson(
            ctx.interfaces<Process::ProcessFactoryList>(), file.readAll());
        REQUIRE(preset);
        looper->loadPreset(*preset);
        INFO(folder << "/" << f.toStdString());
        const auto g = grid_of(*looper);
        if(f.startsWith("Free"))
          CHECK(g == ossia::vec2f{0.f, 0.f});
        else if(f.startsWith("Sixteenths"))
          CHECK(g == ossia::vec2f{0.25f, 1.f});
        else if(f.contains("song form", Qt::CaseInsensitive))
          CHECK(g == ossia::vec2f{1.f, 3.f});
        else
          CHECK(g == ossia::vec2f{1.f, 2.f});
      }
    }
#endif
  });
}
