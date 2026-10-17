// The pattern-select port is the selector: the model property follows it, so
// the grid the layer draws changes with the port rather than only the sound.
// Before, the port reached the executor alone, and moving it did nothing at all
// in the editor.

#include <Process/Dataflow/Port.hpp>
#include <Process/ProcessList.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/network/value/value.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QMimeData>

#include <Patternist/PatternEdit.hpp>
#include <Patternist/PatternModel.hpp>
#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

namespace
{
std::unique_ptr<Patternist::ProcessModel> makeProcess()
{
  return std::make_unique<Patternist::ProcessModel>(
      TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{0}, nullptr);
}
}

TEST_CASE("patternist: the port selects the current pattern", "[midi][pattern]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto proc = makeProcess();

    REQUIRE(proc->patternSelect);
    REQUIRE(proc->currentPattern() == 0);

    proc->patternSelect->setValue(2);
    CHECK(proc->currentPattern() == 2);

    proc->patternSelect->setValue(0);
    CHECK(proc->currentPattern() == 0);
  });
}

TEST_CASE("patternist: selecting past the end grows the list", "[midi][pattern]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto proc = makeProcess();

    const auto before = std::ssize(proc->patterns());
    proc->patternSelect->setValue(int(before) + 1);

    CHECK(std::ssize(proc->patterns()) == before + 2);
    CHECK(proc->currentPattern() == int(before) + 1);
  });
}

TEST_CASE("patternist: a negative selection is clamped", "[midi][pattern]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto proc = makeProcess();

    proc->patternSelect->setValue(-5);
    CHECK(proc->currentPattern() == 0);
  });
}

TEST_CASE(
    "patternist: the switch quantization port holds an ossia rate", "[midi][pattern]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto proc = makeProcess();

    REQUIRE(proc->switchQuantification);

    // The scale QuantificationWidget uses: a rate fed straight to
    // get_quantification_dates(), never the inverse "fraction of a whole note".
    const double init = ossia::convert<float>(proc->switchQuantification->value());
    CHECK(init == 1.); // one bar
  });
}

TEST_CASE(
    "patternist: a document written before the ports still loads", "[midi][pattern]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QObject* owner = new QObject{&doc->model()};

    Patternist::ProcessModel proc{
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, owner};
    proc.setChannel(9);
    proc.setCurrentPattern(3); // grows the list to four
    REQUIRE(std::ssize(proc.patterns()) == 4);

    auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
    const Process::ProcessModel& as_base = proc;

    JSONReader reader;
    reader.readFrom(as_base);
    rapidjson::Document jdoc = toValue(reader);

    // What a document saved before the two control inlets existed looks like.
    if(jdoc.HasMember("PatternSelect"))
      jdoc.RemoveMember("PatternSelect");
    if(jdoc.HasMember("SwitchQuantification"))
      jdoc.RemoveMember("SwitchQuantification");

    JSONWriter writer{jdoc};
    auto clone = deserialize_interface(pl, writer, doc->context(), owner);
    REQUIRE(clone);
    auto pat = dynamic_cast<Patternist::ProcessModel*>(clone);
    REQUIRE(pat);

    CHECK(pat->channel() == 9);
    CHECK(pat->currentPattern() == 3);
    CHECK(std::ssize(pat->patterns()) == 4);

    // The ports are rebuilt at their defaults, and the selector must agree with
    // the pattern the document was saved on rather than point at another one.
    REQUIRE(pat->patternSelect);
    REQUIRE(pat->switchQuantification);
    CHECK(pat->inlets().size() == 7);
    CHECK(ossia::convert<int>(pat->patternSelect->value()) == 3);

    delete clone;
  });
}

TEST_CASE("patternist: a document written with the ports round-trips", "[midi][pattern]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QObject* owner = new QObject{&doc->model()};

    Patternist::ProcessModel proc{
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, owner};
    proc.setCurrentPattern(2);
    proc.switchQuantification->setValue(4.); // 1/4

    auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
    const Process::ProcessModel& as_base = proc;

    JSONReader reader;
    reader.readFrom(as_base);
    const rapidjson::Document jdoc = toValue(reader);
    JSONWriter writer{jdoc};

    auto clone = deserialize_interface(pl, writer, doc->context(), owner);
    REQUIRE(clone);
    auto pat = dynamic_cast<Patternist::ProcessModel*>(clone);
    REQUIRE(pat);

    CHECK(pat->currentPattern() == 2);
    CHECK(ossia::convert<int>(pat->patternSelect->value()) == 2);
    CHECK(ossia::convert<float>(pat->switchQuantification->value()) == 4.);

    delete clone;
  });
}

