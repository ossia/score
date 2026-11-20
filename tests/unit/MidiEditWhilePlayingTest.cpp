// Notes edited in the piano roll while the score plays are what the MIDI
// process plays from then on, whatever made the process: notes entered by
// hand, a MIDI file, or a looper's take, and whether it was made before the
// score started or while it plays.

#include <Process/Drop/ClipMetadata.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/ExecutionSetup.hpp>

#include <Scenario/Application/Drops/DropProcessInInterval.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/tick_methods.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/thread.hpp>
#include <ossia/editor/scenario/scenario.hpp>
#include <ossia/editor/scenario/time_interval.hpp>

#include <QApplication>
#include <QMimeData>
#include <QTemporaryDir>
#include <QUrl>

#include <Fx/Looper/MidiTake.hpp>
#include <Midi/Commands/AddNote.hpp>
#include <Midi/Commands/MoveNotes.hpp>
#include <Midi/Commands/RemoveNotes.hpp>
#include <Midi/Commands/ScaleNotes.hpp>
#include <Midi/MidiProcess.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <libremidi/writer.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Process.hpp>
#include <score_test/Project.hpp>

#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using Catch::Approx;

namespace
{
enum class Origin
{
  ByHand,
  ByHandWhilePlaying,
  FileBeforePlaying,
  FileWhilePlaying,
  LooperWrapWhilePlaying,
  LooperTruncateWhilePlaying,
  LooperWrapBeforePlaying,
};

const char* name(Origin o)
{
  switch(o)
  {
    case Origin::ByHand: return "notes entered by hand before playing";
    case Origin::ByHandWhilePlaying: return "notes entered by hand while playing";
    case Origin::FileBeforePlaying: return "a MIDI file dropped before playing";
    case Origin::FileWhilePlaying: return "a MIDI file dropped while playing";
    case Origin::LooperWrapWhilePlaying:
      return "a looper take (wrapped) dropped while playing";
    case Origin::LooperTruncateWhilePlaying:
      return "a looper take (truncated) dropped while playing";
    case Origin::LooperWrapBeforePlaying:
      return "a looper take (wrapped) dropped before playing";
  }
  return "";
}

bool while_playing(Origin o)
{
  return o == Origin::ByHandWhilePlaying || o == Origin::FileWhilePlaying
         || o == Origin::LooperWrapWhilePlaying
         || o == Origin::LooperTruncateWhilePlaying;
}

bool from_looper(Origin o)
{
  return o == Origin::LooperWrapWhilePlaying || o == Origin::LooperTruncateWhilePlaying
         || o == Origin::LooperWrapBeforePlaying;
}

constexpr int quarters = 8;
// At score's 120 bpm, eight quarters are four seconds.
constexpr double clip_seconds = 4.;
constexpr double box_start = 1.;

//! Eight quarters, a note on each, 60 to 67, each an eighth long but the
//! last one, which lasts until the end of the file.
QByteArray ordinary_file()
{
  libremidi::writer w;
  w.ticksPerQuarterNote = 480;
  // A conductor track, then the notes, as most sequencers write them.
  w.add_event(0, 0, libremidi::meta_events::tempo(500000));
  w.add_event(0, 0, libremidi::meta_events::time_signature(4, 4, 24, 8));
  int prev = 0;
  for(int i = 0; i < quarters; i++)
  {
    const int len = i == quarters - 1 ? 480 : 240;
    w.add_event(i * 480 - prev, 1, libremidi::channel_events::note_on(1, 60 + i, 100));
    w.add_event(len, 1, libremidi::channel_events::note_off(1, 60 + i, 0));
    prev = i * 480 + len;
  }
  std::ostringstream out;
  w.write(out);
  const auto s = std::move(out).str();
  return QByteArray(s.data(), qsizetype(s.size()));
}

//! The same quarters as the looper writes them, with a held note over the
//! loop's end and controllers.
std::string looper_file(bool wrap)
{
  Nodes::Looper::midi_export e;
  e.rate = 48000.;
  e.tempo = 120.;
  e.wrap = wrap;
  const double fpq = 48000. * 60. / 120.;
  e.length = int64_t(quarters * fpq);
  int k = 0;
  for(int i = 0; i < quarters; i++)
    e.notes[k++] = {
        .start = int64_t(i * fpq),
        .length = int64_t(fpq / 2),
        .channel = 1,
        .pitch = uint8_t(60 + i),
        .velocity = 100,
        .pass = 0,
        .replaced_by = 0};
  // From the last half quarter, a quarter long: over the loop's end.
  e.notes[k++] = {
      .start = int64_t(7.5 * fpq),
      .length = int64_t(fpq),
      .channel = 1,
      .pitch = 48,
      .velocity = 90,
      .pass = 0,
      .replaced_by = 0};
  e.note_count = k;
  e.controls[0] = {.pos = 0, .status = 0xB0, .data1 = 7, .data2 = 100};
  e.controls[1] = {.pos = int64_t(3 * fpq), .status = 0xB0, .data1 = 1, .data2 = 64};
  e.control_count = 2;
  return e.to_smf();
}

struct heard
{
  double time;
  bool on;
  int pitch;
  int velocity;
};

const Midi::Note* note_of(const Midi::ProcessModel& m, int pitch)
{
  for(auto& n : m.notes)
    if(n.pitch() == pitch)
      return &n;
  return nullptr;
}

std::optional<heard> first(const std::vector<heard>& h, bool on, int pitch)
{
  for(auto& e : h)
    if(e.on == on && e.pitch == pitch)
      return e;
  return std::nullopt;
}
}

