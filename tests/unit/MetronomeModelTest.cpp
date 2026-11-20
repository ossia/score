// The metronome's model and its execution: the controls, documents saved
// before it had any, the sounds read from files, and the running node
// following the model.

#include "MetronomeOldSaves.hpp"

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Project.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Document/Interval/IntervalExecution.hpp>

#include <Audio/Settings/Model.hpp>
#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Library/LibrarySettings.hpp>
#include <Media/Metro/MetroExecutor.hpp>
#include <Media/Metro/MetroModel.hpp>
#include <Media/Metro/MetroNode.hpp>

#include <score/graphics/RectItem.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/tick_methods.hpp>
#include <ossia/detail/thread.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/detail/flicks.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGraphicsScene>
#include <QTemporaryDir>
#include <QThread>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Media::Metro::Model;
using namespace Media::Metro;

namespace
{
const QString metro_uuid = QStringLiteral("7a20cee3-87da-42a5-9483-975aa33a7a65");

Model* load_json(
    const score::GUIApplicationContext& ctx, score::Document& doc, QObject* owner,
    const char* json)
{
  rapidjson::Document jdoc;
  jdoc.Parse(json);
  REQUIRE(!jdoc.HasParseError());
  auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
  JSONWriter writer{jdoc};
  auto m = dynamic_cast<Model*>(deserialize_interface(pl, writer, doc.context(), owner));
  REQUIRE(m);
  return m;
}

Model* load_binary(
    const score::GUIApplicationContext& ctx, score::Document& doc, QObject* owner,
    const QByteArray& bytes)
{
  auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
  DataStreamWriter writer{bytes};
  auto m = dynamic_cast<Model*>(deserialize_interface(pl, writer, doc.context(), owner));
  REQUIRE(m);
  return m;
}

QByteArray save_json(const Model& m)
{
  const Process::ProcessModel& as_base = m;
  JSONReader reader;
  reader.readFrom(as_base);
  return reader.toByteArray();
}

QByteArray save_binary(const Model& m)
{
  const Process::ProcessModel& as_base = m;
  return score::marshall<DataStream>(as_base);
}

QString text(const ossia::value& v)
{
  return QString::fromStdString(ossia::convert<std::string>(v));
}

//! What a metronome is when nothing was chosen. A new one counts the beats of
//! the signature; one of a document from before the Beat control counts
//! quarter notes, as it always did.
void check_defaults(const Model& m, beat_unit unit)
{
  REQUIRE(m.inlets().size() == 7);
  CHECK(ossia::convert<int>(m.beat->value()) == int(unit));
  CHECK(dynamic_cast<Process::ComboBox*>(m.beat.get()));
  CHECK(m.inlets()[6] == m.beat.get());
  CHECK(!m.beat->description().isEmpty());
  REQUIRE(m.outlets().size() == 2);
  CHECK(ossia::convert<int>(m.sound->value()) == int(Classic));
  CHECK(ossia::convert<float>(m.volume->value()) == 1.f);
  CHECK(ossia::convert<bool>(m.accent->value()) == true);
  CHECK(ossia::convert<int>(m.subdivision->value()) == 1);
  CHECK(text(m.accentFile->value()).isEmpty());
  CHECK(text(m.beatFile->value()).isEmpty());

  CHECK(dynamic_cast<Process::ComboBox*>(m.sound.get()));
  CHECK(dynamic_cast<Process::FloatSlider*>(m.volume.get()));
  CHECK(dynamic_cast<Process::Toggle*>(m.accent.get()));
  CHECK(dynamic_cast<Process::IntSpinBox*>(m.subdivision.get()));
  CHECK(dynamic_cast<Process::FileChooser*>(m.accentFile.get()));
  CHECK(dynamic_cast<Process::FileChooser*>(m.beatFile.get()));

  for(int i = 0; i < 7; i++)
    CHECK(m.inlets()[i]->id() == Id<Process::Port>{i});
  CHECK(m.audio_outlet->id() == Id<Process::Port>{0});
  CHECK(m.bang_outlet->id() == Id<Process::Port>{1});
  CHECK(m.inlets()[0] == m.sound.get());
  CHECK(m.inlets()[1] == m.accentFile.get());
  CHECK(m.inlets()[2] == m.beatFile.get());
  CHECK(m.inlets()[3] == m.volume.get());
  CHECK(m.inlets()[4] == m.accent.get());
  CHECK(m.inlets()[5] == m.subdivision.get());
  CHECK(m.outlets()[0] == m.audio_outlet.get());
  CHECK(m.outlets()[1] == m.bang_outlet.get());
}

template <typename F>
bool wait_until(F&& done, int ms = 10000)
{
  QElapsedTimer t;
  t.start();
  while(!done())
  {
    if(t.elapsed() > ms)
      return false;
    QCoreApplication::processEvents();
    QThread::msleep(2);
  }
  return true;
}

//! Is this the tests' own profile, whose library can be written in.
bool private_profile()
{
  return qEnvironmentVariable("SCORE_CUSTOM_APP_APPLICATION_NAME")
         == QLatin1String("score-test");
}

/**
 * The node of a metronome of a playing document, ticked by hand at 120 bpm in
 * 4/4: a quarter note is half a second.
 */
struct Running
{
  Execution::DocumentPlugin& plug;
  std::shared_ptr<metronome_node> node;
  int rate{};
  int64_t frame{};

