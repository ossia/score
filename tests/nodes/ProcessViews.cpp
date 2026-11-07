// The views of every registered process: its layer, at a usual and at a tiny
// size and in each value of its mode-like controls, its node in the nodal view,
// and its inspector; each built and painted under the crash guard of
// SweepGuard.hpp, so that one broken view is reported by name instead of
// ending the sweep. The hosts of audio plug-ins are swept once for each
// validation plug-in of their format (ValidationPlugins.hpp).
//
// The monitoring processes are also swept while the document plays, each fed
// by a source of the kind of its first inlet, or by a compute shader's storage
// buffer for those that read buffers, so that their views refresh from a
// running node and paint what goes through it. A geometry inlet further down
// gets the geometry source too: a scene of two nodes moving one mesh, which
// the Geometry viewer summarises for its views. The views of textures are
// also fed a 3D texture, an array and a cube map, each written by a compute
// shader.
//
// Every process is also run, in a test case hidden from the default run, which
// ctest runs in shards (see CMakeLists.txt): added to a document whose sources
// feed each of its inlets that is not a control, the document is played for
// some buffers, ticked from the test thread rather than from an audio thread so
// that the crash guard catches what the nodes do, its texture outlets are
// previewed until they render, and its node is painted while it runs.
//
// What a view looks like is checked in the running application; this checks
// that every view builds and paints at all, in every mode and size.

#include "SweepGuard.hpp"
#include "ValidationPlugins.hpp"

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Project.hpp>

#include <Process/Dataflow/NodeItem.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ProcessList.hpp>

#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionAction.hpp>
#include <Process/ExecutionTransaction.hpp>

#include <Dataflow/Commands/EditConnection.hpp>
#include <Audio/AudioApplicationPlugin.hpp>
#include <Audio/AudioDevice.hpp>
#include <Audio/Settings/Model.hpp>

#include <ossia/audio/audio_engine.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/Clock/DefaultClock.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/TexturePort.hpp>
#include <Gfx/Widgets/TexturePreview.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Library/LibrarySettings.hpp>
#include <Inspector/InspectorWidgetList.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>

#include <Threedim/GeometryViewer/SceneMonitor.hpp>
#include <Ui/Monitor/TextureShapeMonitor.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QImage>
#include <QPainter>
#include <QThread>
#include <QWidget>

