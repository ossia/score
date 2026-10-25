// Every MIDI object preset, taken through what a user does with it in the
// application: the process is added to a document, gets its box (the nodal
// item and the timeline layer) and its inspector, receives the preset through
// the load-preset command a library drop submits, plays on the audio engine
// with notes cabled into it, takes a second preset while playing, stops, and
// is removed. The preset tests apply presets to the bare objects; what they
// cannot see is the UI widgets, the UI to audio value path and the execution
// set-up around them.

#include <Process/Commands/LoadPresetCommandFactory.hpp>
#include <Process/Dataflow/NodeItem.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <Process/Preset.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>

#include <Inspector/InspectorWidgetList.hpp>

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Commands/Interval/RemoveProcessFromInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Dataflow/Commands/EditConnection.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QPointF>
#include <QStandardPaths>
#include <QThread>
#include <QWidget>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <set>
#include <vector>

namespace
{
// Pattern sequencer: plays notes with nothing cabled into it.
const QString pattern_uuid = QStringLiteral("49047204-5c1e-4b54-9f43-2b583f664b2e");

QString fixtures_root()
{
#if defined(SCORE_TEST_PRESETS_ROOT)
  return QStringLiteral(SCORE_TEST_PRESETS_ROOT);
#else
  return {};
#endif
}

//! The user library's presets: the presets a user has saved come through the
//! same path, so they are checked too when this machine has a library.
QString user_root()
{
  if(auto env = qEnvironmentVariable("SCORE_USER_PRESETS_ROOT"); !env.isEmpty())
    return env;
  const auto docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  return docs + "/ossia/score/packages/default/Presets";
}

std::vector<std::string> preset_folders()
{
  std::vector<std::string> res;
  for(const auto& d :
      QDir{fixtures_root()}.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
    res.push_back(d.toStdString());
  return res;
}

//! The folder's presets in the fixtures, then the user library's ones that
//! differ from all of them.
QStringList preset_files(const QString& folder)
{
  QStringList res;
  std::set<QByteArray> seen;
  for(const auto& root : {fixtures_root(), user_root()})
  {
    QDir dir{root + "/" + folder};
    for(const auto& f : dir.entryList({"*.scp"}, QDir::Files, QDir::Name))
    {
      QFile file{dir.filePath(f)};
      if(!file.open(QIODevice::ReadOnly))
        continue;
      const auto hash
          = QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha1);
      if(seen.insert(hash).second)
        res.push_back(dir.filePath(f));
    }
  }
  return res;
}

void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  do
  {
    QApplication::processEvents();
    score::test::flush_deferred_deletes();
    QThread::msleep(2);
  } while(t.elapsed() < ms);
}

Scenario::ScenarioDocumentModel& scenario_doc(score::Document& doc)
{
  return score::IDocument::get<Scenario::ScenarioDocumentModel>(doc);
}

const Process::Inlet* first_midi_inlet(const Process::ProcessModel& proc)
{
  for(auto* in : proc.inlets())
    if(in->type() == Process::PortType::Midi)
      return in;
  return nullptr;
}

std::shared_ptr<Process::Preset>
read_preset(const score::Document& doc, const QString& path)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
    return {};
  return Process::Preset::fromJson(
      doc.context().app.interfaces<Process::ProcessFactoryList>(), f.readAll());
}

void load(score::Document& doc, Process::ProcessModel& proc, const Process::Preset& preset)
{
  auto& factories = doc.context().app.interfaces<Process::LoadPresetCommandFactoryList>();
  auto* cmd = factories.make(
      &Process::LoadPresetCommandFactory::make, proc, preset, doc.context());
  REQUIRE(cmd);
  CommandDispatcher<>{doc.context().commandStack}.submit(cmd);
}

//! What the score view and the inspector panel make for a process.
struct process_views
{
  Process::DataflowManager dfm;
  FocusDispatcher focus;
  Process::Context ctx;
  QGraphicsScene scene;
  QGraphicsRectItem* root{};
  Process::NodeItem* node{};
  Process::LayerView* layer_view{};
  Process::LayerPresenter* layer{};
  std::unique_ptr<QWidget> inspector;

