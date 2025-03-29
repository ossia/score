#include "EffectModel.hpp"

#include <Process/Dataflow/PortFactory.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>
#include <Process/PresetHelpers.hpp>

#include <Library/LibrarySettings.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/DeleteAll.hpp>
#include <score/tools/FilePath.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/std/String.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/nodes/faust/faust_node.hpp>
#include <ossia/detail/disable_fpe.hpp>

#include <boost/predef.h>

#include <QDialog>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QFileInfo>
#include <QPlainTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include <Faust/Commands.hpp>
#include <Faust/Descriptor.hpp>
#include <Faust/Utils.hpp>
#if BOOST_ARCH_X86
#include <xmmintrin.h>
#endif
#include <wobjectimpl.h>

#if __has_include(<sndfile.h>)
// Disabled because it breaks build on ArchLinux due to a bug in LibsndfileReader.h
#if defined(SAMPLERATE)
#undef SAMPLERATE
#endif
#define FAUST_HAS_SNDFILE 1
#include <faust/gui/SoundUI.h>
#endif
W_OBJECT_IMPL(Faust::FaustEffectModel)
std::list<::GUI*> GUI::fGuiList;
namespace Process
{

template <>
QString
EffectProcessFactory_T<Faust::FaustEffectModel>::customConstructionData() const noexcept
{
  return "process = _;";
}

// FIXME
template <>
Process::Descriptor
EffectProcessFactory_T<Faust::FaustEffectModel>::descriptor(QString txt) const noexcept
{
  Process::Descriptor d;
  d.category = Process::ProcessCategory::AudioEffect;

  const auto desc = Faust::initDescriptor(txt);
  if(!desc.prettyName.isEmpty())
    d.prettyName = desc.prettyName;
  else
    d.prettyName = "Faust effect";

  if(!desc.author.isEmpty())
    d.author = desc.author;
  else if(!desc.copyright.isEmpty())
    d.author = desc.copyright;

  if(!desc.description.isEmpty())
    d.description = desc.description;
  if(!desc.version.isEmpty())
  {
    d.description += "\n";
    d.description += desc.version;
  }
  if(!desc.license.isEmpty())
  {
    d.description += "\n";
    d.description += desc.license;
  }
  if(!desc.reference.isEmpty())
    d.documentationLink = desc.reference;
  else
    d.documentationLink
        = QUrl(QStringLiteral("https://ossia.io/score-docs/processes/faust.html"));

  return d;
}

template <>
Process::Descriptor EffectProcessFactory_T<Faust::FaustEffectModel>::descriptor(
    const Process::ProcessModel& d) const noexcept
{
  Process::Descriptor desc;
  return descriptor(d.effect());
}
}
namespace Faust
{

static std::vector<std::string> getLibpaths()
{
  auto& lib = score::AppContext().settings<Library::Settings::Model>();

  std::vector<std::string> ret;
  for(auto& path : lib.getIncludePaths())
    ret.push_back(path.toStdString());

  return ret;
}

FaustEffectModel::FaustEffectModel(
    TimeVal t, const QString& faustProgram, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{t, id, "Faust", parent}
{
  init();
  setScript(faustProgram);
}

FaustEffectModel::~FaustEffectModel() { }

bool FaustEffectModel::validate(const QString& txt) const noexcept
{
  SCORE_TODO;
  return true;
}

Process::ScriptChangeResult FaustEffectModel::setScript(const QString& txt)
{
  if(txt != m_script)
  {
    m_script = txt;
    if(m_script.isEmpty())
      m_script = "process = _;";
    auto res = reload();
    scriptChanged(m_script);
    return res;
  }
  return {};
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
      if(key == std::string("options")
         && std::string(value).find("[midi:on]") != std::string::npos)
        midi = true;
    }
  } meta;
  dsp.metadata(&meta);