#include <ossia/audio/audio_protocol.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph/tick_setup.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/detail/thread.hpp>
#include <ossia/editor/scenario/scenario.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/scenario/time_sync.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace
{
struct ViewResult
{
  std::string uuid;
  std::string name;
  bool skipped = false;
  std::string skip_reason;

  bool constructed = false;
  bool has_layer = false;
  bool layer = false;
  bool node = false;
  bool inspector = false;
  int paints = 0;
  int modes = 0;
  //! While playing: its node made the monitor its views read
  bool monitored = false;
  bool crashed = false;
  std::string detail;
  //! It hosts a validation plug-in, and the ports the plug-in gave it
  bool plugin = false;
  std::size_t ports = 0;
};

void settle(int ms = 0)
{
  QElapsedTimer t;
  t.start();
  do
  {
    QApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    if(ms > 0)
      QThread::msleep(5);
  } while(t.elapsed() < ms);
}

//! What a layer and a node are built in, outside of a document view.
struct Surfaces
{
  Process::DataflowManager dfm;
  FocusDispatcher fd;
  Process::Context pctx;
  QGraphicsScene scene;
  QGraphicsRectItem root{QRectF{0., 0., 2000., 1000.}};

  explicit Surfaces(const score::DocumentContext& doc)
      : pctx{doc, dfm, fd}
  {
    scene.addItem(&root);
  }
  ~Surfaces() { scene.removeItem(&root); }

  //! Paints the item and what it contains into an image of its size.
  void paint(const QGraphicsItem& item)
  {
    const QRectF r = item.sceneBoundingRect();
    QImage img{
        QSize{std::max(1, int(r.width())), std::max(1, int(r.height()))},
        QImage::Format_ARGB32_Premultiplied};
    img.fill(Qt::transparent);
    QPainter p{&img};
    scene.render(&p, QRectF{}, r);
  }
};

//! Every value of a control that picks among a few of them: what the views
//! call their modes, and other switches of what they show.
struct ModeControl
{
  Process::ControlInlet* inlet{};
  std::vector<ossia::value> values;
};

std::vector<ModeControl> modeControls(Process::ProcessModel& proc)
{
  constexpr std::size_t max_values = 16;
  std::vector<ModeControl> res;
  for(auto* inlet : proc.inlets())
  {
    if(auto* c = qobject_cast<Process::ComboBox*>(inlet))
    {
      if(c->alternatives.size() < 2 || c->alternatives.size() > max_values)
        continue;
      ModeControl m{c, {}};
      for(const auto& [name, v] : c->alternatives)
        m.values.push_back(v);
      res.push_back(std::move(m));
    }
    else if(auto* e = qobject_cast<Process::Enum*>(inlet))
    {
      if(e->values.size() < 2 || e->values.size() > max_values)
        continue;
      ModeControl m{e, {}};
      for(const auto& v : e->values)
        m.values.push_back(v.toStdString());
      res.push_back(std::move(m));
    }
  }
  return res;
}

//! The layer of `proc` at two sizes and in every mode, its node, and its
//! inspector. `wait` lets running views refresh before each paint.
void paintViews(
    Process::ProcessModel& proc, const score::DocumentContext& docctx, TimeVal duration,
    ViewResult& r, int wait = 0)
{
  auto& layers = docctx.app.interfaces<Process::LayerFactoryList>();
  if(auto* lf = layers.findDefaultFactory(proc))
  {
    Surfaces s{docctx};
    // Processes without a layer of their own show in their node only
    auto* view = lf->makeLayerView(proc, s.pctx, &s.root);
    r.has_layer = view != nullptr;
    auto* presenter = view ? lf->makeLayerPresenter(proc, view, s.pctx, nullptr) : nullptr;
    if(presenter)
    {
      r.layer = true;
      presenter->on_zoomRatioChanged(1.);
      presenter->parentGeometryChanged();

      const std::pair<qreal, qreal> sizes[]{{640., 320.}, {48., 18.}};
      for(auto [w, h] : sizes)
      {
        presenter->setWidth(w, w);
        presenter->setHeight(h);
        settle(wait);
        s.paint(*view);
        r.paints++;
      }

      presenter->setWidth(640., 640.);
      presenter->setHeight(320.);
      for(auto& m : modeControls(proc))
      {
        const ossia::value initial = m.inlet->value();
        for(const auto& v : m.values)
        {
          m.inlet->setValue(v);
          settle(wait);
          s.paint(*view);
          r.paints++;
          r.modes++;
        }
        m.inlet->setValue(initial);
      }

      presenter->setFullView();
      settle(wait);
      s.paint(*view);
      r.paints++;
    }
    delete presenter;
    settle();
  }

  {
    Surfaces s{docctx};
    auto* node = new Process::NodeItem{proc, s.pctx, duration, &s.root};
    settle(wait);
    s.paint(*node);
    r.paints++;
    r.node = true;
  }
  settle();

  {
    auto& inspectors = docctx.app.interfaces<Inspector::InspectorWidgetList>();
    QWidget host;
    host.resize(400, 800);
    const auto widgets = inspectors.make(docctx, {&proc}, &host);
    for(QWidget* w : widgets)
    {
      w->resize(400, 800);
      w->grab();
    }
    r.inspector = !widgets.empty();
  }
  settle();
}

ViewResult resultFor(const Process::ProcessModelFactory& factory)
{
  ViewResult r;
  r.uuid = score::uuids::toByteArray(factory.concreteKey().impl()).toStdString();
  r.name = factory.prettyName().toStdString();
  return r;
}

void report(const std::vector<ViewResult>& results, const char* what)
{
  REQUIRE(!results.empty());
  int swept = 0, skipped = 0, crashed = 0, layers = 0, modes = 0, paints = 0;
  std::string skips;
  for(const auto& r : results)
  {
    INFO("Process: " << r.name << "  {" << r.uuid << "}");
    if(!r.detail.empty())
      INFO("detail:" << r.detail);
    if(r.skipped)
    {
      ++skipped;
      if(!r.skip_reason.starts_with("not selected"))
        skips += "\n  " + r.name + " {" + r.uuid + "}: " + r.skip_reason;
      continue;
    }
    ++swept;
    if(r.crashed)
    {
      ++crashed;
      WARN("CRASHED " << r.name << " {" << r.uuid << "}:" << r.detail);
    }
    layers += r.layer;
    modes += r.modes;
    if(qEnvironmentVariableIsSet("SWEEP_TRACE"))
      qWarning().noquote() << "SWEEP result" << QString::fromStdString(r.name) << "layer"
                           << r.layer << "modes" << r.modes << "paints" << r.paints;
    paints += r.paints;

    CHECK(r.constructed);
    CHECK_FALSE(r.crashed);
    // A layer view gets its presenter
    if(r.has_layer)
      CHECK(r.layer);
    CHECK(r.node);
    // A host that loaded its plug-in has the ports of the plug-in, and an
    // inspector to edit them in
    if(r.plugin && r.constructed && !r.crashed)
    {
      CHECK(r.ports > 0);
      CHECK(r.inspector);
    }
  }
  WARN(
      what << ": " << results.size() << " factories, " << swept << " swept, " << layers
           << " layers, " << modes << " modes, " << paints << " paints, " << crashed
           << " crashed, " << skipped << " skipped.");
  if(!skips.empty())
    WARN("Skipped:" << skips);
}
}

