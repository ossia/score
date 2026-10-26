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

//! The port named `name` of the looper becomes a combobox of the note list
//! named `saved_name`, set to `value`: what the looper saved when that control
//! was the list.
void make_legacy_note_list(
    Process::ProcessModel& looper, const QString& name, const QString& saved_name,
    float value)
{
  auto& inlets = InletsAccess::of(looper);
  auto it = ossia::find_if(inlets, [&](auto* in) { return in->name() == name; });
  REQUIRE(it != inlets.end());
  std::vector<std::pair<QString, ossia::value>> notes;
  for(const auto& [name, v] : Control::Widgets::notes)
    notes.emplace_back(QString::fromUtf8(name), ossia::value{v});
  auto* combo = new Process::ComboBox{notes, 0.25f, saved_name, (*it)->id(), &looper};
  combo->setValue(value);
  delete *it;
  *it = combo;
}

//! The looper as it was saved when Quantif was the list of note values.
void make_legacy_quantif(Process::ProcessModel& looper, float value)
{
  make_legacy_note_list(looper, "Quantize", "Quantif", value);
}

ossia::vec2f grid_of(const Process::ProcessModel& looper, const QString& name = "Quantize")
{
  auto* quantif = qobject_cast<Process::GridChooser*>(inlet_named(looper, name));
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

TEST_CASE("The loopers' Quantize: a score saved with the Quantif note list", "[avnd][upgrade][looper]")
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
          CHECK(inlet_named(*loaded, "Quantize")->id() == id);
          CHECK(inlet_named(*loaded, "Quantif") == nullptr);
          CHECK(grid_of(*loaded) == c.grid);
        }
      }
    }
  });
}

TEST_CASE("The loopers' Quantize: a preset saved with the Quantif note list", "[avnd][upgrade][looper]")
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
      const int id = inlet_named(*looper, "Quantize")->id().val();
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

namespace
{
QString name_of(const Process::Port& p)
{
  return p.name();
}

//! A looper as a score saved it before Mode, Quantize, Record bars, Note grid,
//! Strength and Note ends had those names: Quantif and Grid lists of note
//! values, and the MIDI looper's strength called Quantize. With `sync`, it has
//! the Sync port the loopers had then, at `sync_at`, the ports after it one id
//! further, and it is loaded through the rebuild of its ports.
std::unique_ptr<Process::ProcessModel> make_renamed_looper(
    const score::DocumentContext& dctx, const QString& uuid, bool midi, bool sync,
    int sync_at)
{
  auto looper = make_looper(dctx, uuid);
  if(!looper)
    return {};
  inlet_named(*looper, "Mode")->setValue(std::string{"Overdub"});
  inlet_named(*looper, "Record bars")->setValue(2);
  make_legacy_quantif(*looper, 0.25f);
  if(midi)
  {
    make_legacy_note_list(*looper, "Note grid", "Grid", 1.f / 16.f);
    inlet_named(*looper, "Strength")->setValue(0.7f);
    inlet_named(*looper, "Note ends")->setValue(true);
  }
  else
  {
    inlet_named(*looper, "Follow tempo")->setValue(2);
  }
  // The Quantif list is in place: Quantize is the strength's name from here on.
  const std::pair<const char*, const char*> renames[]
      = {{"Mode", "Loop"},
         {"Record bars", "Bars"},
         {"Strength", "Quantize"},
         {"Note ends", "Quantize ends"}};
  for(auto [now, was] : renames)
    if(auto* p = inlet_named(*looper, now))
      p->setName(was);

  if(sync)
  {
    auto& inlets = InletsAccess::of(*looper);
    for(std::size_t i = sync_at; i < inlets.size(); i++)
      inlets[i]->setId(Id<Process::Port>{inlets[i]->id().val() + 1});
    std::vector<std::pair<QString, ossia::value>> groups;
    for(const char* g : {"None", "A", "B", "C", "D"})
      groups.emplace_back(g, ossia::value{int(groups.size())});
    auto* combo = new Process::ComboBox{
        groups, ossia::value{0}, "Sync", Id<Process::Port>{sync_at}, looper.get()};
    combo->setValue(1);
    inlets.insert(inlets.begin() + sync_at, combo);
  }
  return looper;
}
}

