// The MIDI objects whose controls changed kind, loaded from what scores and
// presets saved before: the Midi filter's Types, Notes and CCs, texts that
// became toggles and rows of ranges, and Midi sustain's Pedal, a toggle that
// became a momentary button. And a score that has a Midi CC snapshot, an object
// that is no more, loads without it.

#include <State/Address.hpp>

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/Preset.hpp>
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

#include <QDir>
#include <QFile>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

namespace
{
const QString midi_filter_uuid = QStringLiteral("f66d6cf7-693a-47d4-a249-5978cc946e43");
const QString midi_sustain_uuid = QStringLiteral("943f891c-e56f-4e60-a270-3420657fe368");
const QString midi_glide_uuid = QStringLiteral("74d26417-1ce5-4765-874e-055921f0a0bb");
const QString midi_cc_snapshot_uuid
    = QStringLiteral("e941cff0-8ebb-4259-a146-29ce10125791");

struct InletsAccess : Process::ProcessModel
{
  static Process::Inlets& of(Process::ProcessModel& p) { return p.*(&InletsAccess::m_inlets); }
};

std::unique_ptr<Process::ProcessModel>
make_process(const score::DocumentContext& dctx, const QString& uuid)
{
  auto& pl = dctx.app.interfaces<Process::ProcessFactoryList>();
  auto* factory = pl.get(UuidKey<Process::ProcessModel>::fromString(uuid));
  if(!factory)
    return {};
  return std::unique_ptr<Process::ProcessModel>{factory->make(
      TimeVal::fromMsecs(1000), QString{}, Id<Process::ProcessModel>{4243}, dctx,
      nullptr)};
}

Process::ControlInlet* inlet_named(const Process::ProcessModel& p, const QString& name)
{
  for(auto* in : p.inlets())
    if(in->name() == name)
      return qobject_cast<Process::ControlInlet*>(in);
  return nullptr;
}

//! The control `name` of `p` replaced by `old`, made with the same id: what a
//! score saved when the control was of that kind.
template <typename F>
Id<Process::Port> replace_inlet(Process::ProcessModel& p, const QString& name, F make)
{
  auto& inlets = InletsAccess::of(p);
  auto it = ossia::find_if(inlets, [&](auto* in) { return in->name() == name; });
  REQUIRE(it != inlets.end());
  const auto id = (*it)->id();
  Process::Inlet* old = make(id);
  delete *it;
  *it = old;
  return id;
}

Process::ProcessModel* roundtrip_json(
    const Process::ProcessModel& proc, const score::DocumentContext& dctx, QObject* parent)
{
  auto& pl = dctx.app.interfaces<Process::ProcessFactoryList>();
  JSONReader reader;
  reader.readFrom(proc);
  const auto doc = readJson(reader.toByteArray());
  JSONObject::Deserializer des{doc};
  return deserialize_interface(pl, des, dctx, parent);
}

Process::ProcessModel* roundtrip_datastream(
    const Process::ProcessModel& proc, const score::DocumentContext& dctx, QObject* parent)
{
  auto& pl = dctx.app.interfaces<Process::ProcessFactoryList>();
  const QByteArray bytes = DataStreamReader::marshall(proc);
  DataStream::Deserializer des{bytes};
  return deserialize_interface(pl, des, dctx, parent);
}

std::vector<int> ints(const ossia::value& v)
{
  std::vector<int> res;
  for(auto& e : ossia::convert<std::vector<ossia::value>>(v))
    res.push_back(ossia::convert<int>(e));
  return res;
}

std::vector<std::vector<int>> rows(const ossia::value& v)
{
  std::vector<std::vector<int>> res;
  for(auto& e : ossia::convert<std::vector<ossia::value>>(v))
    res.push_back(ints(e));
  return res;
}

//! The Types toggles with the kinds of the given indices on (see mtk::midi_kind).
std::vector<int> kinds_on(std::initializer_list<int> on)
{
  std::vector<int> res(18, 0);
  for(int k : on)
    res[k] = 1;
  return res;
}
// mtk::midi_kind
constexpr int k_note_on = 0, k_note_off = 1, k_cc = 2, k_sysex = 7, k_clock = 12;
}

