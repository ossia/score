// Notes recorded into a piano roll (Recording::overdubNotes): where the notes
// of a take, dated in the box, land in the piano roll, and which of its notes
// a replacing take removes.
//  - the piano roll's time is the box's moved by its start offset;
//  - round its loop when it loops, a note cut at the loop's end;
//  - replacing removes the notes sounding over the span recorded, all of them
//    when the take covers a whole loop; merging removes none.

#include <Midi/MidiNote.hpp>
#include <Midi/MidiProcess.hpp>
#include <Automation/AutomationModel.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>
#include <Recording/Takes/OverdubNotes.hpp>
#include <Recording/Takes/RecordedCurves.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <score_test/App.hpp>

using namespace Recording;
using Catch::Approx;

namespace
{
constexpr int64_t sec = 705600000;

void addNote(Midi::ProcessModel& p, int id, double start, double dur, int pitch)
{
  p.notes.add(new Midi::Note{
      Id<Midi::Note>{id}, Midi::NoteData{start, dur, uint8_t(pitch), 100}, &p});
}

bool removes(const NoteOverdub& o, int id)
{
  for(auto& i : o.remove)
    if(i.val() == id)
      return true;
  return false;
}
}

TEST_CASE("Recorded notes land in a piano roll's time", "[recording][overdub]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    // 10 s long
    Midi::ProcessModel p{TimeVal{10 * sec}, Id<Process::ProcessModel>{1}, nullptr};
    addNote(p, 0, 0.1, 0.1, 60); // 1 s to 2 s
    addNote(p, 1, 0.5, 0.1, 62); // 5 s to 6 s

    std::vector<RecordedNote> take{{2 * sec, sec, 64, 90, 0}};

    // Merging: added at 2 s, nothing removed
    auto m = overdubNotes(p, take, sec + sec / 2, 3 * sec, false);
    REQUIRE(m.add.size() == 1);
    CHECK(m.add[0].start() == Approx(0.2));
    CHECK(m.add[0].duration() == Approx(0.1));
    CHECK(m.add[0].pitch() == 64);
    CHECK(m.add[0].velocity() == 90);
    CHECK(m.remove.empty());

    // Replacing over 1.5 s to 3 s: the note of 1 s to 2 s sounds there
    auto r = overdubNotes(p, take, sec + sec / 2, 3 * sec, true);
    CHECK(removes(r, 0));
    CHECK(!removes(r, 1));

    // A start offset moves the box's time: content = box + offset
    p.setStartOffset(TimeVal{sec});
    auto o = overdubNotes(p, take, 2 * sec, 3 * sec, false);
    REQUIRE(o.add.size() == 1);
    CHECK(o.add[0].start() == Approx(0.3));
  });
}

TEST_CASE("Recorded notes go round a looping piano roll", "[recording][overdub]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Midi::ProcessModel p{TimeVal{8 * sec}, Id<Process::ProcessModel>{1}, nullptr};
    p.setLoops(true);
    p.setLoopDuration(TimeVal{2 * sec});
    addNote(p, 0, 0.0, 0.05, 60);  // 0 to 0.4 s
    addNote(p, 1, 0.15, 0.05, 62); // 1.2 s to 1.6 s

    // At 5 s of the box: 1 s into the third loop; held 1.5 s, cut at the loop
    std::vector<RecordedNote> take{{5 * sec, sec + sec / 2, 64, 90, 0}};
    auto m = overdubNotes(p, take, 5 * sec, 6 * sec, false);
    REQUIRE(m.add.size() == 1);
    CHECK(m.add[0].start() == Approx(1. / 8.));
    CHECK(m.add[0].duration() == Approx(1. / 8.));

    // Replacing from 4.9 s to 5.3 s of the box: 0.9 s to 1.3 s of the loop
    auto r = overdubNotes(p, take, 4 * sec + 9 * sec / 10, 5 * sec + 3 * sec / 10, true);
    CHECK(!removes(r, 0));
    CHECK(removes(r, 1));

    // A take as long as a loop replaces all of it
    auto all = overdubNotes(p, take, 4 * sec, 6 * sec, true);
    CHECK(removes(all, 0));
    CHECK(removes(all, 1));
  });
}

namespace
{
//! The value of a recorded curve at x
double valueAt(const RecordedCurve& c, double x)
{
  for(const auto& s : c.segments)
    if(x >= s.start.x() && x <= s.end.x())
    {
      const double t
          = s.end.x() > s.start.x() ? (x - s.start.x()) / (s.end.x() - s.start.x()) : 1.;
      return c.min + (s.start.y() + t * (s.end.y() - s.start.y())) * (c.max - c.min);
    }
  return -1000.;
}
}