TEST_CASE(
    "The loopers: a score saved before the renames and with Sync loads every value "
    "where it belongs",
    "[avnd][upgrade][looper]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    struct looper_case
    {
      QString uuid;
      bool midi;
      int sync_at;
    };
    for(const auto& [uuid, midi, sync_at] :
        {looper_case{audio_looper_uuid, false, 16}, looper_case{midi_looper_uuid, true, 20}})
    {
      auto fresh = make_looper(dctx, uuid);
      if(!fresh)
        SKIP("the loopers are not built");
      for(bool sync : {false, true})
      {
        auto old = make_renamed_looper(dctx, uuid, midi, sync, sync_at);
        REQUIRE(old);
        REQUIRE(old->inlets().size() == fresh->inlets().size() + (sync ? 1 : 0));
        // The ids the document's cables point at.
        const QString kept[] = {"Capture", "Export", "At max layers", "Passes"};
        std::vector<int> saved_ids;
        for(const auto& name : kept)
          saved_ids.push_back(inlet_named(*old, name)->id().val());

        for(auto roundtrip : {&roundtrip_json, &roundtrip_datastream})
        {
          INFO(uuid.toStdString() << (sync ? " with Sync" : " kept ports"));
          std::unique_ptr<Process::ProcessModel> loaded{roundtrip(*old, dctx, nullptr)};
          REQUIRE(loaded);
          CHECK(loaded->inlets().size() == fresh->inlets().size());
          for(const char* gone : {"Sync", "Loop", "Bars", "Quantif", "Grid", "Quantize ends"})
            CHECK(inlet_named(*loaded, gone) == nullptr);
          for(std::size_t i = 0; i < loaded->inlets().size(); i++)
            CHECK(name_of(*loaded->inlets()[i]) == name_of(*fresh->inlets()[i]));

          CHECK(ossia::convert<std::string>(inlet_named(*loaded, "Mode")->value()) == "Overdub");
          CHECK(ossia::convert<int>(inlet_named(*loaded, "Record bars")->value()) == 2);
          // Quantif's 4th is a quarter note, and the grid is Quantize, not the
          // strength that had that name.
          CHECK(grid_of(*loaded) == ossia::vec2f{0.25f, 1.f});
          if(midi)
          {
            CHECK(grid_of(*loaded, "Note grid") == ossia::vec2f{1.f / 16.f, 1.f});
            auto* strength = inlet_named(*loaded, "Strength");
            REQUIRE(strength);
            CHECK(!qobject_cast<Process::GridChooser*>(strength));
            CHECK(ossia::convert<float>(strength->value()) == 0.7f);
            CHECK(ossia::convert<bool>(inlet_named(*loaded, "Note ends")->value()));
          }
          else
          {
            CHECK(ossia::convert<int>(inlet_named(*loaded, "Follow tempo")->value()) == 2);
          }
          for(std::size_t k = 0; k < std::size(kept); k++)
          {
            INFO(kept[k].toStdString());
            CHECK(inlet_named(*loaded, kept[k])->id().val() == saved_ids[k]);
          }
        }
      }
    }
  });
}

TEST_CASE(
    "The loopers' presets: named controls, and the indices of presets that do not "
    "name them",
    "[avnd][upgrade][looper]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    auto load = [](Process::ProcessModel& looper, const char* json) {
      Process::Preset p;
      p.key.key = looper.concreteKey();
      p.data = json;
      looper.loadPreset(p);
    };
    auto as_int = [](Process::ProcessModel& looper, const char* name) {
      auto* in = inlet_named(looper, name);
      REQUIRE(in);
      return ossia::convert<int>(in->value());
    };
    auto as_string = [](Process::ProcessModel& looper, const char* name) {
      auto* in = inlet_named(looper, name);
      REQUIRE(in);
      return ossia::convert<std::string>(in->value());
    };

    {
      auto looper = make_looper(dctx, audio_looper_uuid);
      if(!looper)
        SKIP("the loopers are not built");
      // A preset of the audio looper of 7 inlets: Quantif Whole, Restart Always.
      load(*looper, R"([[2,{"Float":1.0}],[6,{"String":"Always"}],[5,{"Int":3}]])");
      CHECK(grid_of(*looper) == ossia::vec2f{1.f, 2.f});
      CHECK(as_string(*looper, "Start from") == "Loop start");
      CHECK(as_int(*looper, "Record bars") == 3);

      // A preset saved with Sync, at 16: what came after it is one index on.
      load(*looper, R"([[16,{"Int":3}],[17,{"Int":7}],[18,{"Int":5}],[47,{"Int":0}]])");
      CHECK(as_int(*looper, "Keep last (s)") == 7);
      CHECK(as_int(*looper, "Capture bars") == 5);
      CHECK(as_int(*looper, "At max layers") == 0);

      // Named: by its name when the id is another control's, by the name it
      // had, and its choices by the names they had.
      load(
          *looper,
          R"([[3,{"Int":9},"Record bars"],[40,{"Vec2f":[2.0,2.0]},"Quantif"],[1,{"String":"Recording"},"Restart"]])");
      CHECK(as_int(*looper, "Record bars") == 9);
      CHECK(as_int(*looper, "Passthrough") != 9);
      CHECK(grid_of(*looper) == ossia::vec2f{2.f, 2.f});
      CHECK(as_string(*looper, "Start from") == "Where it was");

      // What it saves names every control, and loads back.
      const auto saved = looper->savePreset();
      CHECK(saved.data.contains(R"("Record bars"])"));
      load(*looper, R"([[5,{"Int":1}],[2,{"Vec2f":[0.0,0.0]}]])");
      looper->loadPreset(saved);
      CHECK(as_int(*looper, "Record bars") == 9);
      CHECK(grid_of(*looper) == ossia::vec2f{2.f, 2.f});
    }
    {
      auto looper = make_looper(dctx, midi_looper_uuid);
      REQUIRE(looper);
      // Sync was at 20; Grid a note list, Quantize the strength.
      load(
          *looper,
          R"([[12,{"Float":0.125}],[13,{"Float":0.5}],[20,{"Int":2}],[21,{"Int":6}],[49,{"Int":5}]])");
      CHECK(grid_of(*looper, "Note grid") == ossia::vec2f{0.125f, 1.f});
      CHECK(ossia::convert<float>(inlet_named(*looper, "Strength")->value()) == 0.5f);
      CHECK(as_int(*looper, "Capture bars") == 6);
      CHECK(as_int(*looper, "Max layers") == 5);
      // The note list's None is no note value: the default one, Strength
      // deciding whether notes move.
      load(*looper, R"([[12,{"Float":0.0}]])");
      CHECK(grid_of(*looper, "Note grid") == ossia::vec2f{0.25f, 1.f});

      // Named: Quantize is the grid, Strength the strength.
      load(
          *looper,
          R"([[13,{"Vec2f":[0.5,3.0]},"Quantize"],[2,{"Float":0.25},"Strength"]])");
      CHECK(grid_of(*looper) == ossia::vec2f{0.5f, 3.f});
      CHECK(ossia::convert<float>(inlet_named(*looper, "Strength")->value()) == 0.25f);
    }
  });
}