TEST_CASE(
    "Midi filter: a score saved when Types, Notes and CCs were texts",
    "[avnd][upgrade][midi_filter]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    auto filter = make_process(dctx, midi_filter_uuid);
    if(!filter)
      SKIP("the Midi filter is not built");

    const State::AddressAccessor address{State::Address{"dev", {"filter", "types"}}};
    const auto types_id = replace_inlet(*filter, "Types", [&](Id<Process::Port> id) {
      auto* l = new Process::LineEdit{"note, cc", "Types", id, filter.get()};
      l->setAddress(address);
      return l;
    });
    const auto notes_id = replace_inlet(*filter, "Notes", [&](Id<Process::Port> id) {
      return new Process::LineEdit{"C-1-B2, C4", "Notes", id, filter.get()};
    });
    const auto ccs_id = replace_inlet(*filter, "CCs", [&](Id<Process::Port> id) {
      return new Process::LineEdit{"64-69, 1", "CCs", id, filter.get()};
    });
    inlet_named(*filter, "Invert ranges")->setValue(true);

    for(auto roundtrip : {&roundtrip_json, &roundtrip_datastream})
    {
      std::unique_ptr<Process::ProcessModel> loaded{roundtrip(*filter, dctx, nullptr)};
      REQUIRE(loaded);
      auto* types = inlet_named(*loaded, "Types");
      auto* notes = inlet_named(*loaded, "Notes");
      auto* ccs = inlet_named(*loaded, "CCs");
      REQUIRE(types);
      REQUIRE(notes);
      REQUIRE(ccs);
      // The kinds of control the object has now, at the ids the cables know
      CHECK(!qobject_cast<Process::LineEdit*>(types));
      CHECK(!qobject_cast<Process::LineEdit*>(notes));
      CHECK(!qobject_cast<Process::LineEdit*>(ccs));
      CHECK(types->id() == types_id);
      CHECK(notes->id() == notes_id);
      CHECK(ccs->id() == ccs_id);
      CHECK(types->address() == address);

      CHECK(ints(types->value()) == kinds_on({k_note_on, k_note_off, k_cc}));
      CHECK(rows(notes->value()) == std::vector<std::vector<int>>{{0, 47}, {60, 60}});
      CHECK(rows(ccs->value()) == std::vector<std::vector<int>>{{64, 69}, {1, 1}});
      // The rest is untouched
      CHECK(ossia::convert<bool>(inlet_named(*loaded, "Invert ranges")->value()));
    }
  });
}

TEST_CASE(
    "Midi filter: presets saved when Types, Notes and CCs were texts",
    "[avnd][upgrade][midi_filter][presets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    auto filter = make_process(dctx, midi_filter_uuid);
    if(!filter)
      SKIP("the Midi filter is not built");
    auto load = [&](const char* json) {
      Process::Preset p;
      p.key.key = filter->concreteKey();
      p.data = json;
      filter->loadPreset(p);
    };
    auto* types = inlet_named(*filter, "Types");
    auto* notes = inlet_named(*filter, "Notes");
    auto* ccs = inlet_named(*filter, "CCs");
    REQUIRE(types);
    REQUIRE(notes);
    REQUIRE(ccs);

    // By index, as the first presets of this object were saved
    load(R"([[7,{"String":"-sysex, -clock"}],[9,{"String":"C4-B4"}],[10,{"String":"7"}]])");
    {
      std::vector<int> expected(18, 1);
      expected[k_sysex] = 0;
      expected[k_clock] = 0;
      CHECK(ints(types->value()) == expected);
    }
    CHECK(rows(notes->value()) == std::vector<std::vector<int>>{{60, 71}});
    CHECK(rows(ccs->value()) == std::vector<std::vector<int>>{{7, 7}});

    // By name
    load(R"([[7,{"String":"note"},"Types"],[9,{"String":""},"Notes"]])");
    CHECK(ints(types->value()) == kinds_on({k_note_on, k_note_off}));
    CHECK(rows(notes->value()).empty());

    // What it saves now loads back as it is
    load(R"([[7,{"String":"cc"},"Types"],[10,{"String":"1-4, 6"},"CCs"]])");
    const auto saved = filter->savePreset();
    load(R"([[7,{"String":""},"Types"],[10,{"String":""},"CCs"]])");
    CHECK(ints(types->value()) == std::vector<int>(18, 0));
    filter->loadPreset(saved);
    CHECK(ints(types->value()) == kinds_on({k_cc}));
    CHECK(rows(ccs->value()) == std::vector<std::vector<int>>{{1, 4}, {6, 6}});

#if defined(SCORE_TEST_PRESETS_ROOT)
    // The library's presets give toggles and rows
    QDir dir{QStringLiteral(SCORE_TEST_PRESETS_ROOT) + "/Midi filter"};
    const auto files = dir.entryList({"*.scp"}, QDir::Files, QDir::Name);
    REQUIRE(!files.empty());
    for(const auto& f : files)
    {
      INFO(f.toStdString());
      QFile file{dir.filePath(f)};
      REQUIRE(file.open(QIODevice::ReadOnly));
      auto preset = Process::Preset::fromJson(
          ctx.interfaces<Process::ProcessFactoryList>(), file.readAll());
      REQUIRE(preset);
      filter->loadPreset(*preset);
      CHECK(ints(types->value()).size() == 18);
      CHECK(notes->value().get_type() == ossia::val_type::LIST);
      CHECK(ccs->value().get_type() == ossia::val_type::LIST);
    }
#endif
  });
}

