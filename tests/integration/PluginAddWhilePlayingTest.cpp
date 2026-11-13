// Plug-ins added to a document while it plays: dropped from the library on the
// playing root interval or in its scenario, where a new box starts before,
// at or after the playhead. Each is built into the running graph and runs from
// its first tick on.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/LivePlugin.hpp>

#include <Process/ProcessMimeSerialization.hpp>

#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Audio/AudioApplicationPlugin.hpp>
#include <Audio/Settings/Model.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>
#include <LV2/ApplicationPlugin.hpp>
#include <LV2/Context.hpp>
#include <LV2/EffectModel.hpp>

#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QMimeData>

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace
{
void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  do
  {
    QApplication::processEvents(QEventLoop::AllEvents, 5);
  } while(t.elapsed() < ms);
}

Scenario::IntervalModel& root(score::Document& doc)
{
  return score::IDocument::get<Scenario::ScenarioDocumentModel>(doc).baseInterval();
}

Scenario::ProcessModel& root_scenario(score::Document& doc)
{
  for(auto& p : root(doc).processes)
    if(auto s = qobject_cast<Scenario::ProcessModel*>(&p))
      return *s;
  FAIL("no scenario in the root interval");
  throw;
}

Process::ProcessData process_data(UuidKey<Process::ProcessModel> key, const QString& data)
{
  return Process::ProcessData{key, {}, data};
}

//! As a drop from the library on an interval
std::vector<Process::ProcessModel*> drop_on_interval(
    score::Document& doc, const Scenario::IntervalModel& itv,
    const Process::ProcessData& what)
{
  std::vector<const Process::ProcessModel*> before;
  for(auto& p : itv.processes)
    before.push_back(&p);

  QMimeData mime;
  Mime<Process::ProcessData>::Serializer s{mime};
  s.serialize(what);
  auto& ctx = doc.context();
  REQUIRE(ctx.app.interfaces<Scenario::IntervalDropHandlerList>().drop(
      ctx, itv, QPointF{10., 10.}, mime));

  std::vector<Process::ProcessModel*> added;
  for(auto& p : itv.processes)
    if(std::find(before.begin(), before.end(), &p) == before.end())
      added.push_back(const_cast<Process::ProcessModel*>(&p));
  return added;
}

//! As a drop from the library in the root scenario, at `date`: a new box
Process::ProcessModel*
drop_in_scenario(score::Document& doc, TimeVal date, const Process::ProcessData& what)
{
  auto& scenar = root_scenario(doc);
  Scenario::Command::Macro m{
      new Scenario::Command::AddProcessInNewBoxMacro, doc.context()};
  auto [t, e, st] = m.createDot(scenar, {date, 0.2});
  auto& itv = m.createIntervalAfter(
      scenar, st.id(), {date + TimeVal::fromMsecs(3000.), 0.2});
  auto proc = m.createProcessInNewSlot(itv, what);
  m.commit();
  return proc;
}

struct playing_document
{
  const score::GUIApplicationContext& ctx;
  score::Document* doc{};
  Execution::ExecutionController& exec;

  explicit playing_document(const score::GUIApplicationContext& ctx)
      : ctx{ctx}
      , doc{score::test::new_document(ctx)}
      , exec{ctx.guiApplicationPlugin<Engine::ApplicationPlugin>().execution()}
  {
    REQUIRE(doc);
    REQUIRE(ctx.guiApplicationPlugin<Audio::ApplicationPlugin>().audio);
    exec.request_play_global(true);
    spin(300);
    REQUIRE(plugin().isPlaying());
  }

  ~playing_document()
  {
    exec.request_stop();
    spin(300);
  }

  Execution::DocumentPlugin& plugin() const
  {
    return doc->context().plugin<Execution::DocumentPlugin>();
  }
};

QString lv2_bundle()
{
  return QString::fromUtf8(SCORE_TEST_LV2_BUNDLE);
}

void setup_lv2(
    const score::GUIApplicationContext& ctx,
    const std::vector<std::pair<QString, QString>>& plugins)
{
  auto& lv2 = ctx.applicationPlugin<LV2::ApplicationPlugin>();
  lv2.lv2_context->loadPlugins();
  std::vector<LV2::PluginInfo> infos;
  for(auto& [bundle, uri] : plugins)
  {
    LV2::PluginInfo info;
    info.bundle = bundle;
    info.uri = uri;
    info.name = uri;
    info.valid = true;
    infos.push_back(info);
  }
  lv2.setCachedDescriptors(infos);
}

const auto lv2_key = Metadata<ConcreteKey_k, LV2::Model>::get();
}

