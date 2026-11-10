#pragma once

// A process added to a document while it plays, in every way the user can add
// one: for the plug-in sweeps and the integration tests of the plug-in hosts.
//
// run() goes through the phases, each one a way to add, edit or remove the
// process while a document plays, on a new document it plays first.
// Before each, rec.phase(name) is called, so that a harness that sees its
// worker crash or hang can tell where; after each, rec.result(name, verdict,
// detail), where verdict is "ok", "skipped" (the plug-in has nothing for it)
// or "fail".
//
// The target linking this needs score_plugin_scenario, score_plugin_engine,
// score_plugin_audio, score_plugin_dataflow, score_plugin_deviceexplorer and
// score_lib_process.

#include <Process/Commands/LoadPresetCommandFactory.hpp>
#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/Preset.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <Effect/EffectLayer.hpp>

#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Interval/InsertContentInInterval.hpp>
#include <Scenario/Commands/Interval/RemoveProcessFromInterval.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessCreation.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioEditor.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Audio/AudioApplicationPlugin.hpp>
#include <Audio/Settings/Model.hpp>
#include <Dataflow/Commands/EditConnection.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/ComponentUtils.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/MimeVisitor.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QMimeData>
#include <QUrl>

#include <score_test/Document.hpp>

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace score::test::live
{

struct recorder
{
  std::function<void(std::string_view phase)> phase;
  std::function<void(std::string_view phase, std::string_view verdict, std::string_view detail)>
      result;
};

struct options
{
  //! What the library drags for the plug-in
  Process::ProcessData what;
  //! Files that each make the plug-in when dropped, for a drop of several at
  //! once; several drops in a row when empty
  std::vector<QUrl> files;
  //! Opens the editor of a process; the phase is skipped when unset
  std::function<void(Process::ProcessModel&)> open_editor;
  std::function<void(Process::ProcessModel&)> close_editor;
  //! How long each phase lets the document play, in milliseconds
  int play_ms = 200;
  //! The phases to run: all of them when empty
  std::vector<std::string> only;
};

//! A phase that does not hold: its message is the detail of the verdict
struct failure : std::runtime_error
{
  using std::runtime_error::runtime_error;
};
//! A phase the plug-in has nothing for
struct skip : std::runtime_error
{
  using std::runtime_error::runtime_error;
};

inline void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  do
  {
    QApplication::processEvents(QEventLoop::AllEvents, 5);
  } while(t.elapsed() < ms);
}

inline void expect(bool cond, const std::string& what)
{
  if(!cond)
    throw failure{what};
}

class session
{
public:
  session(const score::GUIApplicationContext& ctx, const options& opt)
      : ctx{ctx}
      , opt{opt}
      , exec{ctx.guiApplicationPlugin<Engine::ApplicationPlugin>().execution()}
  {
    doc = score::test::new_document(ctx);
    expect(doc, "no document");
    // With a main window, the tab of the new document becomes the active one
    // a few events later, which stops what plays
    spin(ctx.mainWindow ? 150 : 0);
    expect(bool(ctx.guiApplicationPlugin<Audio::ApplicationPlugin>().audio), "no audio engine");
    play();
  }

  ~session()
  {
    exec.request_stop();
    spin(100);
    if(doc)
      ctx.docManager.forceCloseDocument(ctx, *doc);
    spin(50);
  }

  session(const session&) = delete;
  session& operator=(const session&) = delete;

  const score::GUIApplicationContext& ctx;
  const options& opt;
  Execution::ExecutionController& exec;
  score::Document* doc{};

  Scenario::ScenarioDocumentModel& model() const
  {
    return score::IDocument::get<Scenario::ScenarioDocumentModel>(*doc);
  }
  Scenario::IntervalModel& root() const { return model().baseInterval(); }
  Scenario::ProcessModel& scenario() const
  {
    for(auto& p : root().processes)
      if(auto s = qobject_cast<Scenario::ProcessModel*>(&p))
        return *s;
    throw failure{"no scenario in the root interval"};
  }
  Execution::DocumentPlugin& plugin() const
  {
    return doc->context().plugin<Execution::DocumentPlugin>();
  }
  score::CommandStack& stack() const { return doc->commandStack(); }