  if(meta.midi)
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
      if(label == std::string("gate"))
        gate = true;
    }
    void addCheckButton(const char* label, FAUSTFLOAT* zone) override { }
    void addVerticalSlider(
        const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min,
        FAUSTFLOAT max, FAUSTFLOAT step) override
    {
      if(label == std::string("freq"))
        freq = true;
      if(label == std::string("gain"))
        gain = true;
    }
    void addHorizontalSlider(
        const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min,
        FAUSTFLOAT max, FAUSTFLOAT step) override
    {
      addVerticalSlider(label, zone, init, min, max, step);
    }
    void addNumEntry(
        const char* label, FAUSTFLOAT* zone, FAUSTFLOAT init, FAUSTFLOAT min,
        FAUSTFLOAT max, FAUSTFLOAT step) override
    {
      if(label == std::string("freq"))
        freq = true;
      if(label == std::string("gain"))
        gain = true;
    }

    // -- passive widgets

    void addHorizontalBargraph(
        const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max) override
    {
    }
    void addVerticalBargraph(
        const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max) override
    {
    }

    // -- soundfiles

    void
    addSoundfile(const char* label, const char* filename, Soundfile** sf_zone) override
    {
    }

  } ui;

  dsp.buildUserInterface(&ui);
  return ui.freq && ui.gain && ui.gate;
}