  Running(Execution::DocumentPlugin& p, const Process::ProcessModel& proc)
      : plug{p}
  {
    REQUIRE(plug.baseScenario());
    auto& procs = plug.baseScenario()->baseInterval().processes();
    auto it = procs.find(proc.id());
    REQUIRE(it != procs.end());
    node = std::dynamic_pointer_cast<metronome_node>(it->second->node);
    REQUIRE(node);
    rate = plug.context().execState->sampleRate;
    REQUIRE(rate > 0);
  }

  //! Plays `frames` in buffers of 512: the left channel and the pulses.
  struct played
  {
    std::vector<double> left;
    std::vector<int64_t> pulses;
  };
  played play(int64_t frames)
  {
    played res;
    constexpr int buffer = 512;
    constexpr double fps = ossia::flicks_per_second<double>;
    auto& st = *plug.context().execState;
    auto& audio = *node->audio_out.target<ossia::audio_port>();
    auto& bangs = *node->bang_out.target<ossia::value_port>();
    const int64_t first = frame;
    while(frames > 0)
    {
      const int n = int(std::min<int64_t>(frames, buffer));
      score::test::run_exec(plug);
      st.bufferSize = n;

      ossia::token_request tk{};
      tk.prev_date = ossia::time_value{int64_t(double(frame) * fps / rate)};
      tk.date = ossia::time_value{int64_t(double(frame + n) * fps / rate)};
      tk.start_sample = 0;
      tk.length_sample = n;
      tk.speed = 1.;
      tk.tempo = 120.;
      tk.signature = {4, 4};
      tk.musical_start_position = double(frame) * 2. / rate;
      tk.musical_end_position = double(frame + n) * 2. / rate;
      tk.musical_start_last_bar = std::floor(tk.musical_start_position / 4.) * 4.;
      tk.musical_end_last_bar = std::floor(tk.musical_end_position / 4.) * 4.;

      audio.set_channels(0);
      bangs.clear();
      // The node posts to the queue of the UI thread, as the audio thread.
      ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
      static_cast<ossia::graph_node&>(*node).run(tk, ossia::exec_state_facade{&st});
      ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
      // As the graph does once a node has run.
      node->accent_file_in->clear();
      node->beat_file_in->clear();

      for(auto& v : bangs.get_data())
        res.pulses.push_back(frame - first + v.timestamp);
      const auto& out = audio.get();
      REQUIRE(out.size() == 2);
      res.left.insert(res.left.end(), out[0].begin(), out[0].begin() + n);
      frame += n;
      frames -= n;
    }
    return res;
  }

  //! A path arrives on a file inlet, as from a cable or an address, and the
  //! node runs for a buffer.
  void send(ossia::value_inlet& inlet, const QString& path)
  {
    inlet->write_value(path.toStdString(), 0);
    play(512);
  }

  //! What the audio thread posted reaches the UI thread, which starts
  //! decoding the paths it was sent.
  void to_ui()
  {
    plug.contextData()->processEditCommands();
    QCoreApplication::processEvents();
  }

  Execution::MetroComponent& component(const Process::ProcessModel& proc)
  {
    auto& procs = plug.baseScenario()->baseInterval().processes();
    auto* c = dynamic_cast<Execution::MetroComponent*>(procs.find(proc.id())->second.get());
    REQUIRE(c);
    return *c;
  }

  //! One beat from a bar line.
  played beat()
  {
    // Onto the next bar line
    const int64_t bar = 2 * rate;
    frame = ((frame + bar - 1) / bar) * bar;
    node->reset();
    return play(rate / 2);
  }
};

double energy(const std::vector<double>& v)
{
  double e = 0.;
  for(double s : v)
    e += s * s;
  return e;
}
}