// A plug-in that never writes its MIDI output, as RKRP StereoHarmonizer: the
// host finds there the atom:Chunk it gave, whose bytes, in a buffer allocated
// while playing, are the leftovers of other allocations. Read as events, they
// went to a UI that is not open, and one of a huge size aborted score.
TEST_CASE(
    "an LV2 plug-in dropped while playing sends nothing it did not write",
    "[integration][execution][lv2]")
{
  qputenv("LV2_PATH", lv2_bundle().toUtf8());
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    setup_lv2(ctx, {{lv2_bundle(), "urn:score:test:mute"}});
    playing_document play{ctx};

    std::vector<LV2::Model*> added;
    for(auto* p : drop_on_interval(
            *play.doc, root(*play.doc), process_data(lv2_key, "urn:score:test:mute")))
      added.push_back(qobject_cast<LV2::Model*>(p));
    for(double date : {0.1, 0.3, 0.32, 0.5})
      added.push_back(qobject_cast<LV2::Model*>(drop_in_scenario(
          *play.doc, TimeVal::fromMsecs(date * 1000.),
          process_data(lv2_key, "urn:score:test:mute"))));
    spin(700);

    REQUIRE(added.size() == 5);
    for(auto* p : added)
    {
      REQUIRE(p);
      CHECK(p->effectContext.instance);
      CHECK(p->plugin_events->queue.size_approx() == 0);
    }
  });
}

TEST_CASE(
    "an installed LV2 plug-in dropped while playing runs",
    "[integration][execution][lv2][installed]")
{
  const QString uri = qEnvironmentVariable("SCORE_TEST_LV2_URI");
  if(uri.isEmpty())
    SKIP("SCORE_TEST_LV2_URI is not set");
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    setup_lv2(ctx, {{qEnvironmentVariable("SCORE_TEST_LV2_BUNDLE"), uri}});
    playing_document play{ctx};
    const auto t0 = play.exec.execution_time();
    auto added = drop_on_interval(*play.doc, root(*play.doc), process_data(lv2_key, uri));
    REQUIRE(added.size() == 1);
    for(double date : {0.1, 0.32, 0.5})
      drop_in_scenario(
          *play.doc, TimeVal::fromMsecs(date * 1000.), process_data(lv2_key, uri));
    spin(1000);
    CHECK(play.exec.execution_time() > t0);
  });
}

// What a plug-in says on its outputs is queued for its UI: without one open,
// nothing reads that queue, which grew by the events of every tick.
TEST_CASE(
    "an LV2 plug-in with no UI open queues nothing for one",
    "[integration][execution][lv2]")
{
  qputenv("LV2_PATH", lv2_bundle().toUtf8());
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // It forwards the time:Position the host gives it, a few events a tick
    setup_lv2(ctx, {{lv2_bundle(), "urn:score:test:thru"}});
    playing_document play{ctx};
    auto added = drop_on_interval(
        *play.doc, root(*play.doc), process_data(lv2_key, "urn:score:test:thru"));
    REQUIRE(added.size() == 1);
    auto p = qobject_cast<LV2::Model*>(added[0]);
    REQUIRE(p);
    spin(500);
    CHECK(p->plugin_events->queue.size_approx() == 0);
  });
}

namespace
{
//! Every phase of score_test/LivePlugin.hpp: the ones that do not apply to the
//! plug-in are skipped, none fails
void all_phases_hold(
    const score::GUIApplicationContext& ctx, score::test::live::options opt)
{
  std::vector<std::string> failures;
  int ok = 0;
  score::test::live::recorder rec;
  rec.result = [&](std::string_view phase, std::string_view verdict, std::string_view detail) {
    if(verdict == "fail")
      failures.push_back(std::string{phase} + ": " + std::string{detail});
    else if(verdict == "ok")
      ok++;
  };
  score::test::live::run(ctx, opt, rec);
  for(auto& f : failures)
    UNSCOPED_INFO(f);
  CHECK(failures.empty());
  CHECK(ok > 0);
}
}

TEST_CASE(
    "an LV2 plug-in added, edited and removed while playing, in every way",
    "[integration][execution][lv2]")
{
  qputenv("LV2_PATH", lv2_bundle().toUtf8());
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    setup_lv2(ctx, {{lv2_bundle(), "urn:score:test:gain"}});
    score::test::live::options opt;
    opt.what = process_data(lv2_key, "urn:score:test:gain");
    all_phases_hold(ctx, opt);
  });
}

// The same for processes of score's own: what a plug-in host gets wrong when
// added while playing may as well be in the scenario or the engine.
TEST_CASE(
    "score's own processes added, edited and removed while playing, in every way",
    "[integration][execution]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    // LFO, MIDI clip, MIDI utility
    for(const char* uuid :
        {"0b1b1816-c33e-4796-a16d-5aab27fe600f", "c189e507-3afa-4a53-9369-e38860e6bb2d",
         "06b33b83-bb67-4f7a-9980-f5d66e4266c5"})
    {
      INFO(uuid);
      const auto key = UuidKey<Process::ProcessModel>::fromString(QString::fromLatin1(uuid));
      if(!ctx.interfaces<Process::ProcessFactoryList>().get(key))
        SKIP("process " << uuid << " is not built");
      score::test::live::options opt;
      opt.what = process_data(key, {});
      all_phases_hold(ctx, opt);
    }
  });
}