TEST_CASE("Every process view builds and paints in every mode", "[nodes][views]")
{
  std::vector<ViewResult> results;
  const std::set<std::string> only = sweep::only();

  auto run = [&](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& interval = score::test::base_interval(*doc);
    const auto duration = interval.duration.defaultDuration();
    auto& pl = ctx.interfaces<Process::ProcessFactoryList>();

    const bool trace = qEnvironmentVariableIsSet("SWEEP_TRACE");
    // The hosts of audio plug-ins are made once for each validation plug-in
    const auto plugins = sweep::ValidationPlugins::make(
        ctx, pl, [&](const std::string& uuid) { return only.empty() || only.contains(uuid); });
    for(auto [factory_ptr, plugin] : sweep::jobs(pl, plugins.get()))
    {
      auto& factory = *factory_ptr;
      auto& r = results.emplace_back(resultFor(factory));
      if(plugin)
      {
        r.name += " (" + plugin->label() + ")";
        r.plugin = true;
      }
      if(!only.empty() && !only.contains(r.uuid))
      {
        r.skipped = true;
        r.skip_reason = "not selected (SWEEP_ONLY)";
        continue;
      }
      if(plugin && !plugin->skip.empty())
      {
        r.skipped = true;
        r.skip_reason = plugin->skip;
        continue;
      }
      if(!plugin && sweep::construction_denylist.contains(r.uuid))
      {
        r.skipped = true;
        r.skip_reason = "construction denylist: needs an external plug-in";
        continue;
      }
      if(trace)
        qWarning().noquote() << "SWEEP views" << QString::fromStdString(r.uuid)
                             << QString::fromStdString(r.name);

      // In the interval, as the application adds it, then undone: the views
      // find their ports and the document stays small
      if(!sweep::guarded(
             [&] {
        auto* proc = score::test::add_process(
            *doc, QString::fromStdString(r.uuid),
            plugin ? plugin->data : factory.customConstructionData());
        if(!proc)
          return;
        r.constructed = true;
        r.ports = proc->inlets().size() + proc->outlets().size();
        paintViews(*proc, doc->context(), duration, r);
        doc->commandStack().undo();
        settle();
      }, r.detail))
        r.crashed = true;
    }
  };

  if(!sweep::run_with_teardown_guard([&] { score::test::run_in_gui_app(run); }))
    WARN("Teardown crashed (signal " << (int)sweep::last_signal << ") after the sweep");

  report(results, "Process views");
}

namespace
{
const Process::Outlet* firstOutlet(const Process::ProcessModel& p, Process::PortType t)
{
  for(auto* o : p.outlets())
    if(o->type() == t)
      return o;
  return nullptr;
}

//! A texture port carries textures or GPU buffers, and the model does not say
//! which: the monitoring processes that read buffers.
const std::set<QString> buffer_sinks{QStringLiteral("Buffer view")};

//! The processes whose texture inlet takes the gaussian splats of a Splat
//! loader, and nothing else: a texture or a generic storage buffer there
//! gives them no splat to draw.
const std::set<QString> splat_sinks{QStringLiteral("Splat")};

//! The Splat loader process (Threedim::SplatLoader)
constexpr auto splat_loader_uuid = "bab30770-d6d7-4727-ad43-38eacdd910a7";

//! A 3DGS PLY of eight splats on the corners of a cube around the origin,
//! which the Splat process's default camera, at (-1, -1, -1) looking at the
//! origin, has in view. The properties are those of the reference 3DGS
//! exporter: colour as the degree-0 SH coefficient (0.5 + 0.2821 * f_dc),
//! opacity as a logit, scale as a log, rotation as a (w, x, y, z) quaternion.
QString splat_fixture()
{
  const QString path
      = QDir::temp().filePath(QStringLiteral("process-exec-media/splats.ply"));
  QByteArray ply
      = "ply\n"
        "format ascii 1.0\n"
        "element vertex 8\n"
        "property float x\nproperty float y\nproperty float z\n"
        "property float f_dc_0\nproperty float f_dc_1\nproperty float f_dc_2\n"
        "property float opacity\n"
        "property float scale_0\nproperty float scale_1\nproperty float scale_2\n"
        "property float rot_0\nproperty float rot_1\nproperty float rot_2\n"
        "property float rot_3\n"
        "end_header\n";
  for(int i = 0; i < 8; i++)
  {
    const float x = (i & 1) ? 0.25f : -0.25f;
    const float y = (i & 2) ? 0.25f : -0.25f;
    const float z = (i & 4) ? 0.25f : -0.25f;
    // Pure red, green or blue, saturated: f_dc = (1 - 0.5) / 0.2821
    const float r = (i % 3 == 0) ? 1.77f : -1.77f;
    const float g = (i % 3 == 1) ? 1.77f : -1.77f;
    const float b = (i % 3 == 2) ? 1.77f : -1.77f;
    ply += QStringLiteral("%1 %2 %3 %4 %5 %6 4 -2 -2 -2 1 0 0 0\n")
               .arg(x)
               .arg(y)
               .arg(z)
               .arg(r)
               .arg(g)
               .arg(b)
               .toLatin1();
  }
  score::test::write_file(path, ply);
  return path;
}

//! The views that show textures of every kind, whose node tells them which
//! one it got (Ui::Monitor::TextureShapeMonitor)
const std::set<QString> texture_kind_sinks{
    QStringLiteral("Texture view"), QStringLiteral("Video scope")};

//! The compute shaders writing a texture of each kind but 2D, and the kind,
//! as score::gfx::TextureKind numbers it in the low bits of the packed shape
struct LayeredSource
{
  const char* file;
  const char* kind;
  uint64_t packed_kind;
};
const LayeredSource layered_sources[]{
    {"csf-3d-image-write.cs", "3D", 1},
    {"csf-array-image-write.cs", "array", 2},
    {"csf-cube-image-write.cs", "cube", 3}};

const Process::ProcessModelFactory*
factoryNamed(const Process::ProcessFactoryList& pl, const QString& name)
{
  for(auto& f : pl)
    if(f.prettyName() == name)
      return &f;
  return nullptr;
}

//! Processes in the base interval of a document that produce data of each
//! kind, and the cables that feed it to the processes under test.
struct Sources
{
  score::Document& doc;
  Process::ProcessModel* audio{};
  Process::ProcessModel* value{};
  Process::ProcessModel* midi{};
  Process::ProcessModel* texture{};
  Process::ProcessModel* geometry{};
  //! A compute shader writing a storage buffer
  Process::ProcessModel* buffer{};
  //! A Splat loader with splat_fixture() loaded
  Process::ProcessModel* splats{};
  int cable_id = 1;