TEST_CASE("metronome: a new one has its controls", "[metronome]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QObject* owner = new QObject{&doc->model()};
    Model m{TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, owner};
    check_defaults(m, SignatureBeat);
    CHECK(m.audio_outlet->propagate());

    auto* beat = dynamic_cast<Process::ComboBox*>(m.beat.get());
    REQUIRE(beat);
    REQUIRE(beat->alternatives.size() == 2);
    CHECK(beat->alternatives[0].first == "Time signature");
    CHECK(ossia::convert<int>(beat->alternatives[0].second) == int(SignatureBeat));
    CHECK(beat->alternatives[1].first == "Quarter note");
    CHECK(ossia::convert<int>(beat->alternatives[1].second) == int(QuarterBeat));
    for(auto& [name, value] : beat->alternatives)
      for(QChar c : name)
        CHECK((c.isLetter() || c == ' '));

    auto* sound = dynamic_cast<Process::ComboBox*>(m.sound.get());
    REQUIRE(sound);
    REQUIRE(sound->alternatives.size() == std::size_t(sound_count));
    for(int i = 0; i < sound_count; i++)
      CHECK(ossia::convert<int>(sound->alternatives[i].second) == i);
    CHECK(sound->alternatives[Classic].first == "Classic");
    CHECK(sound->alternatives[Files].first == "Files");

    // The names the user reads have plain punctuation
    for(auto* inl : m.inlets())
      for(QChar c : inl->name())
        CHECK((c.isLetterOrNumber() || c == ' '));
  });
}

TEST_CASE("metronome: a document saved before the controls loads as it played", "[metronome]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QObject* owner = new QObject{&doc->model()};

    SECTION("JSON, of the build before the controls")
    {
      auto* m = load_json(ctx, *doc, owner, metronome_old_saves::old_json);
      check_defaults(*m, QuarterBeat);
      CHECK(m->duration() == TimeVal::fromMsecs(1000));
      CHECK(m->audio_outlet->propagate());
      CHECK(m->metadata().getName() == "Metronome.1");
      delete m;
    }

    SECTION("JSON, of a released version")
    {
      auto* m = load_json(ctx, *doc, owner, metronome_old_saves::released_json);
      check_defaults(*m, QuarterBeat);
      CHECK(m->id() == Id<Process::ProcessModel>{7});
      CHECK(m->duration().impl == 10231200000);
      // What the document had on its ports stays
      CHECK(!m->audio_outlet->propagate());
      CHECK(m->audio_outlet->gain() == 1.);
      CHECK(m->bang_outlet->name() == "Pulse Out");
      delete m;
    }

    SECTION("JSON, with the other controls but no Beat")
    {
      Model saved{TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, owner};
      saved.sound->setValue(int(Woodblock));
      saved.subdivision->setValue(2);
      saved.beat->setValue(int(SignatureBeat));
      rapidjson::Document jdoc;
      jdoc.Parse(save_json(saved).constData());
      REQUIRE(!jdoc.HasParseError());
      REQUIRE(jdoc.HasMember("Beat"));
      jdoc.RemoveMember("Beat");
      REQUIRE(jdoc.HasMember("Subdivision"));

      auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
      JSONWriter writer{jdoc};
      auto m = dynamic_cast<Model*>(
          deserialize_interface(pl, writer, doc->context(), owner));
      REQUIRE(m);
      REQUIRE(m->inlets().size() == 7);
      // Quarter notes, though a new metronome counts the signature's beats
      CHECK(ossia::convert<int>(m->beat->value()) == int(QuarterBeat));
      CHECK(m->beat->id() == Id<Process::Port>{6});
      // What it had is kept, under the ids it had
      CHECK(ossia::convert<int>(m->sound->value()) == int(Woodblock));
      CHECK(ossia::convert<int>(m->subdivision->value()) == 2);
      for(int i = 0; i < 6; i++)
        CHECK(m->inlets()[i]->id() == saved.inlets()[i]->id());
      delete m;
    }

    SECTION("JSON, with a Beat: what was saved")
    {
      Model saved{TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, owner};
      auto* m = load_json(ctx, *doc, owner, save_json(saved).constData());
      CHECK(ossia::convert<int>(m->beat->value()) == int(SignatureBeat));
      delete m;
    }
  });
}

