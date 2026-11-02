// The views of every registered process: its layer, at a usual and at a tiny
// size and in each value of its mode-like controls, its node in the nodal view,
// and its inspector; each built and painted under the crash guard of
// SweepGuard.hpp, so that one broken view is reported by name instead of
// ending the sweep.
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
// What a view looks like is checked in the running application; this checks
// that every view builds and paints at all, in every mode and size.

#include "SweepGuard.hpp"

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
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

#include <Dataflow/Commands/EditConnection.hpp>
#include <Audio/Settings/Model.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Execution/ExecutionController.hpp>
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
#include <QElapsedTimer>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QImage>
#include <QPainter>
#include <QThread>
#include <QWidget>

#include <catch2/catch_test_macros.hpp>

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
  for(const auto& r : results)
  {
    INFO("Process: " << r.name << "  {" << r.uuid << "}");
    if(!r.detail.empty())
      INFO("detail:" << r.detail);
    if(r.skipped)
    {
      ++skipped;
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
  }
  WARN(
      what << ": " << results.size() << " factories, " << swept << " swept, " << layers
           << " layers, " << modes << " modes, " << paints << " paints, " << crashed
           << " crashed, " << skipped << " skipped.");
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
    for(auto& factory : pl)
    {
      auto& r = results.emplace_back(resultFor(factory));
      if(!only.empty() && !only.contains(r.uuid))
      {
        r.skipped = true;
        r.skip_reason = "not selected (SWEEP_ONLY)";
        continue;
      }
      if(sweep::construction_denylist.contains(r.uuid))
      {
        r.skipped = true;
        r.skip_reason = "construction denylist";
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
            *doc, QString::fromStdString(r.uuid), factory.customConstructionData());
        if(!proc)
          return;
        r.constructed = true;
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
    auto& model = score::IDocument::get<Scenario::ScenarioDocumentModel>(*doc);

    auto add = [&](const Process::ProcessModelFactory& f, const QString& data) {
      return score::test::add_process(
          *doc, QString::fromLatin1(score::uuids::toByteArray(f.concreteKey().impl())),
          data);
    };

    // Sources of each kind of data
    Process::ProcessModel* audio{};
    Process::ProcessModel* value{};
    Process::ProcessModel* midi{};
    Process::ProcessModel* texture{};
    Process::ProcessModel* geometry{};
    Process::ProcessModel* buffer{};
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
    std::vector<Process::ProcessModel*> layered;
    if(auto* f = factoryNamed(pl, QStringLiteral("Compute Shader")))
    {
      buffer = add(*f, QStringLiteral(GFX_TEST_CORPUS_DIR "/csf-storage-items.cs"));
      for(const auto& src : layered_sources)
        layered.push_back(add(*f, QStringLiteral(GFX_TEST_CORPUS_DIR "/") + src.file));
    }

    int cable_id = 1;
    auto connect = [&](const Process::Port* source, const Process::Port* sink) {
      if(!source || !sink)
        return;
      CommandDispatcher<>{doc->context().commandStack}.submit<Dataflow::CreateCable>(
          model, Id<Process::Cable>{cable_id++}, Process::CableType::ImmediateGlutton,
          *source, *sink);
    };
    // The LFO's pulses become notes
    if(value && midi && !midi->inlets().empty())
      connect(firstOutlet(*value, Process::PortType::Message), midi->inlets()[0]);

    auto source = [&](Process::PortType t) -> const Process::Outlet* {
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
    };

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