  Sources(score::Document& d, const Process::ProcessFactoryList& pl)
      : doc{d}
  {
    auto add = [&](const Process::ProcessModelFactory& f, const QString& data) {
      return score::test::add_process(
          doc, QString::fromLatin1(score::uuids::toByteArray(f.concreteKey().impl())),
          data);
    };
    if(auto* f = factoryNamed(pl, QStringLiteral("Expression Audio Generator")))
      audio = add(*f, f->customConstructionData());
    if(auto* f = factoryNamed(pl, QStringLiteral("LFO")))
      value = add(*f, f->customConstructionData());
    if(auto* f = factoryNamed(pl, QStringLiteral("Pulse to Midi")))
      midi = add(*f, f->customConstructionData());
    if(auto* f = factoryNamed(pl, QStringLiteral("ISF Shader")))
      texture = add(*f, QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-xy-pattern.fs"));
    // A scene rather than a plain geometry: what plain geometry shows is the
    // single mesh of a scene
    if(auto* f = factoryNamed(pl, QStringLiteral("Asset Loader")))
    {
      geometry = add(*f, f->customConstructionData());
      if(geometry && !geometry->inlets().empty())
        if(auto* file = qobject_cast<Process::ControlInlet*>(geometry->inlets()[0]))
          file->setValue(std::string(THREEDIM_TEST_DIR "/shared-mesh-two-nodes.gltf"));
    }
    if(!geometry)
      if(auto* f = factoryNamed(pl, QStringLiteral("Torus")))
        geometry = add(*f, f->customConstructionData());
    if(auto* f = factoryNamed(pl, QStringLiteral("Compute Shader")))
      buffer = add(*f, QStringLiteral(GFX_TEST_CORPUS_DIR "/csf-storage-items.cs"));
    if(auto* f = factoryNamed(pl, QStringLiteral("Splat loader")))
      splats = add(*f, splat_fixture());

    // The LFO's pulses become notes
    if(value && midi && !midi->inlets().empty())
      connect(firstOutlet(*value, Process::PortType::Message), midi->inlets()[0]);
  }

  const Process::Outlet* outlet(Process::PortType t) const
  {
    switch(t)
    {
      case Process::PortType::Audio:
        return audio ? firstOutlet(*audio, t) : nullptr;
      case Process::PortType::Message:
        return value ? firstOutlet(*value, t) : nullptr;
      case Process::PortType::Midi:
        return midi ? firstOutlet(*midi, t) : nullptr;
      case Process::PortType::Texture:
        return texture ? firstOutlet(*texture, t) : nullptr;
      case Process::PortType::Geometry:
        return geometry ? firstOutlet(*geometry, t) : nullptr;
    }
    return nullptr;
  }

  //! What feeds the inlet `in` of a process made by `factory`
  const Process::Outlet* outletFor(const QString& factory, const Process::Inlet& in) const
  {
    const auto t = in.type();
    if(t == Process::PortType::Texture && buffer_sinks.contains(factory))
      return buffer ? firstOutlet(*buffer, t) : nullptr;
    if(t == Process::PortType::Texture && splat_sinks.contains(factory))
      return splats ? firstOutlet(*splats, t) : nullptr;
    return outlet(t);
  }

  void connect(const Process::Port* source, const Process::Port* sink)
  {
    if(!source || !sink)
      return;
    auto& model = score::IDocument::get<Scenario::ScenarioDocumentModel>(doc);
    CommandDispatcher<>{doc.context().commandStack}.submit<Dataflow::CreateCable>(
        model, Id<Process::Cable>{cable_id++}, Process::CableType::ImmediateGlutton,
        *source, *sink);
  }
};
}

TEST_CASE(
    "Monitoring views paint what runs through them", "[nodes][views][monitoring]")
{
  std::vector<ViewResult> results;
  const std::set<std::string> only = sweep::only();

  auto run = [&](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto& interval = score::test::base_interval(*doc);
    const auto duration = interval.duration.defaultDuration();
    auto& pl = ctx.interfaces<Process::ProcessFactoryList>();

    auto add = [&](const Process::ProcessModelFactory& f, const QString& data) {
      return score::test::add_process(
          *doc, QString::fromLatin1(score::uuids::toByteArray(f.concreteKey().impl())),
          data);
    };

    Sources sources{*doc, pl};
    std::vector<Process::ProcessModel*> layered;
    if(auto* f = factoryNamed(pl, QStringLiteral("Compute Shader")))
      for(const auto& src : layered_sources)
        layered.push_back(add(*f, QStringLiteral(GFX_TEST_CORPUS_DIR "/") + src.file));

    auto connect = [&](const Process::Port* source, const Process::Port* sink) {
      sources.connect(source, sink);
    };
    auto source = [&](Process::PortType t) { return sources.outlet(t); };
    auto* buffer = sources.buffer;

    std::vector<std::pair<std::size_t, Process::ProcessModel*>> monitors;
    //! The views fed a texture of several images, and the kind they must see
    struct Kind
    {
      std::size_t result;
      Process::ProcessModel* view;
      uint64_t kind;
    };
    std::vector<Kind> kinds;
    for(auto& factory : pl)
    {
      if(!factory.category().startsWith(QStringLiteral("Monitoring")))
        continue;
      auto& r = results.emplace_back(resultFor(factory));
      if(!only.empty() && !only.contains(r.uuid))
      {
        r.skipped = true;
        r.skip_reason = "not selected (SWEEP_ONLY)";
        continue;
      }
      auto* proc = add(factory, factory.customConstructionData());
      if(!proc)
        continue;
      r.constructed = true;
      if(!proc->inlets().empty())
      {
        const auto t = proc->inlets()[0]->type();
        if(t == Process::PortType::Texture && buffer_sinks.contains(factory.prettyName()))
          connect(buffer ? firstOutlet(*buffer, t) : nullptr, proc->inlets()[0]);
        else
          connect(source(t), proc->inlets()[0]);
        if(t != Process::PortType::Geometry)
          for(auto* inlet : proc->inlets())
            if(inlet->type() == Process::PortType::Geometry)
            {
              connect(source(Process::PortType::Geometry), inlet);
              break;
            }
      }
      monitors.emplace_back(results.size() - 1, proc);

      if(texture_kind_sinks.contains(factory.prettyName()))
        for(std::size_t i = 0; i < layered.size(); i++)
        {
          if(!layered[i])
            continue;
          auto& lr = results.emplace_back(resultFor(factory));
          lr.name += std::string(" (") + layered_sources[i].kind + ")";
          auto* view = add(factory, factory.customConstructionData());
          if(!view)
            continue;
          lr.constructed = true;
          connect(firstOutlet(*layered[i], Process::PortType::Texture), view->inlets()[0]);
          monitors.emplace_back(results.size() - 1, view);
          kinds.push_back({results.size() - 1, view, layered_sources[i].packed_kind});
        }
    }
    // SWEEP_ONLY may leave none of them
    if(monitors.empty() && !only.empty())
      return;
    REQUIRE(!monitors.empty());

    // The views read from the running nodes while the user lets execution
    // update them
    ctx.settings<Scenario::Settings::Model>().setExecutionUpdate(true);
    auto& exec = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>().execution();
    // The audio engine the clock of the execution runs on
    ctx.settings<Audio::Settings::Model>().changed();
    settle(50);
    exec.request_play_global(true);
    settle(1500);

    for(auto [idx, proc] : monitors)
    {
      auto& r = results[idx];
      r.monitored = proc->monitor() != nullptr;
      if(!sweep::guarded([&] { paintViews(*proc, doc->context(), duration, r, 60); }, r.detail))
        r.crashed = true;

      // The viewer drew the scene in full: both nodes, each with the mesh
      if(r.name == "Geometry viewer" && proc->monitor())
      {
        auto& m = static_cast<Gfx::GeometryViewer::SceneMonitor&>(*proc->monitor());
        const auto summary = m.latest();
        CHECK(summary);
        if(summary)
        {
          CHECK(summary->counts.meshes == 2);
          CHECK(summary->counts.primitives == 2);
          CHECK(summary->counts.vertices == 6);
        }
      }
    }

    // The node saw the texture itself, not a 2D image of it
    for(const auto& k : kinds)
    {
      INFO("Process: " << results[k.result].name);
      if(!k.view->monitor())
        continue;
      const uint64_t shape
          = static_cast<Ui::Monitor::TextureShapeMonitor&>(*k.view->monitor()).shape.load();
      // Without compute shaders, as on the OpenGL fallback of a display
      // without Vulkan, the source writes no image: the view gets the 2D
      // target its inlet renders into, or nothing
      if((shape & 0xF) == 0)
        WARN(results[k.result].name << " got no texture of several images");
      else
        CHECK((shape & 0xF) == k.kind);
    }

    exec.request_stop();
    settle(300);
  };

  if(!sweep::run_with_teardown_guard([&] { score::test::run_in_gui_app(run); }))
    WARN("Teardown crashed (signal " << (int)sweep::last_signal << ") after the sweep");

  report(results, "Monitoring views while playing");

  // The document ran: the monitoring processes got their monitors
  // Others take what they show from their inlets
  int monitored = 0;
  std::string names;
  for(const auto& r : results)
    if(r.monitored)
    {
      monitored++;
      names += " " + r.name + ",";
    }
  WARN(monitored << " monitoring processes read from a running node:" << names);
  if(only.empty())
    CHECK(monitored > 0);
}

namespace
{
//! A document's execution, ticked from this thread in the place of the audio
//! thread: a crash or a hang of a node then happens under the crash guard of
//! the process being swept, which reports it by name and goes on with the next.
//! It is what Execution::makeExecutionTick runs, without an audio engine.
struct ManualExecution
{
  Execution::DocumentPlugin& plug;
  std::shared_ptr<Execution::BaseScenarioElement> base;
  smallfun::function<void(unsigned long, double), 128> tick;
  std::vector<Execution::ExecutionAction*> actions;
  std::shared_ptr<ossia::audio_protocol> proto;
  int64_t frames{};