TEST_CASE("metronome: the controls round-trip", "[metronome]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QObject* owner = new QObject{&doc->model()};
    Model m{TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, owner};
    m.sound->setValue(int(Cowbell));
    m.volume->setValue(0.25f);
    m.accent->setValue(false);
    m.subdivision->setValue(3);
    m.beat->setValue(int(QuarterBeat));
    m.accentFile->setValue(std::string{"sounds/hi.wav"});
    m.beatFile->setValue(std::string{"sounds/lo.wav"});

    const auto check = [](const Model& r) {
      REQUIRE(r.inlets().size() == 7);
      CHECK(ossia::convert<int>(r.beat->value()) == int(QuarterBeat));
      CHECK(r.beat->id() == Id<Process::Port>{6});
      CHECK(dynamic_cast<Process::ComboBox*>(r.beat.get()));
      CHECK(ossia::convert<int>(r.sound->value()) == int(Cowbell));
      CHECK(ossia::convert<float>(r.volume->value()) == 0.25f);
      CHECK(ossia::convert<bool>(r.accent->value()) == false);
      CHECK(ossia::convert<int>(r.subdivision->value()) == 3);
      CHECK(text(r.accentFile->value()) == "sounds/hi.wav");
      CHECK(text(r.beatFile->value()) == "sounds/lo.wav");
      CHECK(dynamic_cast<Process::ComboBox*>(r.sound.get()));
      CHECK(dynamic_cast<Process::FileChooser*>(r.beatFile.get()));
    };

    const auto json = save_json(m);
    auto* j = load_json(ctx, *doc, owner, json.constData());
    check(*j);
    delete j;

    // DataStream too: undo/redo and copy/paste
    auto* d = load_binary(ctx, *doc, owner, save_binary(m));
    check(*d);
    delete d;

    // And the other choice, which is not what a missing one reads as
    m.beat->setValue(int(SignatureBeat));
    j = load_json(ctx, *doc, owner, save_json(m).constData());
    CHECK(ossia::convert<int>(j->beat->value()) == int(SignatureBeat));
    delete j;
    d = load_binary(ctx, *doc, owner, save_binary(m));
    CHECK(ossia::convert<int>(d->beat->value()) == int(SignatureBeat));
    delete d;
  });
}

TEST_CASE("metronome: the layer shows the controls", "[metronome]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    // In the document: the port items address their ports by path.
    auto* proc = dynamic_cast<Model*>(score::test::add_process(*doc, metro_uuid, ""));
    REQUIRE(proc);
    Model& m = *proc;

    Process::DataflowManager dfm;
    FocusDispatcher focus;
    Process::Context pctx{doc->context(), dfm, focus};
    QGraphicsScene scene;
    auto& layers = ctx.interfaces<Process::LayerFactoryList>();
    auto* factory = layers.findDefaultFactory(m);
    REQUIRE(factory);
    CHECK(factory->concreteKey() == Metadata<ConcreteKey_k, Model>::get());
    QGraphicsItem* item = factory->makeItem(m, pctx, nullptr);
    REQUIRE(item);
    scene.addItem(item);
    QCoreApplication::processEvents();
    // A widget per control, at least
    int items = 0;
    const auto count = [&](auto&& self, QGraphicsItem* it) -> void {
      for(auto* c : it->childItems())
      {
        items++;
        self(self, c);
      }
    };
    count(count, item);
    CHECK(items >= 6);
    CHECK(item->boundingRect().height() > 50.);
    delete item;
  });
}

TEST_CASE("metronome: sound files are decoded for the engine", "[metronome]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QObject* owner = new QObject{&doc->model()};
    Model m{TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, owner};

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString hi = dir.filePath("hi.wav");
    const QString lo = dir.filePath("lo.wav");
    const QString longer = dir.filePath("long.wav");
    // 0x4040 on 16 bits: about half of full scale
    score::test::write_wav(hi, 0.05, 0x40);
    score::test::write_wav(lo, 0.1, 0x20);
    score::test::write_wav(longer, Model::maxFileSeconds + 1., 0x40);

    const int rate = ctx.settings<Audio::Settings::Model>().getRate();
    REQUIRE(rate > 0);

    int changes = 0;
    QObject::connect(&m, &Model::soundsChanged, owner, [&] { changes++; });

    CHECK(!m.sounds(Files, rate));
    // The synthesized sounds are the node's own
    CHECK(!m.sounds(Click, rate));

    m.accentFile->setValue(hi.toStdString());
    CHECK(changes >= 1);
    REQUIRE(wait_until([&] { return bool(m.sounds(Files, rate)); }));
    {
      auto set = m.sounds(Files, rate);
      CHECK(set->accent.frames() == Catch::Approx(0.05 * rate).margin(rate * 0.002));
      CHECK(set->accent.right.empty());
      CHECK(set->accent.left[set->accent.frames() / 2] == Catch::Approx(0.5).margin(0.02));
      // No click when it ends
      CHECK(set->accent.left.back() == 0.f);
      CHECK(set->beat.empty());
    }

    m.beatFile->setValue(lo.toStdString());
    REQUIRE(wait_until([&] {
      auto s = m.sounds(Files, rate);
      return s && !s->beat.empty();
    }));
    {
      auto set = m.sounds(Files, rate);
      CHECK(set->beat.frames() == Catch::Approx(0.1 * rate).margin(rate * 0.002));
      CHECK(set->beat.left[100] == Catch::Approx(0.25).margin(0.02));
      CHECK(!set->accent.empty());
    }

    // An engine at another rate than the settings': the same duration
    {
      auto set = m.sounds(Files, rate * 2);
      REQUIRE(set);
      CHECK(set->beat.frames() == Catch::Approx(0.1 * rate * 2).margin(rate * 0.004));
      CHECK(set->beat.left[200] == Catch::Approx(0.25).margin(0.02));
    }

    // Not a click: nothing is decoded, and the other file still plays
    m.accentFile->setValue(longer.toStdString());
    QCoreApplication::processEvents();
    {
      auto set = m.sounds(Files, rate);
      REQUIRE(set);
      CHECK(set->accent.empty());
      CHECK(!set->beat.empty());
    }

    // Neither a file that is not there, nor one that is no sound
    m.accentFile->setValue(dir.filePath("nothing.wav").toStdString());
    m.beatFile->setValue(std::string{"/etc/hostname"});
    QCoreApplication::processEvents();
    CHECK(!m.sounds(Files, rate));

    // The files of a loaded document are read as it loads
    m.accentFile->setValue(hi.toStdString());
    const auto json = save_json(m);
    auto* r = load_json(ctx, *doc, owner, json.constData());
    REQUIRE(wait_until([&] { return bool(r->sounds(Files, rate)); }));
    CHECK(r->sounds(Files, rate)->accent.frames() == m.sounds(Files, rate)->accent.frames());
    delete r;
  });
}

