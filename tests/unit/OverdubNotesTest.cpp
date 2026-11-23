// Notes recorded into a piano roll (Recording::overdubNotes): where the notes
// of a take, dated in the box, land in the piano roll, and which of its notes
// a replacing take removes.
//  - the piano roll's time is the box's moved by its start offset;
//  - round its loop when it loops, a note cut at the loop's end;
//  - replacing removes the notes sounding over the span recorded, all of them
//    when the take covers a whole loop; merging removes none.

#include <Midi/MidiNote.hpp>
#include <Midi/MidiProcess.hpp>
#include <Recording/Takes/OverdubNotes.hpp>

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