  void play()
  {
    exec.request_play_global(true);
    spin(150);
    expect(plugin().isPlaying(), "the document does not play");
  }

  //! The document plays on: its time moves
  void plays_on(int ms)
  {
    const auto t0 = exec.execution_time();
    spin(ms);
    expect(plugin().isPlaying(), "the document stopped playing");
    expect(exec.execution_time() > t0, "the document's time stands still");
  }

  std::vector<Process::ProcessModel*> processes(const Scenario::IntervalModel& itv) const
  {
    std::vector<Process::ProcessModel*> res;
    for(auto& p : itv.processes)
      res.push_back(const_cast<Process::ProcessModel*>(&p));
    return res;
  }

  //! The processes of `itv` that are not in `before`
  std::vector<Process::ProcessModel*> added(
      const Scenario::IntervalModel& itv,
      const std::vector<Process::ProcessModel*>& before) const
  {
    std::vector<Process::ProcessModel*> res;
    for(auto* p : processes(itv))
      if(std::find(before.begin(), before.end(), p) == before.end()
         && !qobject_cast<Scenario::ProcessModel*>(p))
        res.push_back(p);
    return res;
  }

  bool drop(const Scenario::IntervalModel& itv, const QMimeData& mime)
  {
    return ctx.interfaces<Scenario::IntervalDropHandlerList>().drop(
        doc->context(), itv, QPointF{20., 20.}, mime);
  }

  //! As a drop from the library on `itv`
  Process::ProcessModel& drop_process(const Scenario::IntervalModel& itv)
  {
    const auto before = processes(itv);
    QMimeData mime;
    Mime<Process::ProcessData>::Serializer s{mime};
    s.serialize(opt.what);
    expect(drop(itv, mime), "the drop was not handled");
    auto res = added(itv, before);
    expect(res.size() == 1, "the drop made " + std::to_string(res.size()) + " processes");
    return *res.front();
  }

  //! As a drop from the library in the root scenario, at `date`: a new box
  Process::ProcessModel& drop_in_box(TimeVal date, TimeVal duration)
  {
    auto& scenar = scenario();
    Scenario::Command::Macro m{
        new Scenario::Command::AddProcessInNewBoxMacro, doc->context()};
    auto [t, e, st] = m.createDot(scenar, {date, 0.4});
    auto& itv = m.createIntervalAfter(scenar, st.id(), {date + duration, 0.4});
    auto proc = m.createProcessInNewSlot(itv, opt.what);
    m.showRack(itv);
    m.commit();
    expect(proc, "the box has no process");
    return *proc;
  }

  Scenario::IntervalModel& interval_of(const Process::ProcessModel& proc) const
  {
    auto itv = qobject_cast<Scenario::IntervalModel*>(proc.parent());
    expect(itv, "the process is not in an interval");
    return *itv;
  }

  //! The execution component of `proc`, when its interval has one
  Execution::ProcessComponent* component(const Process::ProcessModel& proc) const
  {
    auto& itv = interval_of(proc);
    // The root interval's component is the base scenario's
    Execution::IntervalComponentBase* comp{};
    if(&itv == &root())
    {
      if(auto& base = plugin().baseScenario())
        comp = &base->baseInterval();
    }
    else
      comp = score::findComponent<Execution::IntervalComponentBase>(itv.components());
    if(!comp)
      return nullptr;
    auto it = comp->processes().find(proc.id());
    return it != comp->processes().end() ? it->second.get() : nullptr;
  }

  //! `proc` is in the running graph: it has an execution component with a node
  void runs(const Process::ProcessModel& proc) const
  {
    auto comp = component(proc);
    expect(comp, "the process has no execution component");
    expect(bool(comp->node), "the execution component has no node");
  }

  void no_component(const Process::ProcessModel& proc) const
  {
    expect(!component(proc), "the removed process still has an execution component");
  }

