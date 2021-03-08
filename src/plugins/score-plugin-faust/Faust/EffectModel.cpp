#include "EffectModel.hpp"

#include <Faust/Commands.hpp>
#include <Faust/Utils.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/std/String.hpp>
#include <score/tools/DeleteAll.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/nodes/faust/faust_node.hpp>

#include <QFileInfo>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include <wobjectimpl.h>

#if __has_include(<sndfile.h>)
#define SAMPLERATE 1
#define FAUST_HAS_SNDFILE 1
#include <faust/gui/SoundUI.h>
#endif
W_OBJECT_IMPL(Faust::FaustEffectModel)
std::list<::GUI*> GUI::fGuiList;
namespace Process
{

template <>
QString EffectProcessFactory_T<Faust::FaustEffectModel>::customConstructionData() const
{
  return "process = _;";
}

template <>
Process::Descriptor
EffectProcessFactory_T<Faust::FaustEffectModel>::descriptor(QString d) const
{
  Process::Descriptor desc;
  return desc;
}
}
namespace Faust
{

FaustEffectModel::FaustEffectModel(
    TimeVal t,
    const QString& faustProgram,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{t, id, "Faust", parent}
{
  init();
  setText(faustProgram);
}

FaustEffectModel::~FaustEffectModel() { }

bool FaustEffectModel::validate(const QString& txt) const noexcept
{
  SCORE_TODO;
  return true;
}

void FaustEffectModel::setText(const QString& txt)
{
  if (txt != m_text)
  {
    m_text = txt;
    if (m_text.isEmpty())
      m_text = "process = _;";
    reload();
    textChanged(m_text);
  }
}

void FaustEffectModel::init() { }

QString FaustEffectModel::prettyName() const noexcept
{
  return m_declareName.isEmpty() ? "Faust" : m_declareName;
}

static bool faustIsMidi(llvm_dsp& dsp)
{
  struct _ final : Meta
  {
    bool midi{};
    void declare(const char* key, const char* value) override
    {
      if (key == std::string("options")
          && std::string(value).find("[midi:on]") != std::string::npos)
        midi = true;
    }
  } meta;
  dsp.metadata(&meta);

  if (meta.midi)
    return true;

  struct _2 final : ::UI
  {
    bool gate{false};
    bool freq{false};
    bool gain{false};

    void openTabBox(const char* label) override { }
    void openHorizontalBox(const char* label) override { }
    void openVerticalBox(const char* label) override { }
    void closeBox() override { }

    // -- active widgets

    void addButton(const char* label, FAUSTFLOAT* zone) override
    {
      if (label == std::string("gate"))
        gate = true;
    }
    void addCheckButton(const char* label, FAUSTFLOAT* zone) override { }
    void addVerticalSlider(
        const char* label,
        FAUSTFLOAT* zone,
        FAUSTFLOAT init,
        FAUSTFLOAT min,
        FAUSTFLOAT max,
        FAUSTFLOAT step) override
    {
      if (label == std::string("freq"))
        freq = true;
      if (label == std::string("gain"))
        gain = true;
    }
    void addHorizontalSlider(
        const char* label,
        FAUSTFLOAT* zone,
        FAUSTFLOAT init,
        FAUSTFLOAT min,
        FAUSTFLOAT max,
        FAUSTFLOAT step) override
    {
      addVerticalSlider(label, zone, init, min, max, step);
    }
    void addNumEntry(
        const char* label,
        FAUSTFLOAT* zone,
        FAUSTFLOAT init,
        FAUSTFLOAT min,
        FAUSTFLOAT max,
        FAUSTFLOAT step) override
    {
    }

    // -- passive widgets

    void addHorizontalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max)
        override
    {
    }
    void addVerticalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max)
        override
    {
    }

    // -- soundfiles

    void addSoundfile(const char* label, const char* filename, Soundfile** sf_zone) override { }

  } ui;

  dsp.buildUserInterface(&ui);
  return ui.freq && ui.gain && ui.gate;
}