Process::ScriptChangeResult FaustEffectModel::reload()
{
  Process::ScriptChangeResult res;
  auto& ctx = score::IDocument::documentContext(*this);
  score::delete_later<Process::Inlets>& inlets_to_clear = res.inlets;
  score::delete_later<Process::Outlets>& outlets_to_clear = res.outlets;

  auto fx_text = m_script.toUtf8();
  if(fx_text.isEmpty())
  {
    return res;
  }

  if(QFile f{fx_text}; f.open(QIODevice::ReadOnly))
  {
    QFileInfo fi{f};
    m_path = score::relativizeFilePath(fi.absolutePath(), ctx);
    fx_text = f.readAll();
    m_script = fx_text;
    m_declareName = fi.completeBaseName();
  }
  else
  {
    m_declareName = QStringLiteral("Faust");
  }

  const char* triple =
#if defined(_WIN32)
      "x86_64-pc-windows-msvc"
#elif defined(__emscripten__)
      "wasm32-unknown-unknown-wasm"
#elif defined(__aarch64__)
      "aarch64-none-linux-gnueabi"
#elif defined(__arm__)
      "arm-none-linux-gnueabihf"
#else
      ""
#endif
      ;

  std::string fx_path = score::locateFilePath(m_path, ctx).toStdString();
  auto str = fx_text.toStdString();

  std::vector<const char*> argv;
  argv.push_back(sizeof(FAUSTFLOAT) == 4 ? "-single" : "-double");
  argv.push_back("-vec");

  if(!fx_path.empty())
  {
    argv.push_back("-I");
    argv.push_back(fx_path.c_str());
  }

  const auto libPaths = getLibpaths();
  for(auto& lib : libPaths)
  {
    argv.push_back("-I");
    argv.push_back(lib.c_str());
  }

  std::string err;
  err.resize(4097);
  llvm_dsp_factory* fac{};

#if BOOST_ARCH_X86
  // https://github.com/grame-cncm/faust/issues/1117
  _mm_setcsr(_MM_MASK_MASK | _MM_FLUSH_ZERO_OFF);
#endif

  fac = createDSPFactoryFromString(
      "score", str, argv.size(), argv.data(), triple, err, -1);

  ossia::disable_fpe();

  if(err[0] != 0)
  {
    errorMessage(0, QString::fromStdString(err));
    qDebug() << "Faust error: " << err;
  }

  if(!fac)
  {
    // TODO mark as invalid, like JS
    return res;
  }

  auto obj = fac->createDSPInstance();
  if(!obj)
    return res;

  res.valid = true;

  if(faustIsMidi(*obj))
  {
    delete obj;
    deleteDSPFactory(fac);
    fac = nullptr;
    {
      auto midi_fac = ossia::nodes::createCustomPolyDSPFactoryFromString(
          "score", str, argv.size(), argv.data(), triple, err, -1);
      auto midi_obj = midi_fac->createPolyDSPInstance(4, true, true);
      {
        auto fac = midi_fac;
        auto obj = midi_obj;

        static std::vector<std::shared_ptr<ossia::nodes::custom_dsp_poly_factory>>
            dsp_poly_factories;
        const bool had_dsp = bool(faust_object);
        const bool had_poly_dsp = bool(faust_poly_object);
        faust_poly_object.reset(obj);
        faust_poly_factory.reset(fac);

        faust_object.reset();
        faust_factory.reset();

        dsp_poly_factories.push_back(faust_poly_factory);
        Process::Inlets toRemove;
        Process::Outlets toRemoveO;
        if(had_poly_dsp)
        {
          // updating an existing DSP
          // Try to reuse controls
          Faust::UpdateUI<decltype(*this), true> ui{*this, toRemove, toRemoveO};
          ui.i = 2;
          ui.o = 1;
          faust_poly_object->buildUserInterface(&ui);

          for(std::size_t i = ui.i; i < m_inlets.size(); i++)
          {
            toRemove.push_back(m_inlets[i]);
          }
          m_inlets.resize(ui.i);

          for(std::size_t i = ui.o; i < m_outlets.size(); i++)
          {
            toRemoveO.push_back(m_outlets[i]);
          }
          m_outlets.resize(ui.o);

          score::clearAndDeleteLater(toRemove, inlets_to_clear);
          score::clearAndDeleteLater(toRemoveO, outlets_to_clear);
        }
        else if((!m_inlets.empty() || !m_outlets.empty()) && !had_poly_dsp && !had_dsp)
        {
          // Try to reuse controls
          Faust::UpdateUI<decltype(*this), false> ui{*this, toRemove, toRemoveO};
          ui.i = 2;
          ui.o = 1;
          faust_poly_object->buildUserInterface(&ui);
        }
        else
        {
          score::clearAndDeleteLater(m_inlets, inlets_to_clear);
          score::clearAndDeleteLater(m_outlets, outlets_to_clear);

          m_inlets.push_back(new Process::AudioInlet{getStrongId(m_inlets), this});
          m_inlets.push_back(new Process::MidiInlet{getStrongId(m_inlets), this});

          auto out = new Process::AudioOutlet{getStrongId(m_outlets), this};
          out->setPropagate(true);
          m_outlets.push_back(out);

          Faust::UI<decltype(*this), true> ui{*this};
          faust_poly_object->buildUserInterface(&ui);
        }
      }
    }
  }
  else
  {
    static std::vector<std::shared_ptr<llvm_dsp_factory>> dsp_factories;
    const bool had_dsp = bool(faust_object);
    const bool had_poly_dsp = bool(faust_poly_object);
    faust_poly_object.reset();
    faust_poly_factory.reset();

    faust_object.reset(obj);
    faust_factory.reset(fac, deleteDSPFactory);

    dsp_factories.push_back(faust_factory);
    Process::Inlets toRemove;
    Process::Outlets toRemoveO;
    if(had_dsp)
    {
      // Try to reuse controls
      Faust::UpdateUI<decltype(*this), true> ui{*this, toRemove, toRemoveO};
      faust_object->buildUserInterface(&ui);

      for(std::size_t i = ui.i; i < m_inlets.size(); i++)
      {
        toRemove.push_back(m_inlets[i]);
      }
      m_inlets.resize(ui.i);

      for(std::size_t i = ui.o; i < m_outlets.size(); i++)
      {
        toRemoveO.push_back(m_outlets[i]);
      }
      m_outlets.resize(ui.o);

      score::clearAndDeleteLater(toRemove, inlets_to_clear);
      score::clearAndDeleteLater(toRemoveO, outlets_to_clear);
    }
    else if((!m_inlets.empty() || !m_outlets.empty()) && !had_dsp && !had_poly_dsp)
    {
      // loading - controls already exist but not linked to the dsp
      Faust::UpdateUI<decltype(*this), false> ui{*this, toRemove, toRemoveO};
      faust_object->buildUserInterface(&ui);
    }
    else
    {
      // creating a new dsp
      score::clearAndDeleteLater(m_inlets, inlets_to_clear);
      score::clearAndDeleteLater(m_outlets, outlets_to_clear);

      m_inlets.push_back(new Process::AudioInlet{getStrongId(m_inlets), this});
      auto out = new Process::AudioOutlet{getStrongId(m_outlets), this};
      out->setPropagate(true);
      m_outlets.push_back(out);

      Faust::UI<decltype(*this), false> ui{*this};
      faust_object->buildUserInterface(&ui);
    }
  }

  auto lines = fx_text.split('\n');
  for(int i = 0; i < std::min(5, int(lines.size())); i++)
  {
    if(lines[i].startsWith("declare name"))
    {
      auto s = lines[i].indexOf('"', 12);
      if(s > 0)
      {
        auto e = lines[i].indexOf('"', s + 1);
        if(e > s)
        {
          m_declareName = lines[i].mid(s + 1, e - s - 1);
          prettyNameChanged();
        }
      }
      break;
    }
  }

  metadata().setName(m_declareName);
  metadata().setLabel(m_declareName);

  return res;
}