  void remove(const Process::ProcessModel& proc)
  {
    CommandDispatcher<>{doc->context().commandStack}.submit(
        new Scenario::Command::RemoveProcessFromInterval{interval_of(proc), proc.id()});
  }

  Process::ProcessModel* by_id(
      const Scenario::IntervalModel& itv, const Id<Process::ProcessModel>& id) const
  {
    auto it = itv.processes.find(id);
    return it != itv.processes.end() ? &*it : nullptr;
  }

  void connect(const Process::Port& source, const Process::Port& sink)
  {
    auto& m = model();
    CommandDispatcher<>{doc->context().commandStack}.submit<Dataflow::CreateCable>(
        m, getStrongId(m.cables), Process::CableType::ImmediateGlutton, source, sink);
  }

  //! The first outlet of `out` and inlet of `in` of the same type, as a drop
  //! on a cable needs
  std::pair<Process::Outlet*, Process::Inlet*>
  cable_ends(const Process::ProcessModel& out, const Process::ProcessModel& in) const
  {
    for(auto* o : out.outlets())
    {
      if(qobject_cast<Process::ControlOutlet*>(o))
        continue;
      for(auto* i : in.inlets())
        if(i->type() == o->type() && !qobject_cast<Process::ControlInlet*>(i))
          return {o, i};
    }
    return {};
  }
};

using phase_fn = std::function<void(session&)>;