void FaustEffectModel::reloadFx(llvm_dsp_factory* fac, llvm_dsp* obj)
{
  static std::vector<std::shared_ptr<llvm_dsp_factory>> dsp_factories;
  const bool had_dsp = bool(faust_object);
  const bool had_poly_dsp = bool(faust_poly_object);
  faust_poly_object.reset();
  faust_poly_factory.reset();

  faust_object.reset(obj);
  faust_factory.reset(fac, deleteDSPFactory);

  dsp_factories.push_back(faust_factory);
  if (had_dsp)
  {
    // Try to reuse controls
    Faust::UpdateUI<decltype(*this), true> ui{*this};
    faust_object->buildUserInterface(&ui);

    std::vector<Process::Inlet*> toRemove;
    for (std::size_t i = ui.i; i < m_inlets.size(); i++)
    {
      toRemove.push_back(m_inlets[i]);
    }
    m_inlets.resize(ui.i);
    for(auto inl : toRemove)
      inl->deleteLater();
  }
  else if ((!m_inlets.empty() || !m_outlets.empty()) && !had_dsp && !had_poly_dsp)
  {
    // loading - controls already exist but not linked to the dsp
    Faust::UpdateUI<decltype(*this), false> ui{*this};
    faust_object->buildUserInterface(&ui);
  }
  else
  {
    // creating a new dsp
    auto inls = score::clearAndDeleteLater(m_inlets);
    auto outls = score::clearAndDeleteLater(m_outlets);

    m_inlets.push_back(new Process::AudioInlet{getStrongId(m_inlets), this});
    auto out = new Process::AudioOutlet{getStrongId(m_outlets), this};
    out->setPropagate(true);
    m_outlets.push_back(out);

    Faust::UI<decltype(*this), false> ui{*this};
    faust_object->buildUserInterface(&ui);
  }
}

void FaustEffectModel::reloadMidi(ossia::nodes::custom_dsp_poly_factory* fac, ossia::nodes::custom_dsp_poly_effect* obj)
{
  static std::vector<std::shared_ptr<ossia::nodes::custom_dsp_poly_factory>> dsp_poly_factories;
  const bool had_dsp = bool(faust_object);
  const bool had_poly_dsp = bool(faust_poly_object);
  faust_poly_object.reset(obj);
  faust_poly_factory.reset(fac);

  faust_object.reset();
  faust_factory.reset();

  dsp_poly_factories.push_back(faust_poly_factory);
  if (had_poly_dsp)
  {
    // updating an existing DSP
    // Try to reuse controls
    Faust::UpdateUI<decltype(*this), true> ui{*this};
    ui.i = 2;
    faust_poly_object->buildUserInterface(&ui);

    std::vector<Process::Inlet*> toRemove;
    for (std::size_t i = ui.i; i < m_inlets.size(); i++)
    {
      toRemove.push_back(m_inlets[i]);
    }
    m_inlets.resize(ui.i);
    for(auto inl : toRemove)
      delete inl;
  }
  else if ((!m_inlets.empty() || !m_outlets.empty()) && !had_poly_dsp && !had_dsp)
  {
    // Try to reuse controls
    Faust::UpdateUI<decltype(*this), false> ui{*this};
    ui.i = 2;
    faust_poly_object->buildUserInterface(&ui);
  }
  else
  {
    auto inls = score::clearAndDeleteLater(m_inlets);
    auto outls = score::clearAndDeleteLater(m_outlets);

    m_inlets.push_back(new Process::AudioInlet{getStrongId(m_inlets), this});
    m_inlets.push_back(new Process::MidiInlet{getStrongId(m_inlets), this});

    auto out = new Process::AudioOutlet{getStrongId(m_outlets), this};
    out->setPropagate(true);
    m_outlets.push_back(out);

    Faust::UI<decltype(*this), true> ui{*this};
    faust_poly_object->buildUserInterface(&ui);
  }
}

