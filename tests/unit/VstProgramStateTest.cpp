// A VST 2 process saves the program its plug-in is on with its state, in
// documents (JSON and binary) and presets, and a load puts the plug-in back on
// it before the chunk or the parameters, which then are what was saved. A
// state saved without a program leaves the plug-in on the one it was set up
// on. A process whose plug-in is not installed keeps the program it was saved
// with, and saves it back.

#include <Process/Dataflow/Port.hpp>
#include <Process/Preset.hpp>
#include <Process/ProcessList.hpp>

#include <Vst/ApplicationPlugin.hpp>
#include <Vst/Control.hpp>
#include <Vst/EffectModel.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/presenter/DocumentManager.hpp>

#include <QCoreApplication>
#include <QFile>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <functional>
#include <memory>

namespace
{
struct Fixture
{
  const char* path;
  int32_t id;
};

const Fixture params_fixture{SCORE_VST_UNFLAGGED_INSTRUMENT, "sUnI"_4c};
const Fixture chunk_fixture{SCORE_VST_CHUNK_INSTRUMENT, "sUnC"_4c};

void install(const score::GUIApplicationContext& ctx, std::vector<Fixture> fixtures)
{
  auto& app = ctx.applicationPlugin<vst::ApplicationPlugin>();
  app.vst_infos.clear();
  for(auto& f : fixtures)
  {
    vst::VSTInfo info;
    info.path = QString::fromUtf8(f.path);
    info.prettyName = "UnflaggedInstrument";
    info.uniqueID = f.id;
    info.controls = 1;
    info.isValid = true;
    app.vst_infos.push_back(info);
    app.vst_modules.insert({f.id, nullptr});
  }
}

using ModelPtr = std::unique_ptr<vst::Model>;

ModelPtr make(const Fixture& f)
{
  auto m = std::make_unique<vst::Model>(
      TimeVal::fromMsecs(1000), QString::number(f.id), Id<Process::ProcessModel>{1},
      nullptr);
  REQUIRE(m->fx);
  return m;
}

const Process::ProcessModel& base(const vst::Model& m)
{
  return m;
}

QByteArray to_json(const vst::Model& m)
{
  return score::marshall<JSONObject>(base(m)).toByteArray();
}

ModelPtr from_json(const QByteArray& json)
{
  const auto doc = readJson(json);
  JSONObject::Deserializer des{doc};
  return std::make_unique<vst::Model>(des, nullptr);
}

QByteArray to_bytes(const vst::Model& m)
{
  return score::marshall<DataStream>(base(m));
}

//! As the removal and duplication commands load a process: through its
//! factory, which reads the key they write first
ModelPtr from_bytes(const QByteArray& bytes)
{
  auto& ctx = score::GUIAppContext();
  auto* doc = ctx.docManager.currentDocument();
  REQUIRE(doc);
  DataStream::Deserializer des{bytes};
  auto* p = deserialize_interface(
      ctx.interfaces<Process::ProcessFactoryList>(), des, doc->context(), nullptr);
  auto* m = qobject_cast<vst::Model*>(p);
  REQUIRE(m);
  return ModelPtr{m};
}

int program(vst::Model& m)
{
  return int(m.dispatch(effGetProgram));
}
float level(vst::Model& m)
{
  return m.fx->getParameter(0);
}
float control_level(const vst::Model& m)
{
  for(auto* p : m.inlets())
    if(auto c = qobject_cast<vst::ControlInlet*>(p))
      return ossia::convert<float>(c->value());
  FAIL("no control");
  return 0.f;
}
//! Programs selected since the plug-in was opened, outside a program change
intptr_t unwrapped(vst::Model& m)
{
  return m.dispatch(effVendorSpecific, 0);
}

//! On program 5 (level 0.25), its level then edited to 0.9
ModelPtr on_program_5(const Fixture& f)
{
  auto m = make(f);
  m->selectProgram(5);
  REQUIRE(level(*m) == 0.25f);
  m->fx->setParameter(0, 0.9f);
  return m;
}

void check_on_program_5(vst::Model& m)
{
  REQUIRE(m.fx);
  CHECK(program(m) == 5);
  // The state after the program, not the program's own level
  CHECK(level(m) == 0.9f);
  CHECK(control_level(m) == 0.9f);
  CHECK(unwrapped(m) == 0);
}

void each_fixture(const std::function<void(const Fixture&)>& fn)
{
  for(auto* f : {&params_fixture, &chunk_fixture})
  {
    INFO((f == &params_fixture ? "parameters" : "chunk"));
    fn(*f);
  }
}
}