//! The phases, in the order they run, on one document playing on
inline std::vector<std::pair<std::string, phase_fn>> phases(const options& opt)
{
  std::vector<std::pair<std::string, phase_fn>> p;
  const int ms = opt.play_ms;

  p.emplace_back("live-drop-interval", [ms](session& s) {
    auto& proc = s.drop_process(s.root());
    s.plays_on(ms);
    s.runs(proc);
  });

  p.emplace_back("live-drop-stopped-interval", [ms](session& s) {
    // A box that starts long after the playhead: not playing yet
    auto& box = s.drop_in_box(s.exec.execution_time() + TimeVal::fromMsecs(60000.), TimeVal::fromMsecs(5000.));
    auto& itv = s.interval_of(box);
    auto& proc = s.drop_process(itv);
    s.plays_on(ms);
    s.runs(box);
    s.runs(proc);
  });

  p.emplace_back("live-drop-nodal", [ms](session& s) {
    auto& box = s.drop_in_box(s.exec.execution_time(), TimeVal::fromMsecs(20000.));
    auto& itv = s.interval_of(box);
    itv.setViewMode(Scenario::IntervalModel::Nodal);
    auto& proc = s.drop_process(itv);
    s.plays_on(ms);
    s.runs(proc);
  });

  // Mid-buffer: a box does not start on a buffer boundary
  p.emplace_back("live-drop-box-past", [ms](session& s) {
    auto now = s.exec.execution_time();
    auto& proc = s.drop_in_box(now - TimeVal::fromMsecs(37.), TimeVal::fromMsecs(10000.));
    s.plays_on(ms);
    (void)proc;
  });
  p.emplace_back("live-drop-box-playhead", [ms](session& s) {
    auto& proc = s.drop_in_box(s.exec.execution_time() + TimeVal::fromMsecs(3.), TimeVal::fromMsecs(10000.));
    s.plays_on(ms);
    s.runs(proc);
  });
  p.emplace_back("live-drop-box-future", [ms](session& s) {
    auto& proc = s.drop_in_box(s.exec.execution_time() + TimeVal::fromMsecs(53.), TimeVal::fromMsecs(10000.));
    s.plays_on(ms + 60);
    s.runs(proc);
  });

  p.emplace_back("live-start-offset", [ms](session& s) {
    auto& proc = s.drop_process(s.root());
    CommandDispatcher<>{s.doc->context().commandStack}.submit<Process::SetStartOffset>(
        proc, TimeVal::fromMsecs(7.3));
    s.plays_on(ms);
    s.runs(proc);
  });

  // Loop boundaries that split buffers in two
  p.emplace_back("live-loop", [ms](session& s) {
    auto& proc = s.drop_process(s.root());
    CommandDispatcher<> disp{s.doc->context().commandStack};
    disp.submit<Process::SetLoopDuration>(proc, TimeVal::fromMsecs(31.7));
    disp.submit<Process::SetLoop>(proc, true);
    s.plays_on(ms);
    s.runs(proc);
    // Dropped in a box that already loops
    auto& box = s.drop_in_box(s.exec.execution_time(), TimeVal::fromMsecs(10000.));
    disp.submit<Process::SetLoopDuration>(box, TimeVal::fromMsecs(43.1));
    disp.submit<Process::SetLoop>(box, true);
    s.plays_on(ms / 2);
    auto& second = s.drop_process(s.interval_of(box));
    disp.submit<Process::SetLoopDuration>(second, TimeVal::fromMsecs(43.1));
    disp.submit<Process::SetLoop>(second, true);
    s.plays_on(ms);
    s.runs(second);
  });

  p.emplace_back("live-speed", [ms](session& s) {
    auto& box = s.drop_in_box(s.exec.execution_time(), TimeVal::fromMsecs(20000.));
    auto& itv = s.interval_of(box);
    itv.duration.setSpeed(0.37);
    s.plays_on(ms / 2);
    auto& proc = s.drop_process(itv);
    s.plays_on(ms / 2);
    itv.duration.setSpeed(2.3);
    s.plays_on(ms / 2);
    itv.duration.setSpeed(0.);
    spin(ms / 2);
    itv.duration.setSpeed(1.);
    s.plays_on(ms / 2);
    s.runs(proc);
  });

  p.emplace_back("live-paused", [ms](session& s) {
    s.exec.request_play_global(false);
    spin(60);
    auto& proc = s.drop_process(s.root());
    spin(ms / 2);
    s.exec.request_play_global(true);
    s.plays_on(ms);
    s.runs(proc);
  });

  p.emplace_back("live-remove-undo-redo", [ms](session& s) {
    auto& proc = s.drop_process(s.root());
    const auto id = proc.id();
    s.plays_on(ms / 2);
    s.remove(proc);
    s.plays_on(ms / 2);
    expect(!s.by_id(s.root(), id), "the process is still there");
    s.stack().undo();
    s.plays_on(ms / 2);
    auto back = s.by_id(s.root(), id);
    expect(back, "undoing the removal does not bring the process back");
    s.runs(*back);
    s.stack().redo();
    s.plays_on(ms / 2);
    expect(!s.by_id(s.root(), id), "redoing the removal leaves the process");
  });

  p.emplace_back("live-undo-redo-add", [ms](session& s) {
    auto& proc = s.drop_process(s.root());
    const auto id = proc.id();
    s.plays_on(ms / 2);
    s.stack().undo();
    s.plays_on(ms / 2);
    expect(!s.by_id(s.root(), id), "undoing the drop leaves the process");
    s.stack().redo();
    s.plays_on(ms / 2);
    auto back = s.by_id(s.root(), id);
    expect(back, "redoing the drop does not bring the process back");
    s.runs(*back);
  });

  p.emplace_back("live-copy-paste", [ms](session& s) {
    auto& proc = s.drop_process(s.root());
    s.plays_on(ms / 2);
    JSONReader r;
    r.stream.StartObject();
    Process::copyProcesses(r, {&proc}, s.doc->context());
    r.stream.EndObject();
    auto copy = readJson(r.toByteArray());
    const auto before = s.processes(s.root());
    CommandDispatcher<>{s.doc->context().commandStack}.submit(
        new Scenario::Command::PasteProcessesInInterval{
            copy, s.root(), ExpandMode::GrowShrink, QPointF{40., 40.}});
    auto res = s.added(s.root(), before);
    expect(res.size() == 1, "the paste made " + std::to_string(res.size()) + " processes");
    s.plays_on(ms);
    s.runs(*res.front());
  });

  p.emplace_back("live-duplicate", [ms](session& s) {
    auto& proc = s.drop_process(s.root());
    s.plays_on(ms / 2);
    const auto before = s.processes(s.root());
    Scenario::duplicateProcess(s.root(), proc, s.doc->context());
    auto res = s.added(s.root(), before);
    expect(res.size() == 1, "the duplication made " + std::to_string(res.size()) + " processes");
    s.plays_on(ms);
    s.runs(*res.front());
  });

  auto drop_preset = [ms](session& s, const Process::Preset& preset) {
    QMimeData mime;
    mime.setData(score::mime::processpreset(), preset.toJson());
    const auto before = s.processes(s.root());
    expect(s.drop(s.root(), mime), "the preset drop was not handled");
    auto res = s.added(s.root(), before);
    expect(res.size() == 1, "the preset drop made " + std::to_string(res.size()) + " processes");
    s.plays_on(ms);
    s.runs(*res.front());
  };

  p.emplace_back("live-preset-user", [ms, drop_preset](session& s) {
    auto& proc = s.drop_process(s.root());
    s.plays_on(ms / 2);
    drop_preset(s, proc.savePreset());
  });

  p.emplace_back("live-preset-builtin", [drop_preset](session& s) {
    auto& proc = s.drop_process(s.root());
    auto presets = proc.builtinPresets();
    if(presets.empty())
      throw skip{"no built-in preset"};
    drop_preset(s, presets[presets.size() / 2]);
  });

  p.emplace_back("live-preset-change", [ms](session& s) {
    auto& proc = s.drop_process(s.root());
    s.plays_on(ms / 2);
    auto presets = proc.builtinPresets();
    std::vector<Process::Preset> to_load;
    if(presets.size() > 1)
      to_load = {presets[1], presets[0]};
    else if(!presets.empty())
      to_load = {presets[0]};
    to_load.push_back(proc.savePreset());
    auto& factories = s.ctx.interfaces<Process::LoadPresetCommandFactoryList>();
    for(auto& preset : to_load)
    {
      auto cmd = factories.make(
          &Process::LoadPresetCommandFactory::make, proc, preset, s.doc->context());
      expect(cmd, "no command loads the preset");
      CommandDispatcher<>{s.doc->context().commandStack}.submit(cmd);
      s.plays_on(ms / 2);
    }
    s.runs(proc);
  });

  p.emplace_back("live-drop-on-cable", [ms](session& s) {
    auto& a = s.drop_process(s.root());
    auto& b = s.drop_process(s.root());
    auto [out, in] = s.cable_ends(a, b);
    if(!out || !in)
      throw skip{"no outlet and inlet of the same type to put a cable between"};
    s.connect(*out, *in);
    expect(!out->cables().empty(), "the cable was not made");
    s.plays_on(ms / 2);
    auto& cable = out->cables().front().find(s.doc->context());
    const auto before = s.processes(s.root());
    Process::DataflowManager dataflow;
    FocusDispatcher focus;
    const Process::Context pctx{s.doc->context(), dataflow, focus};
    Scenario::createProcessInCable(pctx, s.model(), s.opt.what, std::nullopt, {}, cable);
    auto res = s.added(s.root(), before);
    expect(res.size() == 1, "the drop on the cable made " + std::to_string(res.size()) + " processes");
    s.plays_on(ms);
    s.runs(*res.front());
    // A preset of it on the cable it is now part of
    if(auto& cables = out->cables(); !cables.empty())
    {
      auto& c2 = cables.front().find(s.doc->context());
      const auto before2 = s.processes(s.root());
      Scenario::loadPresetInCable(pctx, s.model(), a.savePreset(), c2);
      auto res2 = s.added(s.root(), before2);
      expect(res2.size() == 1, "the preset drop on the cable made " + std::to_string(res2.size()) + " processes");
      s.plays_on(ms / 2);
      s.runs(*res2.front());
    }
  });

  // As a drop on a node: the new process takes the old one's place
  p.emplace_back("live-replace", [ms](session& s) {
    auto& old = s.drop_process(s.root());
    s.plays_on(ms / 2);
    const auto old_id = old.id();
    Scenario::Command::Macro m{
        new Scenario::Command::DropProcessInIntervalMacro, s.doc->context()};
    auto p = m.createProcessInNewSlot(s.root(), s.opt.what, old.position());
    expect(p, "the replacement was not made");
    m.removeProcess(s.root(), old_id);
    m.commit();
    expect(!s.by_id(s.root(), old_id), "the replaced process is still there");
    s.plays_on(ms);
    s.runs(*p);
  });

  p.emplace_back("live-multi-drop", [ms](session& s) {
    const auto before = s.processes(s.root());
    std::size_t expected = 3;
    if(!s.opt.files.empty())
    {
      QMimeData mime;
      QList<QUrl> urls;
      for(int i = 0; i < 3; i++)
        for(auto& u : s.opt.files)
          urls.push_back(u);
      mime.setUrls(urls);
      expected = urls.size();
      expect(s.drop(s.root(), mime), "the drop of files was not handled");
    }
    else
    {
      // Several drops before the next tick
      for(std::size_t i = 0; i < expected; i++)
      {
        QMimeData mime;
        Mime<Process::ProcessData>::Serializer ser{mime};
        ser.serialize(s.opt.what);
        expect(s.drop(s.root(), mime), "the drop was not handled");
      }
    }
    auto res = s.added(s.root(), before);
    expect(res.size() == expected, "the drop made " + std::to_string(res.size()) + " processes for "
           + std::to_string(expected));
    s.plays_on(ms);
    for(auto* p : res)
      s.runs(*p);
  });

  p.emplace_back("live-editor", [ms](session& s) {
    if(!s.opt.open_editor)
      throw skip{"no editor in this harness"};
    auto& proc = s.drop_process(s.root());
    s.opt.open_editor(proc);
    s.plays_on(ms);
    s.runs(proc);
    if(s.opt.close_editor)
      s.opt.close_editor(proc);
    spin(30);
  });

  // Changing the audio settings stops the playback and restarts the engine:
  // what plays then runs at the new buffer size and rate.
  p.emplace_back("live-audio-settings", [ms](session& s) {
    auto& proc = s.drop_process(s.root());
    s.plays_on(ms / 2);
    auto& set = s.ctx.settings<Audio::Settings::Model>();
    const int bs = set.getBufferSize();
    const int rate = set.getRate();
    struct restore
    {
      Audio::Settings::Model& set;
      int bs, rate;
      ~restore()
      {
        set.setBufferSize(bs);
        set.setRate(rate);
        set.changed();
        spin(100);
      }
    } r{set, bs, rate};
    set.setBufferSize(bs == 256 ? 1024 : 256);
    set.setRate(rate == 48000 ? 44100 : 48000);
    set.changed();
    spin(150);
    s.play();
    s.plays_on(ms);
    s.runs(proc);
  });

  if(!opt.only.empty())
    std::erase_if(p, [&](auto& ph) {
      return std::find(opt.only.begin(), opt.only.end(), ph.first) == opt.only.end();
    });
  return p;
}

//! The names of the phases, in order
inline std::vector<std::string> phase_names()
{
  std::vector<std::string> names;
  for(auto& [name, _] : phases(options{}))
    names.push_back(name);
  return names;
}

//! Runs every phase, each on a document of its own that plays. A phase that
//! fails or throws is recorded and the next one runs.
inline void run(
    const score::GUIApplicationContext& ctx, const options& opt, const recorder& rec)
{
  for(auto& [name, fn] : phases(opt))
  {
    if(rec.phase)
      rec.phase(name);
    std::string verdict = "ok", detail;
    try
    {
      session s{ctx, opt};
      fn(s);
    }
    catch(const skip& e)
    {
      verdict = "skipped";
      detail = e.what();
    }
    catch(const std::exception& e)
    {
      verdict = "fail";
      detail = e.what();
    }
    catch(...)
    {
      verdict = "fail";
      detail = "an exception that is not a std::exception";
    }
    if(rec.result)
      rec.result(name, verdict, detail);
  }
}

}