  explicit ManualExecution(score::Document& doc)
      : plug{doc.context().plugin<Execution::DocumentPlugin>()}
  {
    // A new document stops the execution through queued requests: they must
    // not stop this one
    settle();
    plug.reload(false, score::test::base_interval(doc));
    base = plug.baseScenario();
    if(!base)
      return;

    auto& data = *plug.contextData();
    tick = ossia::make_tick(
        ossia::tick_setup_options{}, *data.execState, *data.execGraph,
        *base->baseInterval().OSSIAInterval(), base->baseScenario(), {});
    actions = plug.actions();
    for(Execution::ExecutionAction& act :
        doc.context().app.interfaces<Execution::ExecutionActionList>())
      actions.push_back(&act);
    if(plug.audio_device)
      proto = plug.audioProto();

    Execution::DefaultClock{plug.context()}.play(TimeVal::zero(), *base);
  }

  //! The pending execution commands; what they hold is released on the UI
  //! thread, through the GC queue
  void runCommands()
  {
    auto& data = *plug.contextData();
    Execution::ExecutionCommand c;
    while(data.m_execQueue.try_dequeue(c))
    {
      c();
      data.m_gcQueue.enqueue(Execution::gc(std::move(c)));
    }
  }

  //! One buffer, as the audio thread runs it
  void run(int buffer)
  {
    if(!base)
      return;
    auto& st = *plug.contextData()->execState;
    const ossia::audio_tick_state t{
        nullptr, nullptr, 0, 0, uint64_t(buffer), double(frames) / st.sampleRate};

    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    runCommands();
    if(proto)
      proto->setup_buffers(t);
    for(auto* act : actions)
      act->startTick(t);
    tick(t.frames, t.seconds);
    for(auto* act : actions)
      act->endTick(t);
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
    frames += buffer;
  }