QString FaustEffectModel::effect() const noexcept
{
  return m_script;
}

void FaustEffectModel::loadPreset(const Process::Preset& preset)
{
  Process::loadScriptProcessPreset<FaustEffectModel::p_script>(*this, preset);
}

Process::Preset FaustEffectModel::savePreset() const noexcept
{
  return Process::saveScriptProcessPreset(*this, this->m_script);
}

}

template <>
void DataStreamReader::read(const Faust::FaustEffectModel& eff)
{
  m_stream << eff.m_script << eff.m_path;
  readPorts(*this, eff.m_inlets, eff.m_outlets);
}

template <>
void DataStreamWriter::write(Faust::FaustEffectModel& eff)
{
  m_stream >> eff.m_script >> eff.m_path;
  eff.reload();
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets,
      eff.m_outlets, &eff);
}

template <>
void JSONReader::read(const Faust::FaustEffectModel& eff)
{
  obj["Text"] = eff.script();
  obj["Path"] = eff.m_path;
  readPorts(*this, eff.m_inlets, eff.m_outlets);
}

template <>
void JSONWriter::write(Faust::FaustEffectModel& eff)
{
  eff.m_script = obj["Text"].toString();
  if(auto path_it = obj.tryGet("Path"))
    eff.m_path = path_it->toString();
  eff.reload();
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets,
      eff.m_outlets, &eff);
}