void FaustEffectModel::reload()
{
  auto fx_text = m_text.toUtf8();
  if (fx_text.isEmpty())
  {
    return;
  }

  if(QFile f{fx_text}; f.open(QIODevice::ReadOnly))
  {
    m_path = QFileInfo{f}.absolutePath();
    fx_text = f.readAll();
    m_text = fx_text;
  }

  const char* triple =
#if defined(_MSC_VER)
      "x86_64-pc-windows-msvc"
#elif defined(__emscripten__)
      "wasm32-unknown-unknown-wasm"
#elif defined(__aarch64__)
      ""
#elif defined(__arm__)
      "arm-none-linux-gnueabihf"
#else
      ""
#endif
      ;

  std::string fx_path = m_path.toStdString();
  auto str = fx_text.toStdString();
  int argc = fx_path.empty() ? 0 : 2;
  const char* argv[3]{"-I", fx_path.c_str()};

  std::string err;
  err.resize(4097);
  llvm_dsp_factory* fac{};

  fac = createDSPFactoryFromString("score", str, argc, argv, triple, err, -1);

  if (err[0] != 0)
  {
    errorMessage(0, QString::fromStdString(err));
    qDebug() << "Faust error: " << err;
  }
  if (!fac)
  {
    // TODO mark as invalid, like JS
    return;
  }

  auto obj = fac->createDSPInstance();
  if (!obj)
    return;

  if (faustIsMidi(*obj))
  {
    delete obj;
    deleteDSPFactory(fac);
    fac = nullptr;
    {
      auto midi_fac = ossia::nodes::createCustomPolyDSPFactoryFromString("score", str, argc, argv, triple, err, -1);
      auto midi_obj = midi_fac->createPolyDSPInstance(4, true, true);
      reloadMidi(midi_fac, midi_obj);
    }
  }
  else
  {
    reloadFx(fac, obj);
  }

  auto lines = fx_text.split('\n');
  for (int i = 0; i < std::min(5, lines.size()); i++)
  {
    if (lines[i].startsWith("declare name"))
    {
      auto s = lines[i].indexOf('"', 12);
      if (s > 0)
      {
        auto e = lines[i].indexOf('"', s + 1);
        if (e > s)
        {
          m_declareName = lines[i].mid(s + 1, e - s - 1);
          prettyNameChanged();
        }
      }
      break;
    }
  }

  metadata().setLabel(m_declareName);
  inletsChanged();
  outletsChanged();
  changed();
}

}

template <>
void DataStreamReader::read(const Faust::FaustEffectModel& eff)
{
  m_stream << eff.m_text << eff.m_path;
  readPorts(*this, eff.m_inlets, eff.m_outlets);
}

template <>
void DataStreamWriter::write(Faust::FaustEffectModel& eff)
{
  m_stream >> eff.m_text >> eff.m_path;
  eff.reload();
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets, eff.m_outlets, &eff);
}

template <>
void JSONReader::read(const Faust::FaustEffectModel& eff)
{
  obj["Text"] = eff.text();
  obj["Path"] = eff.m_path;
  readPorts(*this, eff.m_inlets, eff.m_outlets);
}

template <>
void JSONWriter::write(Faust::FaustEffectModel& eff)
{
  eff.m_text = obj["Text"].toString();
  eff.m_path = obj["Path"].toString();
  eff.reload();
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets, eff.m_outlets, &eff);
}

