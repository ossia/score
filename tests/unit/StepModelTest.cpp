// The step sequencer's model: sequences, the Sequence port, the commands, and
// documents saved before sequences and the Duration port existed.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/ProcessList.hpp>

#include <Audio/Settings/Model.hpp>
#include <Media/Step/Commands.hpp>
#include <Media/Step/Model.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QCoreApplication>
#include <QPointer>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Media::Step::Model;

namespace
{
Model* reload(
    const score::GUIApplicationContext& ctx, score::Document& doc, QObject* owner,
    rapidjson::Document& jdoc)
{
  auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
  JSONWriter writer{jdoc};
  auto clone = deserialize_interface(pl, writer, doc.context(), owner);
  REQUIRE(clone);
  auto m = dynamic_cast<Model*>(clone);
  REQUIRE(m);
  return m;
}

rapidjson::Document save(const Model& m)
{
  const Process::ProcessModel& as_base = m;
  JSONReader reader;
  reader.readFrom(as_base);
  return toValue(reader);
}
}

TEST_CASE("step sequencer: the Sequence port selects, and grows the list", "[step]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Model m{TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{0}, nullptr};
    REQUIRE(m.inlets().size() == 3);
    REQUIRE(m.sequences().size() == 1);
    const auto first = m.steps();

    m.sequenceSelect->setValue(2);
    CHECK(m.currentSequence() == 2);
    REQUIRE(m.sequences().size() == 3);
    // New sequences have the length of the one shown
    CHECK(m.stepCount() == int(first.size()));

    // Counts are per sequence
    m.setStepCount(5);
    CHECK(m.stepCount() == 5);
    m.sequenceSelect->setValue(0);
    CHECK(m.steps() == first);

    m.sequenceSelect->setValue(-3);
    CHECK(m.currentSequence() == 0);
  });
}

TEST_CASE("step sequencer: a new count reaches whoever follows the steps", "[step]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Model m{TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{0}, nullptr};
    // Connected by name: the signal functions are inline, and the address of
    // the test executable's copy is not the plug-in's, which a pointer-to-member
    // connection compares against.
    auto* witness = new QObject;
    QPointer<QObject> alive{witness};
    REQUIRE(QObject::connect(&m, SIGNAL(stepsChanged()), witness, SLOT(deleteLater())));
    // The executor listens to stepsChanged, so a count change must emit it
    // along with stepCountChanged.
    m.setStepCount(12);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(!alive);
    delete alive.data();
  });
}

TEST_CASE("step sequencer: commands act on the sequence they were made on", "[step]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    auto* m = dynamic_cast<Model*>(score::test::add_process(
        *doc, QStringLiteral("c953fa55-65f0-4b93-8bfc-54780250d2b8"), {}));
    REQUIRE(m);

    m->sequenceSelect->setValue(1);
    const auto before1 = m->steps();
    Media::ChangeSteps edit{*m, ossia::float_vector{0.f, 0.f}};
    Media::SetStepCount count{*m, 3};
    edit.redo(dctx);
    CHECK(m->steps() == ossia::float_vector{0.f, 0.f});

    // Shown: sequence 0; undo still restores sequence 1.
    m->sequenceSelect->setValue(0);
    const auto steps0 = m->steps();
    edit.undo(dctx);
    CHECK(m->sequences()[1] == before1);
    CHECK(m->steps() == steps0);

    count.redo(dctx);
    CHECK(m->sequences()[1].size() == 3);
    CHECK(m->steps() == steps0);
    count.undo(dctx);
    CHECK(m->sequences()[1] == before1);
  });
}

TEST_CASE("step sequencer: sequences and ports round-trip", "[step]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QObject* owner = new QObject{&doc->model()};
    Model m{TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, owner};
    m.sequenceSelect->setValue(2);
    m.setSteps({0.1f, 0.2f, 0.3f});
    m.switchQuantification->setValue(4.);
    m.stepDuration->setValue(ossia::vec2f{0.125f, 1.f});
    m.setMin(-2.);

    auto jdoc = save(m);
    auto* r = reload(ctx, *doc, owner, jdoc);
    CHECK(r->sequences() == m.sequences());
    CHECK(r->currentSequence() == 2);
    CHECK(ossia::convert<int>(r->sequenceSelect->value()) == 2);
    CHECK(ossia::convert<float>(r->switchQuantification->value()) == 4.f);
    CHECK(r->stepDuration->value() == ossia::value{ossia::vec2f{0.125f, 1.f}});
    CHECK(r->min() == -2.);
    CHECK(r->outlet->id() == Id<Process::Port>{0});
    delete r;

    // DataStream too: undo/redo and copy/paste
    const Process::ProcessModel& as_base = m;
    auto bytes = score::marshall<DataStream>(as_base);
    DataStreamWriter w{bytes};
    auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
    auto* d = dynamic_cast<Model*>(deserialize_interface(pl, w, doc->context(), owner));
    REQUIRE(d);
    CHECK(d->sequences() == m.sequences());
    CHECK(d->stepDuration->value() == m.stepDuration->value());
    delete d;
  });
}

TEST_CASE("step sequencer: a document from before sequences plays as it did", "[step]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QObject* owner = new QObject{&doc->model()};
    Model m{TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, owner};

    // What a document saved by the previous release has: one list of steps,
    // a count and a duration in samples.
    auto jdoc = save(m);
    for(auto key : {"SequenceSelect", "SwitchQuantification", "StepDuration", "Sequences",
                    "Sequence"})
      if(jdoc.HasMember(key))
        jdoc.RemoveMember(key);
    auto& alloc = jdoc.GetAllocator();
    rapidjson::Value steps{rapidjson::kArrayType};
    for(float f : {0.25f, 0.5f, 0.75f})
      steps.PushBack(f, alloc);
    jdoc.AddMember("Steps", steps, alloc);
    jdoc.AddMember("StepCount", 3, alloc);
    jdoc.AddMember("StepDur", 24000, alloc);

    auto* r = reload(ctx, *doc, owner, jdoc);
    REQUIRE(r->sequences().size() == 1);
    CHECK(r->steps() == ossia::float_vector{0.25f, 0.5f, 0.75f});
    CHECK(r->currentSequence() == 0);
    REQUIRE(r->inlets().size() == 3);
    CHECK(ossia::convert<int>(r->sequenceSelect->value()) == 0);
    CHECK(ossia::convert<float>(r->switchQuantification->value()) == 1.f);

    // The same length, in seconds and not synced
    const double rate = ctx.settings<Audio::Settings::Model>().getRate();
    const auto dur = ossia::convert<ossia::vec2f>(r->stepDuration->value());
    CHECK(dur[0] == Catch::Approx(24000. / rate));
    CHECK(dur[1] == 0.f);
    CHECK(r->outlet->id() == Id<Process::Port>{0});
    delete r;
  });
}