  //! The component of `proc` in the running document
  Execution::ProcessComponent* component(const Process::ProcessModel& proc) const
  {
    if(!base)
      return nullptr;
    auto& procs = base->baseInterval().processes();
    auto it = procs.find(proc.id());
    return it != procs.end() ? it->second.get() : nullptr;
  }

  //! As a stop of the clock does, then the release of the graph
  void stop()
  {
    if(!base)
      return;
    Execution::DefaultClock{plug.context()}.stop(*base);
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    runCommands();
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
    base->cleanup();
    if(auto g = plug.contextData()->execGraph)
    {
      ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
      g->clear();
      ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
    }
    tick = {};
    base.reset();
    plug.finished();
  }
};

//! What running a process did
struct ExecResult
{
  //! Its component exists in the running document
  bool executed = false;
  //! It has a node of the graph, and the node processed frames
  bool has_node = false;
  bool node_ran = false;
  //! Texture outlets, and those a preview got an image of
  int textures = 0;
  int rendered = 0;
  //! It hosts a validation plug-in, and the loudest sample its audio outlets had
  bool plugin = false;
  double peak = 0.;
};

//! Processes that cannot run headless, and why. Kept as short as possible:
//! a process that needs something gets it from the sweep instead.
const std::map<std::string, std::string> execution_denylist = {
};

//! Processes that run with no component of their own, and why. They are still
//! made, added and played; the sweep does not require them to have an
//! executor.
const std::map<std::string, std::string> no_component = {
    {"14bcc6d2-cb34-4bc6-8c70-e512f11d1ceb",
     "the tempo curve of an interval: the interval executes it"},
    {"7c3f6a2e-5b1d-4c8e-9a47-2e6d0f1b8c35",
     "stands in for a process this build cannot load: it does not play"},
};

//! Processes whose node runs but processes no frame in the sweep, and why.
//! Every other node must process frames.
const std::map<std::string, std::string> idle_allowed = {
};

//! Processes whose texture outlets get no image in the sweep, and why. Every
//! other texture outlet must render.
const std::map<std::string, std::string> unrendered_allowed = {
};

//! What the processes that are made from a file or a name get
struct MediaFiles
{
  QString wav;
  QString pd_patch;
  QString splats = splat_fixture();

  explicit MediaFiles(const score::GUIApplicationContext& ctx)
  {
    const QString dir = QDir::temp().filePath(QStringLiteral("process-exec-media"));
    wav = dir + QStringLiteral("/tone.wav");
    score::test::write_wav(wav, 0.5);

    // Audio through, and a bang on every message
    pd_patch = dir + QStringLiteral("/through.pd");
    score::test::write_file(
        pd_patch, "#N canvas 0 0 450 300 12;\n"
                  "#X obj 30 30 inlet~;\n"
                  "#X obj 30 80 outlet~;\n"
                  "#X obj 150 30 inlet;\n"
                  "#X obj 150 80 outlet;\n"
                  "#X connect 0 0 1 0;\n"
                  "#X connect 2 0 3 0;\n");

    // The metronome plays the sounds of the default library, which the
    // private profile of the tests does not have. Any other profile's library
    // is the user's: nothing is added to it.
    if(const auto why = sweep::outside_test_profile(); !why.empty())
    {
      qWarning().noquote() << "the metronome sounds are not added to the library:"
                           << QString::fromStdString(why);
      return;
    }
    const QString util
        = ctx.settings<Library::Settings::Model>().getDefaultLibraryPath()
          + QStringLiteral("/Util/");
    for(auto name : {"metro_tick.wav", "metro_tock.wav"})
    {
      if(QFileInfo::exists(util + name))
        continue;
      // Written beside and renamed, so that a sweep running at the same time
      // never reads half of one
      const QString part = util + name + QStringLiteral(".%1.part")
                                             .arg(QCoreApplication::applicationPid());
      score::test::write_wav(part, 0.05, 0x40);
      if(!QFile::rename(part, util + name))
        QFile::remove(part);
    }
  }