TEST_CASE("metronome: the classic ticks are those of the library", "[metronome]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    if(!private_profile())
    {
      WARN("not the tests' own profile: nothing is written in the library");
      return;
    }
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QObject* owner = new QObject{&doc->model()};
    const int rate = ctx.settings<Audio::Settings::Model>().getRate();
    const QString util = ctx.settings<Library::Settings::Model>().getDefaultLibraryPath()
                         + QStringLiteral("/Util/");
    QFile::remove(util + "metro_tick.wav");
    QFile::remove(util + "metro_tock.wav");

    // A library without them: nothing, the execution plays the built-in click
    Model m{TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, owner};
    CHECK(!m.sounds(Classic, rate));

    // Installed afterwards, they are found
    int changes = 0;
    QObject::connect(&m, &Model::soundsChanged, owner, [&] { changes++; });
    score::test::write_wav(util + "metro_tick.wav", 0.05, 0x40);
    score::test::write_wav(util + "metro_tock.wav", 0.08, 0x20);
    REQUIRE(wait_until([&] { return bool(m.sounds(Classic, rate)); }));
    CHECK(changes >= 1);
    auto set = m.sounds(Classic, rate);
    CHECK(set->accent.frames() == Catch::Approx(0.05 * rate).margin(rate * 0.002));
    CHECK(set->beat.frames() == Catch::Approx(0.08 * rate).margin(rate * 0.002));
    CHECK(set->accent.left[100] == Catch::Approx(0.5).margin(0.02));
    CHECK(set->beat.left[100] == Catch::Approx(0.25).margin(0.02));

    QFile::remove(util + "metro_tick.wav");
    QFile::remove(util + "metro_tock.wav");
    CHECK(!m.sounds(Classic, rate));
  });
}

TEST_CASE("metronome: the running node follows the model", "[metronome][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = dynamic_cast<Model*>(score::test::add_process(*doc, metro_uuid, ""));
    REQUIRE(proc);
    proc->sound->setValue(int(Woodblock));
    proc->volume->setValue(0.5f);
    proc->subdivision->setValue(2);

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*doc));
    Running r{plug, *proc};
    const int rate = r.rate;
    const int64_t quarter = rate / 2;

    // What the model had when the execution started
    CHECK(r.node->beat == int(SignatureBeat));
    proc->beat->setValue(int(QuarterBeat));
    score::test::run_exec(plug);
    CHECK(r.node->beat == int(QuarterBeat));
    proc->beat->setValue(int(SignatureBeat));
    score::test::run_exec(plug);
    CHECK(r.node->sound == int(Woodblock));
    CHECK(r.node->volume == 0.5f);
    CHECK(r.node->accent == true);
    CHECK(r.node->subdivision == 2);

    // Two bars: the clicks are where the beats are, an eighth note apart
    {
      auto out = r.play(8 * quarter);
      REQUIRE(out.pulses.size() == 16);
      for(int i = 0; i < 16; i++)
        CHECK(out.pulses[i] == i * quarter / 2);
      CHECK(energy(out.left) > 0.1);
    }

    proc->subdivision->setValue(1);
    proc->volume->setValue(1.f);
    const auto woodblock = r.beat();
    CHECK(r.node->subdivision == 1);
    CHECK(r.node->volume == 1.f);
    CHECK(woodblock.pulses == std::vector<int64_t>{0});
    CHECK(energy(woodblock.left) > 1.);

    // Another sound is another signal
    proc->sound->setValue(int(Beep));
    const auto beep = r.beat();
    CHECK(r.node->sound == int(Beep));
    CHECK(energy(beep.left) > 1.);
    CHECK(beep.left != woodblock.left);

    proc->sound->setValue(int(Cowbell));
    const auto cowbell = r.beat();
    CHECK(cowbell.left != woodblock.left);
    CHECK(cowbell.left != beep.left);

    // Without the accent the bar line plays the beat
    proc->accent->setValue(false);
    const auto plain = r.beat();
    CHECK(r.node->accent == false);
    CHECK(plain.left != cowbell.left);
    proc->accent->setValue(true);

    // Silent
    proc->volume->setValue(0.f);
    r.beat();
    const auto silent = r.beat();
    CHECK(silent.pulses.size() == 1);
    CHECK(energy(silent.left) == 0.);
    proc->volume->setValue(1.f);
    r.beat();

    // Files chosen while it plays reach the node once decoded
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString hi = dir.filePath("hi.wav");
    score::test::write_wav(hi, 0.05, 0x40);
    proc->sound->setValue(int(Files));
    proc->accentFile->setValue(hi.toStdString());
    REQUIRE(wait_until([&] {
      score::test::run_exec(plug);
      return r.node->sounds(Files) != nullptr;
    }));
    CHECK(
        r.node->sounds(Files)->accent.frames()
        == Catch::Approx(0.05 * rate).margin(rate * 0.002));
    const auto file = r.beat();
    CHECK(file.left[rate / 100] == Catch::Approx(0.5).margin(0.02));
    CHECK(file.left[rate / 10] == 0.);

    // No file any more: the node is told, and plays the built-in click
    proc->accentFile->setValue(std::string{});
    score::test::run_exec(plug);
    CHECK(r.node->sounds(Files) == nullptr);
    proc->sound->setValue(int(Click));
    const auto click = r.beat();
    proc->sound->setValue(int(Files));
    const auto fallback = r.beat();
    CHECK(fallback.left == click.left);

    plug.clear();
  });
}

