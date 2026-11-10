// VST 2 processes in documents, with the MIDI ports a process gets when its
// plug-in receives MIDI events (a MIDI input, synth flag or not) or sends them
// (a MIDI output).
//
// A document saved before those ports existed loads as it was saved: the same
// ports, cables, values and presets, and it executes. Its presets load in new
// processes. New processes, cabled to their MIDI ports, are saved and loaded
// in both formats, copied and pasted, duplicated alone and with their
// interval, removed, and given a preset dropped on them or in their interval;
// each of these is undone and redone.
//
// The plug-ins: UnflaggedInstrument (tests/unit/vst-fixtures), which takes
// and sends MIDI without the synth flag, and the validation plug-ins Kars, a
// synth, and MaFreeverb, an effect (dpf-plugins-vst, in /usr/lib/vst or
// SCORE_VALIDATION_PLUGINS/vst), without which the test is skipped.
//
// tests/testdata/vst2/before-midi-ports/ was written by "VST 2: write the
// documents" below, with SCORE_VST_DOCUMENTS_OUT set, by a build of this
// branch whose Model::create and Model::initFx made processes as a04edef446
// did: a MIDI input for effFlagsIsSynth only, no MIDI output, program 0
// selected. The serializers are those of a04edef446. Its manifest.json holds
// what that build saw: the ports, the values and the cables of each process.

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionSetup.hpp>
#include <Process/Preset.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Interval/InsertContentInInterval.hpp>
#include <Scenario/Commands/Interval/RemoveProcessFromInterval.hpp>
#include <Scenario/Commands/LoadPresetCommand.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Commands/Scenario/DuplicateInterval.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Audio/Settings/Model.hpp>
#include <Dataflow/Commands/EditConnection.hpp>
#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Vst/ApplicationPlugin.hpp>
#include <Vst/Control.hpp>
#include <Vst/EffectModel.hpp>
#include <Vst/Node.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/selection/SelectionStack.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/thread.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <libremidi/message.hpp>
#include <libremidi/ump.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Process.hpp>
#include <score_test/Project.hpp>

#include <dlfcn.h>

#include <map>
#include <optional>
#include <set>
#include <string>

namespace
{
const QString vst_uuid = QStringLiteral("be8e6bd3-75f2-4102-8895-8a4eb4ea545a");
const QString midi_uuid = QStringLiteral("c189e507-3afa-4a53-9369-e38860e6bb2d");
const QString lfo_uuid = QStringLiteral("0b1b1816-c33e-4796-a16d-5aab27fe600f");

// The values the patch sets, away from every default
constexpr float unflagged_level = 0.3f;
// Its first parameter, Sustain, is a toggle
constexpr float kars_first = 1.f;
constexpr float freeverb_first = 0.2f;

struct Plugin
{
  QString path;
  QString name;
  int32_t id{};
  bool synth{};
};

//! The unique id and synth flag of the VST 2 plug-in in `path`, as its entry
//! point makes it; nothing when it does not load
std::optional<Plugin> probe(const QString& path, const QString& name)
{
  void* h = dlopen(path.toUtf8().constData(), RTLD_NOW | RTLD_LOCAL);
  if(!h)
    return {};
  using entry_t = AEffect* (*)(audioMasterCallback);
  auto entry = reinterpret_cast<entry_t>(dlsym(h, "VSTPluginMain"));
  // DPF names it as the VST SDK before 2.4 did
  if(!entry)
    entry = reinterpret_cast<entry_t>(dlsym(h, "main"));
  if(!entry)
    return {};
  AEffect* fx
      = entry([](AEffect*, int32_t op, int32_t, intptr_t, void*, float) -> intptr_t {
    switch(op)
    {
      case audioMasterVersion:
        return 2400;
      case audioMasterGetSampleRate:
        return 44100;
      case audioMasterGetBlockSize:
        return 512;
      default:
        return 0;
    }
  });
  if(!fx || fx->magic != kEffectMagic)
    return {};
  Plugin p{path, name, fx->uniqueID, bool(fx->flags & effFlagsIsSynth)};
  fx->dispatcher(fx, effClose, 0, 0, nullptr, 0.f);
  return p;
}

struct Plugins
{
  std::optional<Plugin> unflagged, kars, freeverb;

  Plugins()
  {
    const QString env = qEnvironmentVariable("SCORE_VALIDATION_PLUGINS");
    const QString root = (env.isEmpty() ? QStringLiteral("/usr/lib") : env) + "/vst/";
    unflagged = probe(
        QStringLiteral(SCORE_VST_UNFLAGGED_INSTRUMENT),
        QStringLiteral("UnflaggedInstrument"));
    kars = probe(root + "Kars-vst.so", QStringLiteral("Kars"));
    freeverb = probe(root + "MaFreeverb-vst.so", QStringLiteral("MaFreeverb"));
  }

  std::string missing() const
  {
    if(!unflagged)
      return "the UnflaggedInstrument fixture does not load";
    if(!kars || !freeverb)
      return "Kars-vst.so or MaFreeverb-vst.so is not installed (dpf-plugins-vst; "
             "SCORE_VALIDATION_PLUGINS sets where they are looked for)";
    return {};
  }