  //! The data `f` is made from
  QString constructionData(const Process::ProcessModelFactory& f) const
  {
    const auto uuid = score::uuids::toByteArray(f.concreteKey().impl());
    if(uuid == score::test::sound_process_uuid.toLatin1())
      return wav;
    if(uuid == "7b3b18ea-311b-40f9-b04e-60ec1fe05786") // PureData
      return pd_patch;
    if(uuid == splat_loader_uuid)
      return splats;
    if(uuid == "415785fe-a7d8-4e62-a85f-9dfccf7dcd96") // Airwindows: the effect
      return QStringLiteral("Galactic");
    return f.customConstructionData();
  }
};
}

TEST_CASE("Every process runs with each inlet fed", "[.][nodes][exec]")
{
  // The sweep sets settings of its own and adds files to the default library
  if(const auto why = sweep::outside_test_profile(); !why.empty())
    SKIP("not run outside the test profile: " << why);

  std::vector<ViewResult> results;
  std::vector<ExecResult> runs;
  const std::set<std::string> only = sweep::only();
  const bool trace = qEnvironmentVariableIsSet("SWEEP_TRACE");

  auto run = [&](const score::GUIApplicationContext& ctx) {
    auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
    ctx.settings<Scenario::Settings::Model>().setExecutionUpdate(true);
    const MediaFiles media{ctx};

    score::Document* doc{};
    std::unique_ptr<Sources> sources;
    auto open = [&] {
      doc = score::test::new_document(ctx);
      REQUIRE(doc != nullptr);
      sources = std::make_unique<Sources>(*doc, pl);
      // This thread is the only one to tick: the audio engine, which a new
      // document starts, would run the execution actions on the pause tick
      // at the same time
      settle();
      if(auto& engine = ctx.guiApplicationPlugin<Audio::ApplicationPlugin>().audio)
        engine->stop();
    };
    open();

    // The hosts of audio plug-ins run once for each validation plug-in
    const auto selected = [&](const std::string& uuid) {
      return (only.empty() || only.contains(uuid)) && sweep::in_shard(uuid);
    };
    const auto plugins = sweep::ValidationPlugins::make(ctx, pl, selected);

    for(auto [factory_ptr, plugin] : sweep::jobs(pl, plugins.get()))
    {
      auto& factory = *factory_ptr;
      auto& r = results.emplace_back(resultFor(factory));
      auto& x = runs.emplace_back();
      if(plugin)
      {
        r.name += " (" + plugin->label() + ")";
        x.plugin = true;
      }
      if(!only.empty() && !only.contains(r.uuid))
      {
        r.skipped = true;
        r.skip_reason = "not selected (SWEEP_ONLY)";
        continue;
      }
      if(!sweep::in_shard(r.uuid))
      {
        r.skipped = true;
        r.skip_reason = "in another shard (SWEEP_SHARD)";
        continue;
      }
      if(plugin && !plugin->skip.empty())
      {
        r.skipped = true;
        r.skip_reason = plugin->skip;
        continue;
      }
      if(!plugin && sweep::construction_denylist.contains(r.uuid))
      {
        r.skipped = true;
        r.skip_reason = "construction denylist: needs an external plug-in";
        continue;
      }
      if(auto it = execution_denylist.find(r.uuid); it != execution_denylist.end())
      {
        r.skipped = true;
        r.skip_reason = it->second;
        continue;
      }
      if(trace)
        qWarning().noquote() << "SWEEP exec" << QString::fromStdString(r.uuid)
                             << QString::fromStdString(r.name);

      auto& stack = doc->commandStack();
      const int mark = stack.currentIndex();
      std::unique_ptr<ManualExecution> exec;
      // Added, fed, played, previewed and painted
      auto runOne = [&] {
        auto* proc = score::test::add_process(
            *doc, QString::fromStdString(r.uuid),
            plugin ? plugin->data : media.constructionData(factory));
        if(!proc)
          return;
        r.constructed = true;

        for(auto* in : proc->inlets())
          if(!qobject_cast<Process::ControlInlet*>(in))
            sources->connect(sources->outletFor(factory.prettyName(), *in), in);

        exec = std::make_unique<ManualExecution>(*doc);
        constexpr int buffer = 512;
        for(int i = 0; i < 32; i++)
        {
          exec->run(buffer);
          if(auto* comp = x.plugin ? exec->component(*proc) : nullptr; comp && comp->node)
            x.peak = std::max(x.peak, sweep::audio_peak(*comp->node));
          settle();
        }

        // What each texture outlet renders, as its preview shows it
        std::vector<std::unique_ptr<Gfx::TexturePreview>> previews;
        auto* gfx = doc->context().findPlugin<Gfx::DocumentPlugin>();
        for(auto* out : proc->outlets())
          if(auto* tex = qobject_cast<Gfx::TextureOutlet*>(out))
          {
            x.textures++;
            if(gfx && tex->graphicsPort().node != score::gfx::invalid_node_index)
            {
              auto& p = previews.emplace_back(std::make_unique<Gfx::TexturePreview>());
              p->attach(&gfx->context, tex->graphicsPort(), QSize{64, 64});
            }
          }
        if(!previews.empty())
        {
          QElapsedTimer t;
          t.start();
          while(t.elapsed() < 2000)
          {
            exec->run(buffer);
            settle();
            QThread::msleep(4);
            x.rendered = 0;
            for(auto& p : previews)
              x.rendered += !p->image().isNull();
            if(x.rendered == int(previews.size()) && t.elapsed() > 100)
              break;
          }
        }

        if(auto* comp = exec->component(*proc))
        {
          x.executed = true;
          if(comp->node)
          {
            x.has_node = true;
            x.node_ran = comp->node->processed_frames() > 0;
          }
        }
        r.monitored = proc->monitor() != nullptr;

        // Its node while it runs
        {
          Surfaces s{doc->context()};
          auto* node = new Process::NodeItem{
              *proc, s.pctx, score::test::base_interval(*doc).duration.defaultDuration(),
              &s.root};
          settle();
          s.paint(*node);
          r.paints++;
          r.node = true;
        }

        previews.clear();
        exec->stop();
        exec.reset();
        settle();
      };
      const bool ok = sweep::guarded(
          [&] {
        try
        {
          runOne();
        }
        catch(const std::exception& e)
        {
          r.detail += std::string(" [EXCEPTION: ") + e.what() + "]";
          r.crashed = true;
        }
        while(stack.currentIndex() > mark)
          stack.undo();
        settle();
      }, r.detail);

      if(!ok)
      {
        r.crashed = true;
        ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
        // What the crash left is not trusted: the next process gets a new
        // document, and this one is left as it is
        (void)exec.release();
        open();
      }
    }
  };

  if(!sweep::run_with_teardown_guard([&] { score::test::run_in_gui_app(run); }))
    WARN("Teardown crashed (signal " << (int)sweep::last_signal << ") after the sweep");

  REQUIRE(!results.empty());
  int swept = 0, executed = 0, nodes = 0, ran = 0, textures = 0, rendered = 0;
  std::string skipped, crashed, idle, unrendered, without, hosted, allowed;
  for(std::size_t i = 0; i < results.size(); i++)
  {
    const auto& r = results[i];
    const auto& x = runs[i];
    INFO("Process: " << r.name << "  {" << r.uuid << "}");
    if(!r.detail.empty())
      INFO("detail:" << r.detail);
    if(r.skipped)
    {
      if(!r.skip_reason.starts_with("not selected") && !r.skip_reason.starts_with("in another"))
        skipped += "\n  " + r.name + " {" + r.uuid + "}: " + r.skip_reason;
      continue;
    }
    ++swept;
    if(r.crashed)
      crashed += "\n  " + r.name + " {" + r.uuid + "}:" + r.detail;
    executed += x.executed;
    nodes += x.has_node;
    ran += x.node_ran;
    textures += x.textures;
    rendered += x.rendered;
    if(x.has_node && !x.node_ran)
    {
      if(auto it = idle_allowed.find(r.uuid); it != idle_allowed.end())
        allowed += "\n  " + r.name + " processed no frame: " + it->second;
      else
      {
        idle += " " + r.name + ",";
        FAIL_CHECK("its node processed no frame");
      }
    }
    if(x.rendered < x.textures)
    {
      if(auto it = unrendered_allowed.find(r.uuid); it != unrendered_allowed.end())
        allowed += "\n  " + r.name + " rendered no image: " + it->second;
      else
      {
        unrendered += " " + r.name + ",";
        FAIL_CHECK(x.rendered << " of its " << x.textures << " texture outlets rendered");
      }
    }
    if(trace)
      qWarning().noquote() << "SWEEP exec result" << QString::fromStdString(r.name)
                           << "executed" << x.executed << "node" << x.has_node << "ran"
                           << x.node_ran << "textures" << x.rendered << "/" << x.textures;

    CHECK(r.constructed);
    CHECK_FALSE(r.crashed);
    // Every process gets a component when its document runs
    if(auto it = no_component.find(r.uuid); it != no_component.end())
    {
      if(!x.executed)
        without += "\n  " + r.name + " {" + r.uuid + "}: " + it->second;
    }
    else if(r.constructed && !r.crashed)
      CHECK(x.executed);
    // A validation plug-in loaded, and made sound of what it was fed
    if(x.plugin)
    {
      hosted += "\n  " + r.name + ": peak " + std::to_string(x.peak);
      CHECK(x.node_ran);
      CHECK(x.peak > 0.);
    }
  }
  WARN(
      "Processes running: " << results.size() << " factories, " << swept << " run, "
                            << executed << " executed, " << ran << " of " << nodes
                            << " nodes processed frames, " << rendered << " of "
                            << textures << " texture outlets rendered.");
  if(!hosted.empty())
    WARN("Audio plug-ins hosted:" << hosted);
  if(!skipped.empty())
    WARN("Skipped:" << skipped);
  if(!without.empty())
    WARN("Run with no component of their own:" << without);
  if(!allowed.empty())
    WARN("Allowed to do nothing:" << allowed);
  // A crash can leave state shared by the whole application broken, a lock
  // held or a queue marked busy, and the processes after it crash in turn: the
  // first one listed is the one to look at.
  if(!crashed.empty())
    WARN("Crashed, in the order they ran:" << crashed);
  if(!idle.empty())
    WARN("Nodes that processed no frame:" << idle);
  if(!unrendered.empty())
    WARN("Texture outlets with no image:" << unrendered);
}