TEST_CASE(
    "Midi sustain: a Pedal saved as a toggle is a momentary button, down if it was",
    "[avnd][upgrade][midi_sustain]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    auto sustain = make_process(dctx, midi_sustain_uuid);
    if(!sustain)
      SKIP("Midi sustain is not built");
    REQUIRE(qobject_cast<Process::Button*>(inlet_named(*sustain, "Pedal")));

    for(bool down : {true, false})
    {
      const auto id = replace_inlet(*sustain, "Pedal", [&](Id<Process::Port> id) {
        return new Process::Toggle{down, "Pedal", id, sustain.get()};
      });
      for(auto roundtrip : {&roundtrip_json, &roundtrip_datastream})
      {
        std::unique_ptr<Process::ProcessModel> loaded{roundtrip(*sustain, dctx, nullptr)};
        REQUIRE(loaded);
        auto* pedal = qobject_cast<Process::Button*>(inlet_named(*loaded, "Pedal"));
        REQUIRE(pedal);
        CHECK(pedal->id() == id);
        CHECK(ossia::convert<bool>(pedal->value()) == down);
      }
    }

    // The presets that hold the pedal down
    auto* pedal = inlet_named(*sustain, "Pedal");
    Process::Preset p;
    p.key.key = sustain->concreteKey();
    p.data = R"([[1,{"String":"All"}],[2,{"Bool":true}]])";
    sustain->loadPreset(p);
    CHECK(ossia::convert<bool>(pedal->value()));
    p.data = R"([[2,{"Bool":false},"Pedal"]])";
    sustain->loadPreset(p);
    CHECK(!ossia::convert<bool>(pedal->value()));
  });
}

TEST_CASE(
    "A score with a Midi CC snapshot, which is no more, loads without it",
    "[avnd][upgrade][midi_cc_snapshot]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* sustain = score::test::add_process(*doc, midi_sustain_uuid, {});
    auto* glide = score::test::add_process(*doc, midi_glide_uuid, {});
    if(!sustain || !glide)
      SKIP("the MIDI objects are not built");
    REQUIRE(!ctx.interfaces<Process::ProcessFactoryList>().get(
        UuidKey<Process::ProcessModel>::fromString(midi_cc_snapshot_uuid)));

    // A cable from the object that will be gone into one that stays
    auto& model = score::IDocument::get<Scenario::ScenarioDocumentModel>(*doc);
    CommandDispatcher<>{doc->context().commandStack}.submit<Dataflow::CreateCable>(
        model, Id<Process::Cable>{1}, Process::CableType::ImmediateGlutton,
        *sustain->outlets()[0], *glide->inlets()[0]);
    REQUIRE(model.cables.size() == 1);

    // The score as a version that had the Midi CC snapshot saved it
    const auto processes = score::test::base_interval(*doc).processes.size();
    QByteArray json = score::test::save_as_json(*doc);
    const auto count = json.count(midi_sustain_uuid.toUtf8());
    REQUIRE(count > 0);
    json.replace(midi_sustain_uuid.toUtf8(), midi_cc_snapshot_uuid.toUtf8());

    auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
    auto* loaded = ctx.docManager.loadDocument(
        ctx, QStringLiteral("cc-snapshot"), json, JSONObject::type(), *delegates.begin());
    QApplication::processEvents();
    QApplication::processEvents();
    REQUIRE(loaded);

    // The glide stays, the snapshot does not come back
    auto& itv = score::test::base_interval(*loaded);
    CHECK(itv.processes.size() == processes - 1);
    const auto has = [&](const QString& uuid) {
      const auto key = UuidKey<Process::ProcessModel>::fromString(uuid);
      return ossia::any_of(itv.processes, [&](auto& p) { return p.concreteKey() == key; });
    };
    CHECK(has(midi_glide_uuid));
    CHECK(!has(midi_cc_snapshot_uuid));
    CHECK(!has(midi_sustain_uuid));

    // And the score saves again
    CHECK(!score::test::save_as_json(*loaded).isEmpty());
  });
}