TEST_CASE("Values recorded into an automation leave the segments they do not reach as they were", "[recording][overdub][segments]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    // 10 s: a bent segment to 0.3, a straight one to 0.6, a bent one to 1
    Automation::ProcessModel a{TimeVal{10 * sec}, Id<Process::ProcessModel>{1}, nullptr};
    const auto power = Metadata<ConcreteKey_k, Curve::PowerSegment>::get();
    std::vector<Curve::SegmentData> segs(3);
    const double xs[]{0., 0.3, 0.6, 1.};
    const double ys[]{0., 0.8, 0.2, 1.};
    const double gammas[]{3., 1., 0.4};
    for(int i = 0; i < 3; i++)
    {
      segs[i].id = Id<Curve::SegmentModel>{i};
      segs[i].start = {xs[i], ys[i]};
      segs[i].end = {xs[i + 1], ys[i + 1]};
      segs[i].type = power;
      segs[i].specificSegmentData = QVariant::fromValue(Curve::PowerSegmentData{gammas[i]});
      if(i > 0)
        segs[i].previous = Id<Curve::SegmentModel>{i - 1};
      if(i < 2)
        segs[i].following = Id<Curve::SegmentModel>{i + 1};
    }
    a.curve().fromCurveData(segs);
    REQUIRE(a.curve().sortedSegments().size() == 3);

    // Values between 4 s and 5 s: inside the middle segment
    std::vector<ValueTake::Event> ev{
        {4 * sec, ossia::value{0.5f}}, {4 * sec + sec / 2, ossia::value{0.9f}}};
    CurveOptions opt;
    opt.stepped = true;
    auto c = spliceCurve(a, ev, 0, 4 * sec, 5 * sec, opt);
    REQUIRE(c);
    REQUIRE(c->segments.size() > 3);
    CHECK(c->min == Approx(0.));
    CHECK(c->max == Approx(1.));

    // The first and the last: the bent segments they were, end to end
    const auto& first = c->segments.front();
    const auto& last = c->segments.back();
    CHECK(first.type == power);
    CHECK(first.start.x() == Approx(0.));
    CHECK(first.end.x() == Approx(0.3));
    CHECK(first.start.y() == Approx(0.));
    CHECK(first.end.y() == Approx(0.8));
    CHECK(first.specificSegmentData.value<Curve::PowerSegmentData>().gamma == Approx(3.));
    CHECK(last.type == power);
    CHECK(last.start.x() == Approx(0.6));
    CHECK(last.end.x() == Approx(1.));
    CHECK(last.specificSegmentData.value<Curve::PowerSegmentData>().gamma == Approx(0.4));

    // One after the other, without a hole, each knowing its neighbours
    for(std::size_t i = 0; i < c->segments.size(); i++)
    {
      const auto& seg = c->segments[i];
      CHECK(seg.id == Id<Curve::SegmentModel>{int(i)});
      if(i > 0)
      {
        CHECK(seg.start.x() == Approx(c->segments[i - 1].end.x()));
        REQUIRE(seg.previous);
        CHECK(*seg.previous == c->segments[i - 1].id);
      }
      else
        CHECK(!seg.previous);
      if(i + 1 < c->segments.size())
      {
        REQUIRE(seg.following);
        CHECK(*seg.following == c->segments[i + 1].id);
      }
      else
        CHECK(!seg.following);
    }

    // The values where recorded; the middle segment as it was around them
    CHECK(valueAt(*c, 0.42) == Approx(0.5));
    CHECK(valueAt(*c, 0.47) == Approx(0.9));
    CHECK(valueAt(*c, 0.35) == Approx(0.8 + (0.35 - 0.3) / 0.3 * (0.2 - 0.8)).margin(0.01));
    CHECK(valueAt(*c, 0.55) == Approx(0.8 + (0.55 - 0.3) / 0.3 * (0.2 - 0.8)).margin(0.01));

    // It loads as a curve
    Automation::ProcessModel b{TimeVal{10 * sec}, Id<Process::ProcessModel>{2}, nullptr};
    b.curve().fromCurveData(c->segments);
    CHECK(b.curve().sortedSegments().size() == c->segments.size());

    // A range that grows keeps the kept segments' values
    std::vector<ValueTake::Event> high{{4 * sec, ossia::value{3.f}}};
    auto g = spliceCurve(a, high, 0, 4 * sec, 5 * sec, opt);
    REQUIRE(g);
    CHECK(g->max == Approx(3.));
    CHECK(g->segments.front().type == power);
    // 0.8 of the old range [0; 1], in [0; 3]
    CHECK(g->segments.front().end.y() == Approx(0.8 / 3.));
  });
}

TEST_CASE("Values recorded into an automation replace its curve where recorded", "[recording][overdub]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    // 10 s, from 0 to 1 linearly
    Automation::ProcessModel a{TimeVal{10 * sec}, Id<Process::ProcessModel>{1}, nullptr};
    REQUIRE(a.curve().sortedSegments().size() == 1);

    std::vector<ValueTake::Event> ev{
        {2 * sec, ossia::value{5.f}}, {3 * sec, ossia::value{3.f}}};
    CurveOptions opt;
    opt.stepped = true;
    auto c = spliceCurve(a, ev, 0, 2 * sec, 4 * sec, opt);
    REQUIRE(c);
    CHECK(c->min == Approx(0.));
    CHECK(c->max == Approx(5.));
    CHECK(c->segments.front().start.x() == Approx(0.));
    CHECK(c->segments.back().end.x() == Approx(1.));
    // Before and after the span: as it was
    CHECK(valueAt(*c, 0.1) == Approx(0.1).margin(0.01));
    CHECK(valueAt(*c, 0.5) == Approx(0.5).margin(0.01));
    CHECK(valueAt(*c, 0.9) == Approx(0.9).margin(0.01));
    // Inside: the values, held
    CHECK(valueAt(*c, 0.25) == Approx(5.));
    CHECK(valueAt(*c, 0.35) == Approx(3.));

    // The second number of a vector
    std::vector<ValueTake::Event> v2{{2 * sec, ossia::value{ossia::vec2f{1.f, 0.7f}}}};
    auto c2 = spliceCurve(a, v2, 1, 2 * sec, 3 * sec, opt);
    REQUIRE(c2);
    CHECK(valueAt(*c2, 0.25) == Approx(0.7).margin(1e-5));

    // Nothing to play: no curve
    CHECK(!spliceCurve(a, {{2 * sec, ossia::value{std::string{"x"}}}}, 0, 2 * sec, 3 * sec, opt));
  });
}