namespace Execution
{

FaustEffectComponent::FaustEffectComponent(
    Faust::FaustEffectModel& proc, const Execution::Context& ctx, QObject* parent)
    : ProcessComponent_T{proc, ctx, "FaustComponent", parent}
{
  connect(
      &proc, &Faust::FaustEffectModel::programChanged, this,
      [this, &ctx] {
    for(auto& c : this->m_controlConnections)
      QObject::disconnect(c);
    m_controlConnections.clear();

    const Execution::Context& ctx = system();
    Execution::SetupContext& setup = ctx.setup;
    auto old_node = this->node;

    SCORE_ASSERT(ctx.transaction);
    Execution::Transaction& commands = *ctx.transaction;
    if(old_node)
    {
      setup.unregister_node(process(), this->node, commands);
    }

    reload(commands);

    if(this->node)
    {
      setup.register_node(process(), this->node, commands);
      nodeChanged(old_node, this->node, &commands);
    }
  },
      Qt::DirectConnection);

  Execution::Transaction commands{ctx};
  reload(commands);
  commands.run_all();
}

void FaustEffectComponent::reload(Execution::Transaction& transaction)
{
  auto& proc = process();
  if(proc.faust_object)
  {
    reloadFx(transaction);
#if FAUST_HAS_SNDFILE
    static SoundUI soundinterface("", system().execState->sampleRate);
    proc.faust_object->buildUserInterface(&soundinterface);
#endif
  }
  else if(proc.faust_poly_object)
  {
    reloadSynth(transaction);
#if FAUST_HAS_SNDFILE
    static SoundUI soundinterface("", system().execState->sampleRate);
    proc.faust_poly_object->buildUserInterface(&soundinterface);
#endif
  }
}

// TODO reuse this code
template <typename Node_T>
void FaustEffectComponent::setupExecutionControls(
    const Node_T& node, int firstControlIndex)
{
  auto& proc = process();
  auto& ctx = system();

  for(std::size_t i = firstControlIndex, N = proc.inlets().size(); i < N; i++)
  {
    auto inlet = static_cast<Process::ControlInlet*>(proc.inlets()[i]);

    node->set_control(i - firstControlIndex, ossia::convert<float>(inlet->value()));
    auto inl = this->node->root_inputs()[i];
    auto& vp = *inl->target<ossia::value_port>();
    vp.type = inlet->value().get_type();
    vp.domain = inlet->domain().get();

    auto c = connect(
        inlet, &Process::ControlInlet::valueChanged, this,
        [this, inl](const ossia::value& v) {
      system().executionQueue.enqueue([inl, val = v]() mutable {
        inl->target<ossia::value_port>()->write_value(std::move(val), 0);
      });
        });
    m_controlConnections.push_back(c);
  }

  typename Node_T::weak_type weak_node = node;
  auto c = con(
      ctx.doc.coarseUpdateTimer, &QTimer::timeout, this,
      [weak_node, firstControlIndex, &proc] {
    if(auto node = weak_node.lock())
    {
      for(int i = firstControlIndex; i < std::ssize(proc.inlets()); i++)
      {
        auto inlet = static_cast<Process::ControlInlet*>(proc.inlets()[i]);
        int idx = i - firstControlIndex;
        if(idx >= 0 && idx < node->controls.size())
          inlet->setExecutionValue(*node->controls[i - firstControlIndex].second);
        else
          qDebug() << idx << node->controls.size();
      }
    }
      });

  m_controlConnections.push_back(c);
}

template <typename Node_T>
void FaustEffectComponent::setupExecutionControlOutlets(
    const Node_T& node, int firstControlIndex)
{
  auto& proc = process();
  auto& ctx = system();

  for(std::size_t i = firstControlIndex, N = proc.outlets().size(); i < N; i++)
  {
    auto outlet = static_cast<Process::ControlOutlet*>(proc.outlets()[i]);
    *node->displays[i - firstControlIndex].second
        = ossia::convert<float>(outlet->value());
    auto outl = this->node->root_outputs()[i];
    auto& vp = *outl->target<ossia::value_port>();
    vp.type = outlet->value().get_type();
    vp.domain = outlet->domain().get();

    auto c = connect(
        outlet, &Process::ControlOutlet::valueChanged, this,
        [this, outl](const ossia::value& v) {
      system().executionQueue.enqueue([outl, val = v]() mutable {
        outl->target<ossia::value_port>()->write_value(std::move(val), 0);
      });
        });
    m_controlConnections.push_back(c);
  }

  typename Node_T::weak_type weak_node = node;
  auto c = con(
      ctx.doc.coarseUpdateTimer, &QTimer::timeout, this,
      [weak_node, firstControlIndex, &proc] {
    if(auto node = weak_node.lock())
    {
      for(std::size_t i = firstControlIndex; i < proc.outlets().size(); i++)
      {
        auto outlet = static_cast<Process::ControlOutlet*>(proc.outlets()[i]);
        outlet->setExecutionValue(*node->displays[i - firstControlIndex].second);
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
  auto node = ossia::make_node<faust_type>(*ctx.execState, proc.faust_poly_object);
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
  auto& proc = process();
  auto& ctx = system();
  proc.faust_object->init(ctx.execState->sampleRate);

  auto setup = [&](auto& node) {
    this->node = node;
    if(!m_ossia_process)
      m_ossia_process = std::make_shared<ossia::node_process>(node);
    else
      ctx.setup.replace_node(m_ossia_process, node, transaction);
    setupExecutionControls(node, 1);
    setupExecutionControlOutlets(node, 1);
  };

  if(proc.faust_object->getNumInputs() <= 1 && proc.faust_object->getNumOutputs() == 1)
  {
    using faust_type = ossia::nodes::faust_mono_fx;
    auto node = ossia::make_node<faust_type>(*ctx.execState, proc.faust_object);
    setup(node);
  }
  else
  {
    using faust_type = ossia::nodes::faust_fx;
    auto node = ossia::make_node<faust_type>(*ctx.execState, proc.faust_object);
    setup(node);
  }
}

}
W_OBJECT_IMPL(Execution::FaustEffectComponent)