TEST_CASE(
    "Notes edited in the piano roll while playing are what the MIDI process plays",
    "[midi][exec][edit][app]")
{
  for(const Origin origin :
      {Origin::ByHand, Origin::ByHandWhilePlaying, Origin::FileBeforePlaying,
       Origin::FileWhilePlaying, Origin::LooperWrapWhilePlaying,
       Origin::LooperTruncateWhilePlaying, Origin::LooperWrapBeforePlaying})
  {
    DYNAMIC_SECTION("for " << name(origin))
    {
      score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
        auto* doc = score::test::new_document(ctx);
        REQUIRE(doc);
        auto& scenario = score::test::base_scenario(*doc);

        QTemporaryDir dir;
        REQUIRE(dir.isValid());

        // An empty box at one second, as long as the clip.
        Scenario::IntervalModel* box{};
        {
          Scenario::Command::Macro m{
              new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
          auto& from = m.createState(scenario, scenario.startEvent().id(), 0.4);
          const auto start
              = m.createIntervalAfter(
                     scenario, from.id(), {TimeVal::fromMsecs(box_start * 1000.), 0.4})
                    .endState();
          box = &m.createIntervalAfter(
              scenario, start,
              {TimeVal::fromMsecs((box_start + clip_seconds) * 1000.), 0.4});
          m.commit();
        }
        REQUIRE(box);

        Midi::ProcessModel* midi{};
        auto make_process = [&] {
          if(origin == Origin::ByHand || origin == Origin::ByHandWhilePlaying)
          {
            Scenario::Command::Macro m{
                new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
            auto* p = m.createProcessInNewSlot(
                *box, Metadata<ConcreteKey_k, Midi::ProcessModel>::get(), QString{});
            REQUIRE(p);
            midi = qobject_cast<Midi::ProcessModel*>(p);
            REQUIRE(midi);
            std::vector<Midi::NoteData> notes;
            for(int i = 0; i < quarters; i++)
              notes.push_back(
                  {double(i) / quarters, 0.5 / quarters, uint8_t(60 + i), 100});
            m.submit(new Midi::AddNotes{*midi, notes});
            m.commit();
            return;
          }

          QString file;
          if(from_looper(origin))
          {
            file = dir.path() + "/MIDI looper-take1.mid";
            const auto bytes = looper_file(origin != Origin::LooperTruncateWhilePlaying);
            std::ofstream f{file.toStdString(), std::ios::binary};
            f.write(bytes.data(), std::streamsize(bytes.size()));
          }
          else
          {
            file = dir.path() + "/song.mid";
            QFile f{file};
            REQUIRE(f.open(QIODevice::WriteOnly));
            f.write(ordinary_file());
          }

          // What a drop of the file on the box carries: a looper's take says
          // it is a loop, as its drag and its "send to timeline" do.
          QMimeData mime;
          mime.setUrls({QUrl::fromLocalFile(file)});
          if(from_looper(origin))
          {
            const auto meta
                = Process::encode_clip_metadata({file.toStdString()}, {}, true);
            REQUIRE(!meta.empty());
            mime.setData(
                QString::fromUtf8(
                    Process::clip_metadata_mime.data(),
                    qsizetype(Process::clip_metadata_mime.size())),
                QByteArray(meta.data(), qsizetype(meta.size())));
          }
          REQUIRE(Scenario::DropProcessInInterval{}.drop(doc->context(), *box, {}, mime));
          for(auto& p : box->processes)
            if(auto m = qobject_cast<Midi::ProcessModel*>(&p))
              midi = m;
          REQUIRE(midi);
        };

        if(!while_playing(origin))
          make_process();

        auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
        plug.reload(true, score::test::base_interval(*doc));
        score::test::run_exec(plug);
        REQUIRE(plug.baseScenario());
        auto& base = *plug.baseScenario();
        auto& ectx = plug.context();
        auto& st = *ectx.execState;
        auto& itv = *base.baseInterval().OSSIAInterval();

        ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
        const ossia::time_value end{int64_t(1e5 * ossia::flicks_per_second<double>)};
        itv.set_min_duration(end);
        itv.set_nominal_duration(end);
        itv.set_max_duration(end);
        itv.start_and_tick();
        ossia::buffer_tick tick{st, *ectx.execGraph, base.baseScenario(), {}};

        std::shared_ptr<ossia::graph_node> node;
        auto find_node = [&] {
          for(auto* cmp : box->components())
            if(auto ic = dynamic_cast<Execution::IntervalComponent*>(cmp))
              if(auto it = ic->processes().find(midi->id()); it != ic->processes().end())
                node = it->second->node;
        };

        std::vector<heard> h;
        const int buffer = st.bufferSize;
        int64_t frames = 0;
        auto run_until = [&](double seconds) {
          while(double(frames) / st.sampleRate < seconds)
          {
            tick(buffer, double(frames) / st.sampleRate);
            if(node)
            {
              auto& out = *node->root_outputs()[0]->target<ossia::midi_port>();
              for(auto& m : out.messages)
              {
                const int status = m.get_status_code() & 0xF0;
                if(status != 0x80 && status != 0x90)
                  continue;
                const int pitch = (m.data[0] >> 8) & 0x7F;
                // MIDI 2 voice messages carry a 16 bit velocity in the second word.
                const bool midi2 = ((m.data[0] >> 28) & 0xF) == 4;
                const int vel = midi2 ? int(m.data[1] >> 25) : int(m.data[0] & 0x7F);
                h.push_back(
                    {double(frames + m.timestamp) / st.sampleRate,
                     status == 0x90 && vel > 0, pitch, vel});
              }
            }
            frames += buffer;
          }
        };

        auto as_ui = [&](auto f) {
          ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
          f();
          QApplication::processEvents();
          score::test::run_exec(plug);
          ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
        };

        if(!while_playing(origin))
          find_node();
        run_until(0.4);
        if(while_playing(origin))
        {
          as_ui(make_process);
          find_node();
        }
        REQUIRE(midi);
        REQUIRE(node);
        REQUIRE(midi->notes.size() >= std::size_t(quarters));
        CHECK(midi->duration().msec() == Approx(clip_seconds * 1000.).margin(20.));

        run_until(box_start + 0.6);
        // The first two quarters are played as they were.
        CHECK(first(h, true, 60));
        CHECK(first(h, true, 61));

        // As the piano roll does it, from the UI thread, while the score plays.
        as_ui([&] {
          CommandDispatcher<> disp{doc->context().commandStack};
          const double q = 1. / quarters;
          // Up an octave
          disp.submit(new Midi::MoveNotes{*midi, {note_of(*midi, 66)->id()}, 12, 0.});
          // Half a quarter later
          disp.submit(new Midi::MoveNotes{*midi, {note_of(*midi, 63)->id()}, 0, q / 2.});
          // Gone
          disp.submit(new Midi::RemoveNotes{*midi, {note_of(*midi, 65)->id()}});
          // A quarter and a half long
          disp.submit(new Midi::ScaleNotes{
              *midi, {note_of(*midi, 64)->id()}, 1.5 * q - note_of(*midi, 64)->duration()});
          // Softer
          disp.submit(
              new Midi::ChangeNotesVelocity{*midi, {note_of(*midi, 67)->id()}, -50.});
          // New, near the end
          disp.submit(new Midi::AddNote{*midi, Midi::NoteData{0.9, q / 2., 90, 100}});
        });

        run_until(box_start + clip_seconds - 0.01);
        ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

        INFO("heard:");
        for(auto& e : h)
          UNSCOPED_INFO(
              (e.on ? "on  " : "off ") << e.pitch << " vel " << e.velocity << " at "
                                       << e.time);

        CHECK(first(h, true, 62));
        CHECK(!first(h, true, 66));
        CHECK(!first(h, true, 65));
        auto moved_up = first(h, true, 78);
        REQUIRE(moved_up);
        CHECK(moved_up->time == Approx(box_start + 3.0).margin(0.02));

        auto later = first(h, true, 63);
        REQUIRE(later);
        CHECK(later->time == Approx(box_start + 1.75).margin(0.02));

        auto longer_on = first(h, true, 64);
        auto longer_off = first(h, false, 64);
        REQUIRE(longer_on);
        REQUIRE(longer_off);
        CHECK(longer_off->time - longer_on->time == Approx(0.75).margin(0.02));

        auto softer = first(h, true, 67);
        REQUIRE(softer);
        CHECK(softer->velocity == 50);

        auto added = first(h, true, 90);
        REQUIRE(added);
        CHECK(added->time == Approx(box_start + 0.9 * clip_seconds).margin(0.02));

        plug.clear();
        QApplication::processEvents();
      });
    }
  }
}