namespace Execution
{

FaustEffectComponent::FaustEffectComponent(
    Faust::FaustEffectModel& proc,
    const Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : ProcessComponent_T{proc, ctx, id, "FaustComponent", parent}
{
  connect(&proc, &Faust::FaustEffectModel::changed, this, [=] {
    for(auto& c : this->m_controlConnections)
      QObject::disconnect(c);
    m_controlConnections.clear();

    auto& ctx = system();
    Execution::SetupContext& setup = ctx.setup;
    auto old_node = this->node;

    Execution::Transaction commands{ctx};
    if (old_node)
    {
      setup.unregister_node(process(), this->node, commands);
    }

    reload(commands);

    if (this->node)
    {
      setup.register_node(process(), this->node, commands);
      nodeChanged(old_node, this->node, commands);
    }

    commands.run_all();
  });

  Execution::Transaction commands{ctx};
  reload(commands);
  commands.run_all();
}

void FaustEffectComponent::reload(Execution::Transaction& transaction)
{
  auto& proc = process();
  if (proc.faust_object)
  {
    reloadFx(transaction);
#if FAUST_HAS_SNDFILE
    static SoundUI soundinterface("", system().execState->sampleRate);
    proc.faust_object->buildUserInterface(&soundinterface);
#endif
  }
  else if (proc.faust_poly_object)
  {
    reloadSynth(transaction);
#if FAUST_HAS_SNDFILE
    static SoundUI soundinterface("", system().execState->sampleRate);
    proc.faust_poly_object->buildUserInterface(&soundinterface);
#endif
  }
}

template<typename Node_T>
void FaustEffectComponent::setupExecutionControls(const Node_T& node, int firstControlIndex)
{
  auto& proc = process();
  auto& ctx = system();

  for (std::size_t i = firstControlIndex, N = proc.inlets().size(); i < N; i++)
  {
    auto inlet = static_cast<Process::ControlInlet*>(proc.inlets()[i]);
    *node->controls[i - firstControlIndex].second = ossia::convert<float>(inlet->value());
    auto inl = this->node->root_inputs()[i];
    auto c = connect(inlet, &Process::ControlInlet::valueChanged, this, [this, inl](const ossia::value& v) {
      system().executionQueue.enqueue([inl, val = v]() mutable {
        inl->target<ossia::value_port>()->write_value(std::move(val), 0);
      });
    });
    m_controlConnections.push_back(c);
  }

  typename Node_T::weak_type weak_node = node;
  auto c = con(ctx.doc.coarseUpdateTimer, &QTimer::timeout, this, [weak_node, firstControlIndex, &proc] {
    if (auto node = weak_node.lock())
    {
      for (std::size_t i = firstControlIndex; i < proc.inlets().size(); i++)
      {
        auto inlet = static_cast<Process::ControlInlet*>(proc.inlets()[i]);
        inlet->setValue(*node->controls[i - firstControlIndex].second);
      }
    }
  });

  m_controlConnections.push_back(c);
}

template<typename Node_T>
void FaustEffectComponent::setupExecutionControlOutlets(const Node_T& node, int firstControlIndex)
{
  auto& proc = process();
  auto& ctx = system();

  for (std::size_t i = firstControlIndex, N = proc.outlets().size(); i < N; i++)
  {
    auto outlet = static_cast<Process::ControlOutlet*>(proc.outlets()[i]);
    *node->displays[i - firstControlIndex].second = ossia::convert<float>(outlet->value());
    auto outl = this->node->root_outputs()[i];
    auto c = connect(outlet, &Process::ControlOutlet::valueChanged, this, [this, outl](const ossia::value& v) {
      system().executionQueue.enqueue([outl, val = v]() mutable {
        outl->target<ossia::value_port>()->write_value(std::move(val), 0);
      });
    });
    m_controlConnections.push_back(c);
  }

  typename Node_T::weak_type weak_node = node;
  auto c = con(ctx.doc.coarseUpdateTimer, &QTimer::timeout, this, [weak_node, firstControlIndex, &proc] {
    if (auto node = weak_node.lock())
    {
      for (std::size_t i = firstControlIndex; i < proc.outlets().size(); i++)
      {
        auto outlet = static_cast<Process::ControlOutlet*>(proc.outlets()[i]);
        outlet->setValue(*node->displays[i - firstControlIndex].second);
      }
    }
  });

  m_controlConnections.push_back(c);
}

void FaustEffectComponent::reloadSynth(Execution::Transaction& transaction)
{
  using faust_type = ossia::nodes::faust_synth;
  auto& proc = process();
  auto& ctx = system();

  proc.faust_poly_object->init(ctx.execState->sampleRate);
  auto node = std::make_shared<faust_type>(proc.faust_poly_object);
  this->node = node;

  if(!m_ossia_process)
    m_ossia_process = std::make_shared<ossia::node_process>(node);
  else
    ctx.setup.replace_node(m_ossia_process, node, transaction);

  setupExecutionControls(node, 2);
  setupExecutionControlOutlets(node, 1);
}

void FaustEffectComponent::reloadFx(Execution::Transaction& transaction)
{
  using faust_type = ossia::nodes::faust_fx;
  auto& proc = process();
  auto& ctx = system();
  proc.faust_object->init(ctx.execState->sampleRate);
  auto node = std::make_shared<faust_type>(proc.faust_object);
  this->node = node;

  if(!m_ossia_process)
    m_ossia_process = std::make_shared<ossia::node_process>(node);
  else
    ctx.setup.replace_node(m_ossia_process, node, transaction);

  setupExecutionControls(node, 1);
  setupExecutionControlOutlets(node, 1);
}

}
W_OBJECT_IMPL(Execution::FaustEffectComponent)