TEST_CASE("metronome: an old document plays the classic ticks", "[metronome][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    const bool library = private_profile();
    const QString util = ctx.settings<Library::Settings::Model>().getDefaultLibraryPath()
                         + QStringLiteral("/Util/");
    if(library)
    {
      score::test::write_wav(util + "metro_tick.wav", 0.05, 0x40);
      score::test::write_wav(util + "metro_tock.wav", 0.08, 0x20);
    }

    // The process of the old document, in the base interval of this one
    auto& interval = score::test::base_interval(*doc);
    Model* proc{};
    {
      rapidjson::Document jdoc;
      jdoc.Parse(metronome_old_saves::old_json);
      REQUIRE(!jdoc.HasParseError());
      // Its id in the old document may be taken in this one
      jdoc["id"].SetInt(getStrongId(interval.processes).val());
      auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
      JSONWriter writer{jdoc};
      proc = dynamic_cast<Model*>(
          deserialize_interface(pl, writer, doc->context(), &interval));
      REQUIRE(proc);
      interval.processes.add(proc);
    }
    check_defaults(*proc, QuarterBeat);
    const int settings_rate = ctx.settings<Audio::Settings::Model>().getRate();
    if(library)
      REQUIRE(wait_until([&] { return bool(proc->sounds(Classic, settings_rate)); }));

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*doc));
    Running r{plug, *proc};
    const int rate = r.rate;

    CHECK(r.node->sound == int(Classic));
    CHECK(r.node->volume == 1.f);
    CHECK(r.node->accent == true);
    CHECK(r.node->subdivision == 1);
    CHECK(r.node->beat == int(QuarterBeat));

    // A bar: a click per quarter note, the first one the accent
    auto out = r.play(4 * (rate / 2));
    REQUIRE(out.pulses.size() == 4);
    for(int i = 0; i < 4; i++)
      CHECK(out.pulses[i] == i * (rate / 2));

    if(library)
    {
      // The tick on the bar line, the tock on the beats, at their own levels
      REQUIRE(r.node->sounds(Classic));
      CHECK(out.left[rate / 100] == Catch::Approx(0.5).margin(0.02));
      CHECK(out.left[rate / 2 + rate / 100] == Catch::Approx(0.25).margin(0.02));
      CHECK(out.left[rate / 4] == 0.);
    }
    else
    {
      CHECK(energy(out.left) > 1.);
    }

    plug.clear();
    if(library)
    {
      QFile::remove(util + "metro_tick.wav");
      QFile::remove(util + "metro_tock.wav");
    }
  });
}