  //! What the scan would have listed for them, for an engine at 44.1 kHz in
  //! blocks of 512
  void install(const score::GUIApplicationContext& ctx) const
  {
    auto& audio = ctx.settings<Audio::Settings::Model>();
    audio.setRate(44100);
    audio.setBufferSize(512);
    auto& app = ctx.applicationPlugin<vst::ApplicationPlugin>();
    app.vst_infos.clear();
    for(auto& p : {unflagged, kars, freeverb})
    {
      vst::VSTInfo info;
      info.path = p->path;
      info.prettyName = p->name;
      info.uniqueID = p->id;
      info.isSynth = p->synth;
      info.isValid = true;
      app.vst_infos.push_back(info);
      app.vst_modules.insert({p->id, nullptr});
    }
  }
};

void spin()
{
  QApplication::processEvents();
  QApplication::processEvents();
}

Scenario::ScenarioDocumentModel& model(score::Document& doc)
{
  return score::IDocument::get<Scenario::ScenarioDocumentModel>(doc);
}

Process::ProcessModel* by_label(const Scenario::IntervalModel& itv, const QString& label)
{
  for(auto& p : itv.processes)
    if(p.metadata().getLabel() == label)
      return const_cast<Process::ProcessModel*>(&p);
  return nullptr;
}

std::vector<Process::ProcessModel*>
all_labelled(const Scenario::IntervalModel& itv, const QString& label)
{
  std::vector<Process::ProcessModel*> res;
  for(auto& p : itv.processes)
    if(p.metadata().getLabel() == label)
      res.push_back(const_cast<Process::ProcessModel*>(&p));
  return res;
}

QString kind(const Process::Port& p)
{
  if(qobject_cast<const Process::ControlInlet*>(&p))
    return QStringLiteral("control");
  switch(p.type())
  {
    case Process::PortType::Audio:
      return QStringLiteral("audio");
    case Process::PortType::Midi:
      return QStringLiteral("midi");
    case Process::PortType::Message:
      return QStringLiteral("value");
    default:
      return QStringLiteral("other");
  }
}

//! Every port of the process, in order: "in:midi:MIDI In"
QStringList ports(const Process::ProcessModel& proc)
{
  QStringList res;
  for(auto* p : proc.inlets())
    res << "in:" + kind(*p) + ":" + p->name();
  for(auto* p : proc.outlets())
    res << "out:" + kind(*p) + ":" + p->name();
  return res;
}

Process::Inlet* inlet(const Process::ProcessModel& proc, const QString& name)
{
  for(auto* p : proc.inlets())
    if(p->name() == name)
      return p;
  return nullptr;
}

Process::Outlet* outlet(const Process::ProcessModel& proc, const QString& name)
{
  for(auto* p : proc.outlets())
    if(p->name() == name)
      return p;
  return nullptr;
}

Process::Outlet* first_outlet(const Process::ProcessModel& proc, Process::PortType t)
{
  for(auto* p : proc.outlets())
    if(p->type() == t)
      return p;
  return nullptr;
}

std::vector<vst::ControlInlet*> controls(const Process::ProcessModel& proc)
{
  std::vector<vst::ControlInlet*> res;
  for(auto* p : proc.inlets())
    if(auto c = qobject_cast<vst::ControlInlet*>(p))
      res.push_back(c);
  return res;
}

//! The value of each control of a VST process, by parameter number, and
//! whether the plug-in has that value too
std::map<int, float> values(const Process::ProcessModel& proc)
{
  std::map<int, float> res;
  auto* vst = qobject_cast<const vst::Model*>(&proc);
  for(auto* c : controls(proc))
  {
    res[c->fxNum] = ossia::convert<float>(c->value());
    if(vst && vst->fx)
      CHECK(
          vst->fx->getParameter(c->fxNum) == Catch::Approx(res[c->fxNum]).margin(1e-4));
  }
  return res;
}

QString port_label(const Process::Port& port)
{
  auto proc = qobject_cast<const Process::ProcessModel*>(port.parent());
  return (proc ? proc->metadata().getLabel() : QStringLiteral("?")) + ":" + port.name();
}

//! Every cable of the document between ports of `itv`'s processes, as
//! "label:port->label:port"
std::multiset<QString> cables(score::Document& doc, const Scenario::IntervalModel& itv)
{
  std::set<const QObject*> procs;
  for(auto& p : itv.processes)
    procs.insert(&p);
  std::multiset<QString> res;
  for(auto& c : model(doc).cables)
  {
    auto& src = c.source().find(doc.context());
    auto& snk = c.sink().find(doc.context());
    if(procs.count(src.parent()) && procs.count(snk.parent()))
      res.insert(port_label(src) + "->" + port_label(snk));
  }
  return res;
}

void connect(
    score::Document& doc, const Process::Port* source, const Process::Port* sink)
{
  REQUIRE(source);
  REQUIRE(sink);
  auto& m = model(doc);
  CommandDispatcher<>{doc.context().commandStack}.submit<Dataflow::CreateCable>(
      m, getStrongId(m.cables), Process::CableType::ImmediateGlutton, *source, *sink);
}

//! A new interval of the base scenario, after `after` or from its first state
Scenario::IntervalModel&
new_interval(score::Document& doc, const Scenario::IntervalModel* after = nullptr)
{
  auto& scenar = score::test::base_scenario(doc);
  auto create = new Scenario::Command::CreateInterval_State_Event_TimeSync{
      scenar, after ? after->endState() : scenar.states.begin()->id(),
      TimeVal::fromMsecs(2000), after ? 0.7 : 0.3, false};
  const auto id = create->createdInterval();
  CommandDispatcher<>{doc.context().commandStack}.submit(create);
  return scenar.interval(id);
}

Process::ProcessModel*
add(score::Document& doc, Scenario::IntervalModel& itv, const QString& uuid,
    const QString& data, const QString& label)
{
  auto& factories = doc.context().app.interfaces<Process::ProcessFactoryList>();
  auto* factory = factories.get(UuidKey<Process::ProcessModel>::fromString(uuid));
  if(!factory)
    return nullptr;
  auto* cmd = new Scenario::Command::AddOnlyProcessToInterval{
      itv, factory->concreteKey(), data, QPointF{}};
  CommandDispatcher<>{doc.context().commandStack}.submit(cmd);
  auto it = itv.processes.find(cmd->processId());
  if(it == itv.processes.end())
    return nullptr;
  it->metadata().setLabel(label);
  return &*it;
}

//! The program the plug-in of a VST process is on, -1 for none
int program_of(const Process::ProcessModel& proc)
{
  auto* vst = qobject_cast<const vst::Model*>(&proc);
  if(!vst || !vst->fx || vst->fx->fx->numPrograms <= 0)
    return -1;
  return int(vst->fx->dispatch(effGetProgram, 0, 0, nullptr, 0.f));
}

//! A piano roll "notes" and an LFO "lfo" drive "unflagged", whose MIDI output
//! plays "kars" as the notes do; both go through "freeverb". Cables are made
//! to the ports the processes have. "unflagged" is put on its sixth program
//! first, unless `program` is false.
void make_patch(
    score::Document& doc, Scenario::IntervalModel& itv, const Plugins& plugs,
    bool program = true)
{
  auto* notes = add(doc, itv, midi_uuid, {}, "notes");
  auto* lfo = add(doc, itv, lfo_uuid, {}, "lfo");
  auto* unflagged
      = add(doc, itv, vst_uuid, QString::number(plugs.unflagged->id), "unflagged");
  auto* kars = add(doc, itv, vst_uuid, QString::number(plugs.kars->id), "kars");
  auto* freeverb
      = add(doc, itv, vst_uuid, QString::number(plugs.freeverb->id), "freeverb");
  REQUIRE(notes);
  REQUIRE(lfo);
  REQUIRE(unflagged);
  REQUIRE(kars);
  REQUIRE(freeverb);
  REQUIRE(qobject_cast<vst::Model*>(unflagged)->fx);
  REQUIRE(qobject_cast<vst::Model*>(kars)->fx);
  REQUIRE(qobject_cast<vst::Model*>(freeverb)->fx);

  auto* notes_out = first_outlet(*notes, Process::PortType::Midi);
  connect(doc, notes_out, inlet(*kars, "MIDI In"));
  connect(doc, outlet(*kars, "Audio Out"), inlet(*freeverb, "Audio In"));
  connect(doc, outlet(*unflagged, "Audio Out"), inlet(*freeverb, "Audio In"));
  connect(
      doc, first_outlet(*lfo, Process::PortType::Message), inlet(*unflagged, "Level"));
  if(auto* in = inlet(*unflagged, "MIDI In"))
    connect(doc, notes_out, in);
  if(auto* out = outlet(*unflagged, "MIDI Out"))
    connect(doc, out, inlet(*kars, "MIDI In"));

  if(program)
    qobject_cast<vst::Model*>(unflagged)->selectProgram(5);
  controls(*unflagged).at(0)->setValue(unflagged_level);
  controls(*kars).at(0)->setValue(kars_first);
  controls(*freeverb).at(0)->setValue(freeverb_first);
  spin();
}

const QStringList vst_labels{"unflagged", "kars", "freeverb"};

//! What is checked of a patch: the ports and values of its VST processes and
//! its cables
QJsonObject describe(score::Document& doc, const Scenario::IntervalModel& itv)
{
  QJsonObject procs;
  for(auto& label : vst_labels)
  {
    auto* p = by_label(itv, label);
    REQUIRE(p);
    QJsonObject vals;
    for(auto [num, v] : values(*p))
      vals[QString::number(num)] = double(v);
    procs[label] = QJsonObject{
        {"ports", QJsonArray::fromStringList(ports(*p))},
        {"values", vals},
        {"program", program_of(*p)}};
  }
  QJsonArray cs;
  for(auto& c : cables(doc, itv))
    cs.append(c);
  return QJsonObject{{"processes", procs}, {"cables", cs}};
}

//! The patch of `itv` is the one `expected` describes, to the float values
void check_patch(
    score::Document& doc, const Scenario::IntervalModel& itv,
    const QJsonObject& expected)
{
  const auto now = describe(doc, itv);
  const auto eprocs = expected["processes"].toObject();
  const auto nprocs = now["processes"].toObject();
  for(auto& label : vst_labels)
  {
    INFO(label.toStdString());
    const auto e = eprocs[label].toObject();
    const auto n = nprocs[label].toObject();
    CHECK(n["ports"].toArray().toVariantList() == e["ports"].toArray().toVariantList());
    // Not in the documents saved before programs were
    if(e.contains("program"))
      CHECK(n["program"].toInt() == e["program"].toInt());
    const auto ev = e["values"].toObject();
    const auto nv = n["values"].toObject();
    CHECK(nv.keys() == ev.keys());
    for(auto& k : ev.keys())
    {
      INFO("parameter " << k.toStdString());
      CHECK(nv[k].toDouble() == Catch::Approx(ev[k].toDouble()).margin(1e-4));
    }
  }
  QStringList ec, nc;
  for(auto v : expected["cables"].toArray())
    ec << v.toString();
  for(auto v : now["cables"].toArray())
    nc << v.toString();
  CHECK(nc == ec);
}

score::Document* load_file(const score::GUIApplicationContext& ctx, const QString& path)
{
  QFile f{path};
  REQUIRE(f.open(QIODevice::ReadOnly));
  auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
  auto* doc = ctx.docManager.loadDocument(
      ctx, QFileInfo{path}.fileName(), f.readAll(), JSONObject::type(),
      *delegates.begin());
  spin();
  return doc;
}

//! The interval of the base scenario that holds the patch
Scenario::IntervalModel& patch_interval(score::Document& doc)
{
  for(auto& itv : score::test::base_scenario(doc).intervals)
    if(by_label(itv, "unflagged"))
      return itv;
  FAIL("no interval with the patch");
  throw;
}

Execution::DocumentPlugin& load_execution(score::Document& doc)
{
  auto& plug = doc.context().plugin<Execution::DocumentPlugin>();
  plug.reload(true, score::test::base_interval(doc));
  score::test::run_exec(plug);
  return plug;
}

//! The exec node of each VST process of the patch has the ports of the
//! process, of their types, and each cable of the patch is an edge between
//! the exec ports of its ends. A block played with a note held makes Kars and
//! the fixture sound, and the fixture send the note back when it has a MIDI
//! output.
void check_execution(score::Document& doc, Scenario::IntervalModel& itv)
{
  auto& plug = load_execution(doc);
  auto& setup = plug.context().setup;
  REQUIRE(plug.baseScenario());

  for(auto& label : vst_labels)
  {
    INFO(label.toStdString());
    auto* proc = qobject_cast<vst::Model*>(by_label(itv, label));
    REQUIRE(proc);
    std::shared_ptr<ossia::graph_node> node;
    for(auto& [p, n] : setup.inlets)
      if(p->parent() == proc)
        node = n.first;
    REQUIRE(node);
    REQUIRE(node->root_inputs().size() == proc->inlets().size());
    REQUIRE(node->root_outputs().size() == proc->outlets().size());
    for(std::size_t i = 0; i < proc->inlets().size(); i++)
    {
      auto it = setup.inlets.find(proc->inlets()[i]);
      REQUIRE(it != setup.inlets.end());
      CHECK(it->second.second == node->root_inputs()[i]);
      const auto t = proc->inlets()[i]->type();
      if(t == Process::PortType::Midi)
        CHECK(node->root_inputs()[i]->which() == ossia::midi_port::which);
      else if(t == Process::PortType::Audio)
        CHECK(node->root_inputs()[i]->which() == ossia::audio_port::which);
      else
        CHECK(node->root_inputs()[i]->which() == ossia::value_port::which);
    }
    for(std::size_t i = 0; i < proc->outlets().size(); i++)
    {
      auto it = setup.outlets.find(proc->outlets()[i]);
      REQUIRE(it != setup.outlets.end());
      CHECK(it->second.second == node->root_outputs()[i]);
      const auto t = proc->outlets()[i]->type();
      CHECK(
          node->root_outputs()[i]->which()
          == (t == Process::PortType::Midi ? ossia::midi_port::which
                                           : ossia::audio_port::which));
    }
  }

  for(auto& c : model(doc).cables)
  {
    auto& src = c.source().find(doc.context());
    auto& snk = c.sink().find(doc.context());
    auto e = setup.m_cables.find(c.id());
    REQUIRE(e != setup.m_cables.end());
    REQUIRE(e->second);
    auto so = setup.outlets.find(static_cast<Process::Outlet*>(&src));
    auto si = setup.inlets.find(static_cast<Process::Inlet*>(&snk));
    REQUIRE(so != setup.outlets.end());
    REQUIRE(si != setup.inlets.end());
    CHECK(e->second->out == so->second.second);
    CHECK(e->second->in == si->second.second);
  }

  // One block, a note held, in each VST node as the graph would run it
  ossia::execution_state state;
  state.sampleRate = 44100;
  state.bufferSize = 512;
  ossia::token_request tk;
  tk.prev_date = ossia::time_value{0};
  tk.date = ossia::time_value{512};
  tk.start_sample = 0;
  tk.length_sample = 512;
  tk.tempo = 120.;
  for(auto& label : vst_labels)
  {
    INFO(label.toStdString());
    auto* proc = qobject_cast<vst::Model*>(by_label(itv, label));
    std::shared_ptr<ossia::graph_node> node;
    for(auto& [p, n] : setup.inlets)
      if(p->parent() == proc)
        node = n.first;
    REQUIRE(node);
    if(proc->takesMidi())
      static_cast<ossia::midi_inlet*>(node->root_inputs()[1])
          ->data.messages.push_back(
              libremidi::ump_from_midi1(libremidi::channel_events::note_on(1, 60, 100)));
    ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
    node->run(tk, ossia::exec_state_facade{&state});
    ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
    if(!proc->takesMidi())
      continue;
    double peak = 0.;
    auto& audio = node->root_outputs()[0]->cast<ossia::audio_port>();
    for(std::size_t c = 0; c < audio.channels(); c++)
      for(double v : audio.channel(c))
        peak = std::max(peak, std::abs(v));
    CHECK(peak > 0.);
    if(proc->sendsMidi())
      CHECK(!node->root_outputs()[1]->cast<ossia::midi_port>().messages.empty());
    static_cast<ossia::midi_inlet*>(node->root_inputs()[1])->data.messages.clear();
  }
}

QString documents_dir()
{
  return QStringLiteral(SCORE_VST2_DOCUMENTS_DIR) + "/before-midi-ports";
}

QJsonObject read_manifest()
{
  QFile f{documents_dir() + "/manifest.json"};
  REQUIRE(f.open(QIODevice::ReadOnly));
  return QJsonDocument::fromJson(f.readAll()).object();
}

void undo_redo(score::Document& doc)
{
  doc.commandStack().undo();
  spin();
  doc.commandStack().redo();
  spin();
}

//! The preset dropped in `itv`: a new process of its plug-in, with it
Process::ProcessModel* drop_preset(
    score::Document& doc, Scenario::IntervalModel& itv, const Process::Preset& preset,
    const QString& label)
{
  Process::ProcessModel* dropped{};
  {
    Scenario::Command::Macro m{
        new Scenario::Command::DropProcessInIntervalMacro, doc.context()};
    dropped = m.loadProcessFromPreset(itv, preset, QPointF{});
    m.commit();
  }
  if(dropped)
    dropped->metadata().setLabel(label);
  return dropped;
}

//! The preset dropped on `proc`, as the library loads it
void drop_preset_on(
    score::Document& doc, Process::ProcessModel& proc, const Process::Preset& preset)
{
  auto& factories
      = doc.context().app.interfaces<Process::LoadPresetCommandFactoryList>();
  auto* cmd = factories.make(
      &Process::LoadPresetCommandFactory::make, proc, preset, doc.context());
  REQUIRE(cmd);
  CHECK(cmd->key() == Scenario::Command::LoadPreset::static_key());
  CommandDispatcher<>{doc.context().commandStack}.submit(cmd);
  spin();
}
}