TEST_CASE(
    "VST 2: a process reloads on the program its plug-in was on, with its state",
    "[vst][programs]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    REQUIRE(score::test::new_document(ctx));
    install(ctx, {params_fixture, chunk_fixture});
    each_fixture([&](const Fixture& f) {
      auto m = on_program_5(f);
      {
        INFO("JSON");
        auto again = from_json(to_json(*m));
        check_on_program_5(*again);
      }
      {
        INFO("binary");
        auto again = from_bytes(to_bytes(*m));
        check_on_program_5(*again);
      }
      {
        INFO("preset");
        const auto preset = base(*m).savePreset();
        auto other = make(f);
        CHECK(program(*other) == 1);
        static_cast<Process::ProcessModel&>(*other).loadPreset(preset);
        check_on_program_5(*other);
      }
    });
    QCoreApplication::processEvents();
  });
}

TEST_CASE(
    "VST 2: a state saved without a program leaves the plug-in on its own",
    "[vst][programs]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    REQUIRE(score::test::new_document(ctx));
    install(ctx, {params_fixture, chunk_fixture});
    each_fixture([&](const Fixture& f) {
      auto m = on_program_5(f);
      auto doc = readJson(to_json(*m));
      REQUIRE(doc.HasMember("Program"));
      doc.RemoveMember("Program");
      JSONObject::Deserializer des{doc};
      vst::Model again{des, nullptr};
      REQUIRE(again.fx);
      // The program it was set up on, selected once, and the saved values
      CHECK(program(again) == 1);
      CHECK(again.dispatch(effVendorSpecific, 1) == 1);
      CHECK(level(again) == 0.9f);
    });

    // A preset written before programs were saved (tests/testdata/vst2)
    QFile file{
        QStringLiteral(SCORE_VST2_DOCUMENTS_DIR) + "/before-midi-ports/unflagged.scp"};
    REQUIRE(file.open(QIODevice::ReadOnly));
    auto preset = Process::Preset::fromJson(
        ctx.interfaces<Process::ProcessFactoryList>(), file.readAll());
    REQUIRE(preset);
    auto m = make(params_fixture);
    static_cast<Process::ProcessModel&>(*m).loadPreset(*preset);
    CHECK(program(*m) == 1);
    CHECK(m->dispatch(effVendorSpecific, 1) == 1);
    CHECK(level(*m) == 0.3f);
    QCoreApplication::processEvents();
  });
}

TEST_CASE(
    "VST 2: a process whose plug-in is not installed keeps its program",
    "[vst][programs]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    REQUIRE(score::test::new_document(ctx));
    install(ctx, {params_fixture, chunk_fixture});
    each_fixture([&](const Fixture& f) {
      auto m = on_program_5(f);
      const auto json = to_json(*m);
      const auto bytes = to_bytes(*m);

      install(ctx, {});
      auto missing_json = from_json(json);
      auto missing_bytes = from_bytes(bytes);
      CHECK(!missing_json->fx);
      CHECK(!missing_bytes->fx);
      const auto json_back = to_json(*missing_json);
      const auto bytes_back = to_bytes(*missing_bytes);
      const auto saved_back = readJson(json_back);
      REQUIRE(saved_back.HasMember("Program"));
      CHECK(saved_back["Program"].GetInt() == 5);

      install(ctx, {params_fixture, chunk_fixture});
      {
        INFO("JSON");
        check_on_program_5(*from_json(json_back));
      }
      {
        INFO("binary");
        check_on_program_5(*from_bytes(bytes_back));
      }
    });
    QCoreApplication::processEvents();
  });
}