TEST_CASE("metronome: in the engine, a click on every beat of the score", "[metronome][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = dynamic_cast<Model*>(score::test::add_process(*doc, metro_uuid, ""));
    REQUIRE(proc);
    proc->sound->setValue(int(Beep));

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*doc));
    score::test::run_exec(plug);
    Running r{plug, *proc};
    auto& base = *plug.baseScenario();
    auto& ectx = plug.context();
    auto& st = *ectx.execState;
    auto& itv = *base.baseInterval().OSSIAInterval();

    // The whole graph, ticked as the audio engine does: the tokens, their
    // musical positions and their bars are the engine's own.
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    const ossia::time_value end{int64_t(1e5 * ossia::flicks_per_second<double>)};
    itv.set_min_duration(end);
    itv.set_nominal_duration(end);
    itv.set_max_duration(end);
    itv.start_and_tick();
    ossia::buffer_tick tick{st, *ectx.execGraph, base.baseScenario(), {}};

    const int buffer = st.bufferSize;
    const int rate = st.sampleRate;
    REQUIRE(buffer > 0);
    auto& audio = *r.node->audio_out.target<ossia::audio_port>();
    auto& bangs = *r.node->bang_out.target<ossia::value_port>();

    std::vector<int64_t> pulses;
    std::vector<double> left;
    int64_t frames = 0;
    // Three bars at the 120 bpm of a new document
    while(frames < 6 * rate)
    {
      tick(buffer, double(frames) / rate);
      for(auto& v : bangs.get_data())
        pulses.push_back(frames + v.timestamp);
      const auto& out = audio.get();
      if(out.size() == 2 && out[0].size() >= std::size_t(buffer))
        left.insert(left.end(), out[0].begin(), out[0].begin() + buffer);
      else
        left.insert(left.end(), buffer, 0.);
      frames += buffer;
    }
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

    // The last buffer goes past the three bars
    std::erase_if(pulses, [&](int64_t p) { return p >= 6 * rate; });

    // A quarter note is half a second: on the very sample, for every beat
    REQUIRE(pulses.size() == 12);
    for(int i = 0; i < 12; i++)
      CHECK(pulses[i] == int64_t(i) * (rate / 2));

    // The beep starts on the sample of the click and is over before the next:
    // 70 ms of sound, then nothing.
    const auto accent = make_builtin_sound(Beep, true, rate);
    const auto beat = make_builtin_sound(Beep, false, rate);
    for(int i = 0; i < 12; i++)
    {
      const auto& snd = i % 4 == 0 ? accent : beat;
      const int64_t at = pulses[i];
      REQUIRE(at + rate / 2 <= int64_t(left.size()));
      for(int64_t k = 0; k < rate / 2; k++)
      {
        const double expected = k < snd.frames() ? double(snd.left[k]) : 0.;
        REQUIRE(left[at + k] == Catch::Approx(expected).margin(1e-6));
      }
    }

    plug.clear();
  });
}

TEST_CASE("metronome: a path sent to a file inlet while running is played", "[metronome][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = dynamic_cast<Model*>(score::test::add_process(*doc, metro_uuid, ""));
    REQUIRE(proc);
    proc->sound->setValue(int(Files));

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString hi = dir.filePath("hi.wav");
    const QString lo = dir.filePath("lo.wav");
    const QString longer = dir.filePath("long.wav");
    score::test::write_wav(hi, 0.05, 0x40);
    score::test::write_wav(lo, 0.1, 0x20);
    score::test::write_wav(longer, Model::maxFileSeconds + 1., 0x40);

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*doc));
    Running r{plug, *proc};
    auto& comp = r.component(*proc);
    const int rate = r.rate;
    const auto frames_of = [&](double seconds) {
      return Catch::Approx(seconds * rate).margin(rate * 0.002);
    };
    const auto decoded = [&](double seconds) {
      return wait_until([&] {
        score::test::run_exec(plug);
        auto* s = r.node->sounds(Files);
        return s && s->accent.frames() == frames_of(seconds);
      });
    };

    // No file anywhere: the built-in click
    proc->sound->setValue(int(Click));
    const auto click = r.beat();
    proc->sound->setValue(int(Files));
    CHECK(r.beat().left == click.left);
    const auto commands = doc->commandStack().size();

    // The path comes in on the inlet: the click is still the built-in one
    // while the file is decoded, then it is the file.
    r.send(r.node->accent_file_in, hi);
    CHECK(r.node->sounds(Files) == nullptr);
    CHECK(comp.executionDecodes() == 0);
    r.to_ui();
    CHECK(comp.executionDecodes() == 1);
    REQUIRE(decoded(0.05));
    {
      const auto out = r.beat();
      CHECK(out.left != click.left);
      CHECK(out.left[rate / 100] == Catch::Approx(0.5).margin(0.02));
      CHECK(out.left[rate / 10] == 0.);
    }

    // The document is not edited by what its ports receive
    CHECK(text(proc->accentFile->value()).isEmpty());
    CHECK(doc->commandStack().size() == commands);
    CHECK(!proc->sounds(Files, rate));

    // The same path again, on every buffer: nothing is asked for
    for(int i = 0; i < 20; i++)
      r.send(r.node->accent_file_in, hi);
    r.to_ui();
    CHECK(comp.executionDecodes() == 1);

    // The other inlet, the other sound
    r.send(r.node->beat_file_in, lo);
    r.to_ui();
    CHECK(comp.executionDecodes() == 2);
    REQUIRE(wait_until([&] {
      score::test::run_exec(plug);
      auto* s = r.node->sounds(Files);
      return s && !s->beat.empty();
    }));
    CHECK(r.node->sounds(Files)->beat.frames() == frames_of(0.1));
    CHECK(r.node->sounds(Files)->accent.frames() == frames_of(0.05));

    // Not a click: refused, and that inlet has no sound. The beat file then
    // plays on the bar line too.
    r.send(r.node->accent_file_in, longer);
    r.to_ui();
    CHECK(comp.executionDecodes() == 2);
    score::test::run_exec(plug);
    REQUIRE(r.node->sounds(Files));
    CHECK(r.node->sounds(Files)->accent.empty());
    CHECK(r.beat().left[rate / 100] == Catch::Approx(0.25).margin(0.02));

    // Nothing left to play: the built-in click again
    r.send(r.node->beat_file_in, dir.filePath("nothing.wav"));
    r.to_ui();
    score::test::run_exec(plug);
    CHECK(r.node->sounds(Files) == nullptr);
    CHECK(r.beat().left == click.left);

    // Something that is not a path is not one
    r.node->accent_file_in->write_value(3.5f, 0);
    r.play(512);
    r.to_ui();
    CHECK(comp.executionDecodes() == 2);

    // The control edited afterwards wins over what came in
    r.send(r.node->accent_file_in, hi);
    r.to_ui();
    REQUIRE(decoded(0.05));
    proc->accentFile->setValue(lo.toStdString());
    REQUIRE(decoded(0.1));
    // and the path it replaced is a new one when it comes in again
    r.send(r.node->accent_file_in, hi);
    r.to_ui();
    REQUIRE(decoded(0.05));

    plug.clear();
  });
}