TEST_CASE("VST 2: write the documents", "[.vst-documents]")
{
  const QString out = qEnvironmentVariable("SCORE_VST_DOCUMENTS_OUT");
  if(out.isEmpty())
    SKIP("SCORE_VST_DOCUMENTS_OUT is not set");
  qputenv("SCORE_DISABLE_AUDIOPLUGINS", "1");
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    const Plugins plugs;
    if(auto m = plugs.missing(); !m.empty())
      FAIL(m);
    plugs.install(ctx);

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& itv = new_interval(*doc);
    make_patch(*doc, itv, plugs, false);

    QDir{}.mkpath(out);
    score::test::write_file(out + "/document.score", score::test::save_as_json(*doc));

    auto manifest = describe(*doc, itv);
    QJsonObject presets;
    for(auto& label : vst_labels)
    {
      auto* p = by_label(itv, label);
      auto preset = p->savePreset();
      preset.name = label;
      score::test::write_file(out + "/" + label + ".scp", preset.toJson());
      QJsonObject vals;
      for(auto [num, v] : values(*p))
        vals[QString::number(num)] = double(v);
      presets[label] = vals;
    }
    manifest["presets"] = presets;
    score::test::write_file(
        out + "/manifest.json", QJsonDocument{manifest}.toJson(QJsonDocument::Indented));
  });
}

