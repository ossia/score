#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/TimeValue.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace
{
static const auto gain_uuid = UuidKey<Process::ProcessModel>::fromString(
    QStringLiteral("6c158669-0f81-41c9-8cc6-45820dcda867"));
static const auto math_audio_uuid = UuidKey<Process::ProcessModel>::fromString(
    QStringLiteral("13e1f4b0-1c2c-40e6-93ad-dfc91aac5335"));

std::vector<Process::ControlInlet*> controls(const Process::ProcessModel& p)
{
  std::vector<Process::ControlInlet*> res;
  for(auto* inlet : p.inlets())
    if(auto* ctl = qobject_cast<Process::ControlInlet*>(inlet))
      res.push_back(ctl);
  return res;
}

Process::ProcessModel* make_process(
    const score::DocumentContext& dctx, const UuidKey<Process::ProcessModel>& key,
    int id, QObject* parent)
{
  auto& pl = dctx.app.interfaces<Process::ProcessFactoryList>();
  auto* fac = pl.get(key);
  if(!fac)
    return nullptr;
  return fac->make(
      TimeVal::fromMsecs(1000), fac->customConstructionData(),
      Id<Process::ProcessModel>{id}, dctx, parent);
}

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

TEST_CASE("avnd audio process models: controls survive save/load", "[avnd][audio][serialization]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& dctx = doc->context();
    QObject* parent = &doc->model();

    // --- ao::Gain --------------------------------------------------------
    {
      auto* proc = make_process(dctx, gain_uuid, 4501, parent);
      REQUIRE(proc != nullptr);
      CHECK(proc->concreteKey() == gain_uuid);

      // ao::Gain has exactly one control: the Gain knob (range 0..5, init 0).
      auto ctls = controls(*proc);
      REQUIRE(ctls.size() == 1);
      ctls[0]->setValue(3.5f);

      // DataStream
      {
        auto* loaded = roundtrip_datastream(*proc, dctx, parent);
        REQUIRE(loaded != nullptr);
        CHECK(loaded->concreteKey() == gain_uuid);
        CHECK(loaded->inlets().size() == proc->inlets().size());
        CHECK(loaded->outlets().size() == proc->outlets().size());

        auto loaded_ctls = controls(*loaded);
        REQUIRE(loaded_ctls.size() == 1);
        CHECK(loaded_ctls[0]->value() == ossia::value{3.5f});
        delete loaded;
      }

      // JSON
      {
        auto* loaded = roundtrip_json(*proc, dctx, parent);
        REQUIRE(loaded != nullptr);
        CHECK(loaded->concreteKey() == gain_uuid);

        auto loaded_ctls = controls(*loaded);
        REQUIRE(loaded_ctls.size() == 1);
        CHECK(loaded_ctls[0]->value() == ossia::value{3.5f});
        delete loaded;
      }
      delete proc;
    }

    // --- Nodes::MathAudioFilter ------------------------------------------
    {
      auto* proc = make_process(dctx, math_audio_uuid, 4502, parent);
      REQUIRE(proc != nullptr);

      // Controls: Expression (line edit), a, b, c.
      auto ctls = controls(*proc);
      REQUIRE(ctls.size() == 4);

      const std::string expr = "out[0] := x[0] * 0.25;";
      ctls[0]->setValue(expr);
      ctls[1]->setValue(0.75f); // a

      // DataStream
      {
        auto* loaded = roundtrip_datastream(*proc, dctx, parent);
        REQUIRE(loaded != nullptr);
        CHECK(loaded->concreteKey() == math_audio_uuid);

        auto loaded_ctls = controls(*loaded);
        REQUIRE(loaded_ctls.size() == 4);
        CHECK(loaded_ctls[0]->value() == ossia::value{expr});
        CHECK(loaded_ctls[1]->value() == ossia::value{0.75f});
        delete loaded;
      }

      // JSON
      {
        auto* loaded = roundtrip_json(*proc, dctx, parent);
        REQUIRE(loaded != nullptr);
        CHECK(loaded->concreteKey() == math_audio_uuid);

        auto loaded_ctls = controls(*loaded);
        REQUIRE(loaded_ctls.size() == 4);
        CHECK(loaded_ctls[0]->value() == ossia::value{expr});
        CHECK(loaded_ctls[1]->value() == ossia::value{0.75f});
        delete loaded;
      }
      delete proc;
    }
  });
}