TEST_CASE("metronome: of a burst of paths only the last is decoded", "[metronome][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = dynamic_cast<Model*>(score::test::add_process(*doc, metro_uuid, ""));
    REQUIRE(proc);
    proc->sound->setValue(int(Files));

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    std::vector<QString> files;
    for(int i = 0; i < 8; i++)
    {
      files.push_back(dir.filePath(QString("click%1.wav").arg(i)));
      score::test::write_wav(files.back(), 0.02 * (i + 1), 0x40);
    }

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*doc));
    Running r{plug, *proc};
    auto& comp = r.component(*proc);
    const int rate = r.rate;
    const auto accent_is = [&](double seconds) {
      return wait_until([&] {
        score::test::run_exec(plug);
        auto* s = r.node->sounds(Files);
        return s
               && s->accent.frames()
                      == Catch::Approx(seconds * rate).margin(rate * 0.002);
      });
    };

    // A path per buffer, before the UI thread looks at any
    for(const auto& f : files)
      r.send(r.node->accent_file_in, f);
    r.to_ui();
    CHECK(comp.executionDecodes() == 1);
    REQUIRE(accent_is(0.16));

    // Several in one buffer: the last value of the port
    for(int i = 0; i < 4; i++)
      r.node->accent_file_in->write_value(files[i].toStdString(), i);
    r.play(512);
    r.to_ui();
    CHECK(comp.executionDecodes() == 2);
    REQUIRE(accent_is(0.08));

    // On both inlets at once: one decoding each
    for(int i = 0; i < 6; i++)
    {
      r.node->accent_file_in->write_value(files[i].toStdString(), 0);
      r.node->beat_file_in->write_value(files[7 - i].toStdString(), 0);
      r.play(512);
    }
    r.to_ui();
    CHECK(comp.executionDecodes() == 4);
    REQUIRE(accent_is(0.12));
    REQUIRE(wait_until([&] {
      score::test::run_exec(plug);
      auto* s = r.node->sounds(Files);
      return s && s->beat.frames() == Catch::Approx(0.06 * rate).margin(rate * 0.002);
    }));

    // A new path while the previous one is still decoding: the one decoding
    // is dropped, and what ends up playing is the last one asked for.
    r.send(r.node->accent_file_in, files[7]);
    r.to_ui();
    r.send(r.node->accent_file_in, files[0]);
    r.to_ui();
    CHECK(comp.executionDecodes() == 6);
    REQUIRE(accent_is(0.02));
    // Nothing of the dropped one arrives later
    for(int i = 0; i < 50; i++)
    {
      QCoreApplication::processEvents();
      QThread::msleep(2);
    }
    score::test::run_exec(plug);
    CHECK(
        r.node->sounds(Files)->accent.frames()
        == Catch::Approx(0.02 * rate).margin(rate * 0.002));

    plug.clear();
  });
}