namespace
{
void old_document(const score::GUIApplicationContext& ctx, int part)
{
  const Plugins plugs;
  if(auto m = plugs.missing(); !m.empty())
    SKIP(m);
  plugs.install(ctx);
  const auto manifest = read_manifest();

  // What the document holds: the fixture without its MIDI ports
  const auto saved = manifest["processes"].toObject();
  CHECK(
      saved["unflagged"].toObject()["ports"].toArray().toVariantList()
      == QVariantList{"in:audio:Audio In", "in:control:Level", "out:audio:Audio Out"});
  CHECK(saved["kars"].toObject()["ports"].toArray().contains("in:midi:MIDI In"));

  auto* doc = load_file(ctx, documents_dir() + "/document.score");
  REQUIRE(doc);
  auto& itv = patch_interval(*doc);
  check_patch(*doc, itv, manifest);
  auto* unflagged = qobject_cast<vst::Model*>(by_label(itv, "unflagged"));
  REQUIRE(unflagged);
  CHECK(!unflagged->takesMidi());
  CHECK(!unflagged->sendsMidi());
  // On the program it was set up on, selected once, as before programs were saved
  CHECK(program_of(*unflagged) == 1);
  CHECK(unflagged->dispatch(effVendorSpecific, 1) == 1);
  CHECK(unflagged->dispatch(effVendorSpecific, 0) == 0);
  CHECK(qobject_cast<vst::Model*>(by_label(itv, "kars"))->takesMidi());

  check_execution(*doc, itv);

  // Saved again, in both formats, it is the same
  if(part == 0)
  {
    for(auto* again :
        {score::test::reload_via_json(ctx, *doc),
         score::test::reload_via_bytes(ctx, *doc)})
    {
      REQUIRE(again);
      check_patch(*again, patch_interval(*again), manifest);
    }
  }

  // Its processes are removed and restored with their cables
  if(part == 1)
  {
    const auto before = cables(*doc, itv);
    CommandDispatcher<>{doc->context().commandStack}.submit(
        new Scenario::Command::RemoveProcessFromInterval{itv, unflagged->id()});
    spin();
    CHECK(!by_label(itv, "unflagged"));
    doc->commandStack().undo();
    spin();
    check_patch(*doc, itv, manifest);
    CHECK(cables(*doc, itv) == before);
    CHECK(!qobject_cast<vst::Model*>(by_label(itv, "unflagged"))->takesMidi());
    doc->commandStack().redo();
    spin();
    CHECK(!by_label(itv, "unflagged"));
  }
}

void old_presets(const score::GUIApplicationContext& ctx)
{
  const Plugins plugs;
  if(auto m = plugs.missing(); !m.empty())
    SKIP(m);
  plugs.install(ctx);
  const auto manifest = read_manifest();
  const auto presets = manifest["presets"].toObject();
  auto& procs = ctx.interfaces<Process::ProcessFactoryList>();

  auto* doc = score::test::new_document(ctx);
  REQUIRE(doc);
  auto& itv = new_interval(*doc);
  make_patch(*doc, itv, plugs);
  const auto patched = cables(*doc, itv);

  for(auto& label : vst_labels)
  {
    INFO(label.toStdString());
    auto preset
        = Process::Preset::fromFile(procs, documents_dir() + "/" + label + ".scp");
    REQUIRE(preset);
    const auto expected = presets[label].toObject();
    auto check_values = [&](const Process::ProcessModel& p) {
      const auto v = values(p);
      for(auto& k : expected.keys())
        CHECK(v.at(k.toInt()) == Catch::Approx(expected[k].toDouble()).margin(1e-4));
    };

    // Dropped on a cabled process: its ports and cables stay
    auto* proc = by_label(itv, label);
    REQUIRE(proc);
    for(auto* c : controls(*proc))
      c->setValue(0.9f);
    const auto layout = ports(*proc);
    drop_preset_on(*doc, *proc, *preset);
    check_values(*proc);
    CHECK(ports(*proc) == layout);
    CHECK(cables(*doc, itv) == patched);
    undo_redo(*doc);
    check_values(*by_label(itv, label));

    // Dropped in the interval: a new process of its plug-in, with the
    // ports a new process has
    auto* dropped = drop_preset(*doc, itv, *preset, label + "-dropped");
    REQUIRE(dropped);
    check_values(*dropped);
    CHECK(ports(*dropped) == ports(*proc));
    const auto id = dropped->id();
    undo_redo(*doc);
    auto it = itv.processes.find(id);
    REQUIRE(it != itv.processes.end());
    check_values(*it);
    CHECK(ports(*it) == ports(*by_label(itv, label)));
  }
}

void new_processes(const score::GUIApplicationContext& ctx, int part)
{
  const Plugins plugs;
  if(auto m = plugs.missing(); !m.empty())
    SKIP(m);
  plugs.install(ctx);

  auto* doc = score::test::new_document(ctx);
  REQUIRE(doc);
  auto& itv = new_interval(*doc);
  make_patch(*doc, itv, plugs);
  const auto patch = describe(*doc, itv);

  auto* unflagged = qobject_cast<vst::Model*>(by_label(itv, "unflagged"));
  REQUIRE(unflagged);
  REQUIRE(unflagged->takesMidi());
  REQUIRE(unflagged->sendsMidi());
  CHECK(
      ports(*unflagged)
      == QStringList{
          "in:audio:Audio In", "in:midi:MIDI In", "in:control:Level",
          "out:audio:Audio Out", "out:midi:MIDI Out"});
  {
    int to_midi_in = 0, from_midi_out = 0;
    for(auto& c : cables(*doc, itv))
    {
      to_midi_in += c.startsWith("notes:") && c.endsWith("->unflagged:MIDI In");
      from_midi_out += c == "unflagged:MIDI Out->kars:MIDI In";
    }
    CHECK(to_midi_in == 1);
    CHECK(from_midi_out == 1);
  }

  check_execution(*doc, itv);

  // Saved and loaded, in both formats
  if(part == 0)
  {
    for(auto* again :
        {score::test::reload_via_json(ctx, *doc),
         score::test::reload_via_bytes(ctx, *doc)})
    {
      REQUIRE(again);
      auto& aitv = patch_interval(*again);
      check_patch(*again, aitv, patch);
      auto* u = qobject_cast<vst::Model*>(by_label(aitv, "unflagged"));
      CHECK(u->takesMidi());
      CHECK(u->sendsMidi());
      check_execution(*again, aitv);
    }
  }

  // Created, undone and redone
  if(part == 1)
  {
    auto* again
        = add(*doc, itv, vst_uuid, QString::number(plugs.unflagged->id), "again");
    REQUIRE(again);
    const auto layout = ports(*again);
    CHECK(layout == ports(*unflagged));
    doc->commandStack().undo();
    spin();
    CHECK(!by_label(itv, "again"));
    doc->commandStack().redo();
    spin();
    // Remade from the command: the label is not part of it
    std::vector<Process::ProcessModel*> made;
    for(auto& p : itv.processes)
      if(p.effect() == QString::number(plugs.unflagged->id)
         && &p != by_label(itv, "unflagged"))
        made.push_back(&p);
    REQUIRE(made.size() == 1);
    CHECK(ports(*made[0]) == layout);
  }

  // Removed and restored with its cables
  if(part == 2)
  {
    CommandDispatcher<>{doc->context().commandStack}.submit(
        new Scenario::Command::RemoveProcessFromInterval{itv, unflagged->id()});
    spin();
    CHECK(!by_label(itv, "unflagged"));
    for(auto& c : cables(*doc, itv))
      CHECK(!c.contains("unflagged"));
    doc->commandStack().undo();
    spin();
    check_patch(*doc, itv, patch);
    doc->commandStack().redo();
    spin();
    CHECK(!by_label(itv, "unflagged"));
    doc->commandStack().undo();
    spin();
    check_patch(*doc, itv, patch);
    check_execution(*doc, itv);
  }

  // Copied and pasted with the cables between the copies
  if(part == 3)
  {
    doc->context().selectionStack.pushNewSelection(
        Selection{
            by_label(itv, "notes"), by_label(itv, "unflagged"), by_label(itv, "kars")});
    JSONReader r;
    REQUIRE(Scenario::copySelectedProcesses(r, doc->context()));
    const auto copied = r.toByteArray();
    doc->context().selectionStack.deselect();

    // The cables the copies have among themselves
    std::multiset<QString> between_copies;
    for(auto& c : cables(*doc, itv))
      if(!c.contains("lfo:") && !c.contains("freeverb:"))
        between_copies.insert(c);
    REQUIRE(between_copies.size() == 3);

    auto& target = new_interval(*doc, &itv);
    auto json = readJson(copied);
    CommandDispatcher<>{doc->context().commandStack}.submit(
        new Scenario::Command::PasteProcessesInInterval{
            json, target, ExpandMode::GrowShrink, QPointF{}});
    spin();
    auto check_pasted = [&](Scenario::IntervalModel& t) {
      auto* u = by_label(t, "unflagged");
      auto* k = by_label(t, "kars");
      REQUIRE(u);
      REQUIRE(k);
      CHECK(ports(*u) == ports(*unflagged));
      CHECK(ports(*k) == ports(*by_label(itv, "kars")));
      CHECK(values(*u) == values(*unflagged));
      CHECK(values(*k) == values(*by_label(itv, "kars")));
      CHECK(program_of(*u) == 5);
      CHECK(cables(*doc, t) == between_copies);
    };
    check_pasted(target);
    // The originals keep theirs
    check_patch(*doc, itv, patch);
    undo_redo(*doc);
    check_pasted(target);
  }

  // Duplicated alone, and with its interval
  if(part == 4)
  {
    auto* dup = new Scenario::Command::DuplicateOnlyProcessToInterval{itv, *unflagged};
    const auto dupId = dup->processId();
    CommandDispatcher<>{doc->context().commandStack}.submit(dup);
    spin();
    auto check_dup = [&] {
      auto it = itv.processes.find(dupId);
      REQUIRE(it != itv.processes.end());
      auto* d = qobject_cast<vst::Model*>(&*it);
      REQUIRE(d);
      CHECK(ports(*d) == ports(*unflagged));
      CHECK(values(*d) == values(*unflagged));
      CHECK(program_of(*d) == 5);
      CHECK(d->takesMidi());
      CHECK(d->sendsMidi());
    };
    check_dup();
    undo_redo(*doc);
    check_dup();

    auto& scenar = score::test::base_scenario(*doc);
    auto* dupItv = new Scenario::Command::DuplicateInterval{scenar, itv};
    const auto dupItvId = dupItv->createdId();
    CommandDispatcher<>{doc->context().commandStack}.submit(dupItv);
    spin();
    // Its processes, the duplicate among them, with their ports and values;
    // a duplicated interval has no cables, whatever its processes
    auto check_dup_itv = [&] {
      auto& d = scenar.interval(dupItvId);
      CHECK(all_labelled(d, "unflagged").size() == 2);
      for(auto& label : vst_labels)
      {
        INFO(label.toStdString());
        for(auto* p : all_labelled(d, label))
        {
          CHECK(ports(*p) == ports(*by_label(itv, label)));
          CHECK(values(*p) == values(*by_label(itv, label)));
          CHECK(program_of(*p) == program_of(*by_label(itv, label)));
        }
      }
    };
    check_dup_itv();
    undo_redo(*doc);
    check_dup_itv();
  }
}

void dragged_presets(const score::GUIApplicationContext& ctx)
{
  const Plugins plugs;
  if(auto m = plugs.missing(); !m.empty())
    SKIP(m);
  plugs.install(ctx);

  auto* doc = score::test::new_document(ctx);
  REQUIRE(doc);
  auto& itv = new_interval(*doc);
  make_patch(*doc, itv, plugs);
  const auto patched = cables(*doc, itv);
  auto* unflagged = qobject_cast<vst::Model*>(by_label(itv, "unflagged"));
  REQUIRE(unflagged);

  // As the preset button drags it and the library saves it
  auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
  auto preset = Process::Preset::fromJson(
      procs,
      static_cast<const Process::ProcessModel*>(unflagged)->savePreset().toJson());
  REQUIRE(preset);

  auto* dropped = drop_preset(*doc, itv, *preset, "dropped");
  REQUIRE(dropped);
  CHECK(ports(*dropped) == ports(*unflagged));
  CHECK(values(*dropped) == values(*unflagged));
  CHECK(qobject_cast<vst::Model*>(dropped)->takesMidi());
  CHECK(qobject_cast<vst::Model*>(dropped)->sendsMidi());
  const auto id = dropped->id();
  undo_redo(*doc);
  {
    auto it = itv.processes.find(id);
    REQUIRE(it != itv.processes.end());
    CHECK(ports(*it) == ports(*unflagged));
    CHECK(values(*it) == values(*unflagged));
  }

  // Onto the cabled process: its MIDI cables stay
  controls(*unflagged).at(0)->setValue(0.9f);
  drop_preset_on(*doc, *unflagged, *preset);
  CHECK(values(*unflagged).at(0) == Catch::Approx(unflagged_level).margin(1e-4));
  CHECK(cables(*doc, itv) == patched);
  undo_redo(*doc);
  CHECK(
      values(*by_label(itv, "unflagged")).at(0)
      == Catch::Approx(unflagged_level).margin(1e-4));

  // One of its programs: the plug-in is on it, the cables stay
  unflagged = qobject_cast<vst::Model*>(by_label(itv, "unflagged"));
  const auto programs
      = static_cast<const Process::ProcessModel*>(unflagged)->builtinPresets();
  REQUIRE(programs.size() == 8);
  drop_preset_on(*doc, *unflagged, programs[0]);
  CHECK(unflagged->fx->getParameter(0) == Catch::Approx(0.75f).margin(1e-4));
  CHECK(cables(*doc, itv) == patched);
  doc->commandStack().undo();
  spin();
  CHECK(
      values(*by_label(itv, "unflagged")).at(0)
      == Catch::Approx(unflagged_level).margin(1e-4));
}
}

// One application for all: the host callback keeps references to the
// settings of the first application it serves
TEST_CASE(
    "VST 2: documents with processes that receive and send MIDI",
    "[integration][vst][documents]")
{
  qputenv("SCORE_DISABLE_AUDIOPLUGINS", "1");
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    for(int part = 0; part < 2; part++)
    {
      INFO("a document saved before the MIDI ports, part " << part);
      old_document(ctx, part);
      score::test::close_all_documents(ctx);
    }
    {
      INFO("the presets saved before the MIDI ports");
      old_presets(ctx);
      score::test::close_all_documents(ctx);
    }
    for(int part = 0; part < 5; part++)
    {
      INFO("processes with MIDI ports, part " << part);
      new_processes(ctx, part);
      score::test::close_all_documents(ctx);
    }
    {
      INFO("a preset of a process with MIDI ports");
      dragged_presets(ctx);
      score::test::close_all_documents(ctx);
    }
  });
}