  process_views(score::Document& doc, Process::ProcessModel& proc)
      : ctx{doc.context(), dfm, focus}
  {
    root = new QGraphicsRectItem{QRectF{0., 0., 2000., 2000.}};
    scene.addItem(root);
    node = new Process::NodeItem{proc, ctx, TimeVal::fromMsecs(4000.), root};

    auto& layers = doc.context().app.interfaces<Process::LayerFactoryList>();
    if(auto* factory = layers.findDefaultFactory(proc))
    {
      layer_view = factory->makeLayerView(proc, ctx, root);
      layer = factory->makeLayerPresenter(proc, layer_view, ctx, nullptr);
      if(layer)
      {
        layer->setWidth(800., 800.);
        layer->setHeight(300.);
        layer->on_zoomRatioChanged(1.);
        layer->parentGeometryChanged();
      }
    }

    inspector = std::make_unique<QWidget>();
    auto& inspectors = doc.context().app.interfaces<Inspector::InspectorWidgetList>();
    inspectors.make(doc.context(), {&proc}, inspector.get());
  }

  ~process_views()
  {
    inspector.reset();
    delete layer;
    delete root;
  }
};

void exercise(
    const score::GUIApplicationContext& app, score::Document& doc,
    Process::ProcessModel& source, const QString& path, const QString& next_path)
{
  std::fprintf(stderr, "preset: %s\n", qPrintable(path));
  std::fflush(stderr);

  const auto preset = read_preset(doc, path);
  REQUIRE(preset);
  const auto next = read_preset(doc, next_path);
  REQUIRE(next);

  auto& interval = score::test::base_interval(doc);
  auto* add = new Scenario::Command::AddOnlyProcessToInterval{
      interval, preset->key.key, QString{}, QPointF{}};
  CommandDispatcher<>{doc.context().commandStack}.submit(add);
  auto it = interval.processes.find(add->processId());
  REQUIRE(it != interval.processes.end());
  auto* proc = &*it;
  const auto id = proc->id();

  int cable_id = 1;
  for(auto& c : scenario_doc(doc).cables)
    cable_id = std::max(cable_id, c.id().val() + 1);
  if(auto* in = first_midi_inlet(*proc))
    CommandDispatcher<>{doc.context().commandStack}.submit<Dataflow::CreateCable>(
        scenario_doc(doc), Id<Process::Cable>{cable_id},
        Process::CableType::ImmediateGlutton, *source.outlets()[0], *in);

  auto views = std::make_unique<process_views>(doc, *proc);
  spin(10);

  // As a drop from the library onto the box.
  REQUIRE(proc->presetMatches(*preset));
  load(doc, *proc, *preset);
  spin(10);

  auto& plug = doc.context().plugin<Execution::DocumentPlugin>();
  auto& exec = app.guiApplicationPlugin<Engine::ApplicationPlugin>().execution();
  exec.request_play_global(true);
  spin(120);
  CHECK(plug.isPlaying());

  // Another preset on the playing object: its values go to the audio thread.
  load(doc, *proc, *next);
  spin(80);

  exec.request_stop();
  spin(60);
  CHECK_FALSE(plug.isPlaying());

  views.reset();
  spin(5);
  CommandDispatcher<>{doc.context().commandStack}
      .submit<Scenario::Command::RemoveProcessFromInterval>(
          score::test::base_interval(doc), id);
  spin(10);

  // Undoing the removal reads the process back from its binary serialization,
  // as crash recovery and copy-paste do.
  doc.commandStack().undo();
  spin(10);
  CHECK(score::test::base_interval(doc).processes.find(id)
        != score::test::base_interval(doc).processes.end());
  doc.commandStack().redo();
  spin(10);
}
}

TEST_CASE(
    "Every MIDI object preset survives the application's load, views and playback",
    "[presets][app]")
{
  const auto folder = QString::fromStdString(GENERATE(from_range(preset_folders())));
  const auto files = preset_files(folder);
  if(files.empty())
    SKIP("no preset in " << folder.toStdString());

  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* source = score::test::add_process(*doc, pattern_uuid, {});
    REQUIRE(source);
    REQUIRE(!source->outlets().empty());

    for(int i = 0; i < files.size(); i++)
    {
      INFO(files[i].toStdString());
      exercise(ctx, *doc, *source, files[i], files[(i + 1) % files.size()]);
    }
  });
}