TEST_CASE(
    "patternist: lane labels, mute and solo round-trip; short lanes are padded",
    "[midi][pattern]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QObject* owner = new QObject{&doc->model()};

    Patternist::ProcessModel proc{
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, owner};
    proc.setLaneLabels(Patternist::LaneLabels::DrumName);
    auto p = proc.patterns()[0];
    p.lanes[0].mute = true;
    p.lanes[1].solo = true;
    proc.setPattern(0, p);

    auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
    const Process::ProcessModel& as_base = proc;

    SECTION("JSON")
    {
      JSONReader reader;
      reader.readFrom(as_base);
      rapidjson::Document jdoc = toValue(reader);

      // A lane shorter than the pattern, as a hand-written file can have.
      auto& lane0 = jdoc["Patterns"][0]["Lanes"][0];
      lane0["Pattern"].SetString("1-1", jdoc.GetAllocator());

      JSONWriter writer{jdoc};
      auto clone = deserialize_interface(pl, writer, doc->context(), owner);
      REQUIRE(clone);
      auto pat = dynamic_cast<Patternist::ProcessModel*>(clone);
      REQUIRE(pat);

      CHECK(pat->laneLabels() == Patternist::LaneLabels::DrumName);
      const auto& lanes = pat->patterns()[0].lanes;
      CHECK(lanes[0].mute);
      CHECK_FALSE(lanes[0].solo);
      CHECK(lanes[1].solo);
      CHECK(lanes[0].pattern.size() == 16);
      CHECK(lanes[0].pattern[1] == Patternist::Note::Rest);
      CHECK(lanes[0].pattern[2] == Patternist::Note::Note);
      delete clone;
    }

    SECTION("DataStream")
    {
      QByteArray arr;
      DataStreamReader reader{&arr};
      reader.readFrom(as_base);
      DataStreamWriter writer{arr};
      auto clone = deserialize_interface(pl, writer, doc->context(), owner);
      REQUIRE(clone);
      auto pat = dynamic_cast<Patternist::ProcessModel*>(clone);
      REQUIRE(pat);
      CHECK(pat->laneLabels() == Patternist::LaneLabels::DrumName);
      CHECK(pat->patterns() == proc.patterns());
      delete clone;
    }
  });
}

TEST_CASE(
    "patternist: a pattern goes through the clipboard unchanged", "[midi][pattern]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto p = [] {
      using namespace Patternist;
      Pattern p;
      p.length = 4;
      p.division = 8;
      p.lanes.push_back(Lane{{Note::Note, Note::Rest, Note::Legato, Note::Rest}, 36});
      p.lanes.push_back(Lane{{Note::Rest, Note::Note, Note::Rest, Note::Note}, 255});
      return p;
    }();
    p.lanes[0].solo = true;
    p.lanes[1].mute = true;
    Patternist::normalize(p);
    p.lanes[0].params[2].velocityRandom = 0.25f;
    p.lanes[0].params[2].pitchRandom = 3;

    std::unique_ptr<QMimeData> mime{Patternist::patternToMime(p)};
    auto back = Patternist::patternFromMime(*mime);
    REQUIRE(back);
    CHECK(*back == p);

    // Plain text holding the same JSON is accepted too.
    QMimeData text;
    text.setText(mime->text());
    auto fromText = Patternist::patternFromMime(text);
    REQUIRE(fromText);
    CHECK(*fromText == p);

    QMimeData junk;
    junk.setText(QStringLiteral("not a pattern"));
    CHECK_FALSE(Patternist::patternFromMime(junk));

    // Copied steps are not a pattern, nor the other way around.
    const auto steps = Patternist::copySteps(p, {0, {{2, 3, 4, 5}, {2, 3}}});
    std::unique_ptr<QMimeData> stepsMime{Patternist::stepsToMime(steps)};
    auto stepsBack = Patternist::stepsFromMime(*stepsMime);
    REQUIRE(stepsBack);
    REQUIRE(stepsBack->lanes.size() == 2);
    CHECK(stepsBack->lanes[0].length == 4);
    CHECK(stepsBack->lanes[1].length == 2);
    CHECK(stepsBack->lanes[0].pattern == steps.lanes[0].pattern);
    CHECK(stepsBack->lanes[0].params[0] == steps.lanes[0].params[0]);
    CHECK_FALSE(Patternist::patternFromMime(*stepsMime));
    CHECK_FALSE(Patternist::stepsFromMime(*mime));
  });
}
