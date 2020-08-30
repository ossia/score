#if defined(HAS_FAUST)
#include "FaustEffectModel.hpp"

#include <Media/Commands/EditFaustEffect.hpp>
#include <Media/Effect/Faust/FaustUtils.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/std/String.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/nodes/faust/faust_node.hpp>

#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Media::Faust::FaustEffectModel)
std::list<::GUI*> GUI::fGuiList;
namespace Process
{

template <>
QString EffectProcessFactory_T<Media::Faust::FaustEffectModel>::customConstructionData() const
{
  return "process = _;";
}

template <>
Process::Descriptor
EffectProcessFactory_T<Media::Faust::FaustEffectModel>::descriptor(QString d) const
{
  Process::Descriptor desc;
  return desc;
}
}
namespace Media::Faust
{

FaustEffectModel::FaustEffectModel(
    TimeVal t,
    const QString& faustProgram,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{t, id, "Faust", parent}
{
  init();
  /*
  setText(R"__(
          import("stdfaust.lib");

          phasor(f)   = f/ma.SR : (+,1.0:fmod) ~ _ ;
          osc(f)      = phasor(f) * 6.28318530718 : sin;
          process     = osc(hslider("freq", 440, 20, 20000, 1)) *
  hslider("level", 0, 0, 1, 0.01);
          )__");
  */
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

ossia::flat_set<llvm_dsp_factory*> dsp_factories;
ossia::flat_set<dsp_poly_factory*> dsp_poly_factories;
void FaustEffectModel::reloadFx(llvm_dsp_factory* fac, llvm_dsp* obj)
{
  dsp_factories.insert(fac);
  if (faust_factory && faust_object)
  {
    // updating an existing DSP

    delete faust_poly_object; // TODO not thread-safe wrt exec thread
    faust_poly_object = nullptr;
    delete faust_object; // TODO not thread-safe wrt exec thread
    faust_object = nullptr;

    faust_factory = fac;
    faust_object = obj;
    // Try to reuse controls
    Faust::UpdateUI<decltype(*this), true> ui{*this};
    faust_object->buildUserInterface(&ui);

    for (std::size_t i = ui.i; i < m_inlets.size(); i++)
    {
      controlRemoved(*m_inlets[i]);
      delete m_inlets[i];
    }
    m_inlets.resize(ui.i);
  }
  else if (!m_inlets.empty() && !m_outlets.empty() && !faust_factory && !faust_object)
  {
    // loading
    faust_factory = fac;
    faust_object = obj;
    // Try to reuse controls
    Faust::UpdateUI<decltype(*this), false> ui{*this};
    faust_object->buildUserInterface(&ui);
  }
  else
  {
    // creating a new dsp
    faust_factory = fac;
    faust_object = obj;
    for (std::size_t i = 1; i < m_inlets.size(); i++)
    {
      controlRemoved(*m_inlets[i]);
    }
    qDeleteAll(m_inlets);
    qDeleteAll(m_outlets);
    m_inlets.clear();
    m_outlets.clear();

    m_inlets.push_back(new Process::AudioInlet{getStrongId(m_inlets), this});
    auto out = new Process::AudioOutlet{getStrongId(m_outlets), this};
    out->setPropagate(true);
    m_outlets.push_back(out);

    Faust::UI<decltype(*this)> ui{*this};
    faust_object->buildUserInterface(&ui);
  }
}

void FaustEffectModel::reloadMidi(dsp_poly_factory* fac, dsp_poly* obj)
{
  dsp_poly_factories.insert(fac);
  if (faust_factory && faust_object)
  {
    // updating an existing DSP

    delete faust_poly_object; // TODO not thread-safe wrt exec thread
    faust_poly_object = nullptr;
    delete faust_object; // TODO not thread-safe wrt exec thread
    faust_object = nullptr;

    faust_poly_factory = fac;
    faust_poly_object = obj;
    // Try to reuse controls
    Faust::UpdateUI<decltype(*this), true> ui{*this};
    faust_poly_object->buildUserInterface(&ui);

    for (std::size_t i = ui.i; i < m_inlets.size(); i++)
    {
      controlRemoved(*m_inlets[i]);
      delete m_inlets[i];
    }
    m_inlets.resize(ui.i);
  }
  else if (!m_inlets.empty() && !m_outlets.empty() && !faust_poly_factory && !faust_poly_object)
  {
    // loading
    faust_poly_factory = fac;
    faust_poly_object = obj;
    // Try to reuse controls
    Faust::UpdateUI<decltype(*this), false> ui{*this};
    faust_poly_object->buildUserInterface(&ui);
  }
  else
  {
    // creating a new dsp
    faust_poly_factory = fac;
    faust_poly_object = obj;
    for (std::size_t i = 1; i < m_inlets.size(); i++)
    {
      controlRemoved(*m_inlets[i]);
    }
    qDeleteAll(m_inlets);
    qDeleteAll(m_outlets);
    m_inlets.clear();
    m_outlets.clear();

    m_inlets.push_back(new Process::MidiInlet{getStrongId(m_inlets), this});

    auto out = new Process::AudioOutlet{getStrongId(m_outlets), this};
    out->setPropagate(true);
    m_outlets.push_back(out);

    Faust::UI<decltype(*this)> ui{*this};
    faust_poly_object->buildUserInterface(&ui);
  }
}
void FaustEffectModel::reload()
{
  auto fx_text = m_text.toLocal8Bit();
  if (fx_text.isEmpty())
  {
    return;
  }

  const char* triple =
#if defined(_MSC_VER)
      "x86_64-pc-windows-msvc"
#else
      ""
#endif
      ;
  auto str = fx_text.toStdString();
  int argc = 0;
  const char* argv[1]{};

  std::string err;
  err.resize(4097);
  auto fac = createDSPFactoryFromString("score", str, argc, argv, triple, err, -1);
  dsp_factories.insert(fac);

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
    auto fac = createPolyDSPFactoryFromString("score", str, argc, argv, triple, err, -1);
    dsp_poly_factories.insert(fac);

    auto obj = fac->createPolyDSPInstance(64, false, true);
    reloadMidi(fac, obj);
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
void DataStreamReader::read(const Media::Faust::FaustEffectModel& eff)
{
  m_stream << eff.m_text;
  readPorts(*this, eff.m_inlets, eff.m_outlets);
}

template <>
void DataStreamWriter::write(Media::Faust::FaustEffectModel& eff)
{
  m_stream >> eff.m_text;
  eff.reload();
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets, eff.m_outlets, &eff);
}

template <>
void JSONReader::read(const Media::Faust::FaustEffectModel& eff)
{
  obj["Text"] = eff.text();
  readPorts(*this, eff.m_inlets, eff.m_outlets);
}

template <>
void JSONWriter::write(Media::Faust::FaustEffectModel& eff)
{
  eff.m_text = obj["Text"].toString();
  eff.reload();
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets, eff.m_outlets, &eff);
}

namespace Execution
{

FaustEffectComponent::FaustEffectComponent(
    Media::Faust::FaustEffectModel& proc,
    const Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : ProcessComponent_T{proc, ctx, id, "FaustComponent", parent}
{
  auto do_reload = [this] {
    auto& proc = process();
    if (proc.faust_object)
    {
      reloadFx();
    }
    else if (proc.faust_poly_object)
    {
      reloadSynth();
    }
  };
  do_reload();
  connect(&proc, &Media::Faust::FaustEffectModel::changed, this, [=] {
    auto& ctx = system();
    Execution::SetupContext& setup = ctx.setup;
    auto old_node = this->node;

    if (old_node)
    {
      setup.unregister_node(process(), this->node);
    }
    do_reload();
    if (this->node)
    {
      setup.register_node(process(), this->node);
      Execution::Transaction commands{ctx};
      nodeChanged(old_node, this->node, commands);
      commands.run_all();
    }
  });
}

void FaustEffectComponent::reloadSynth()
{
  using faust_type = ossia::nodes::faust_synth;
  auto& proc = process();
  auto& ctx = system();
  proc.faust_poly_object->init(ctx.execState->sampleRate);
  auto node = std::make_shared<faust_type>(proc.faust_poly_object);
  this->node = node;
  m_ossia_process = std::make_shared<ossia::node_process>(node);
  for (std::size_t i = 1; i < proc.inlets().size(); i++)
  {
    auto inlet = static_cast<Process::ControlInlet*>(proc.inlets()[i]);
    *node->controls[i - 1].second = ossia::convert<float>(inlet->value());
    auto inl = this->node->root_inputs()[i];
    connect(inlet, &Process::ControlInlet::valueChanged, this, [this, inl](const ossia::value& v) {
      system().executionQueue.enqueue([inl, val = v]() mutable {
        inl->target<ossia::value_port>()->write_value(std::move(val), 0);
      });
    });
  }

  std::weak_ptr<faust_type> weak_node = node;
  con(ctx.doc.coarseUpdateTimer, &QTimer::timeout, this, [weak_node, &proc] {
    if (auto node = weak_node.lock())
    {
      for (std::size_t i = 1; i < proc.inlets().size(); i++)
      {
        auto inlet = static_cast<Process::ControlInlet*>(proc.inlets()[i]);
        inlet->setValue(*node->controls[i - 1].second);
      }
    }
  });
}
void FaustEffectComponent::reloadFx()
{
  using faust_type = ossia::nodes::faust_fx;
  auto& proc = process();
  auto& ctx = system();
  proc.faust_object->init(ctx.execState->sampleRate);
  auto node = std::make_shared<faust_type>(proc.faust_object);
  this->node = node;
  m_ossia_process = std::make_shared<ossia::node_process>(node);
  for (std::size_t i = 1; i < proc.inlets().size(); i++)
  {
    auto inlet = static_cast<Process::ControlInlet*>(proc.inlets()[i]);
    *node->controls[i - 1].second = ossia::convert<float>(inlet->value());
    auto inl = this->node->root_inputs()[i];
    connect(inlet, &Process::ControlInlet::valueChanged, this, [this, inl](const ossia::value& v) {
      system().executionQueue.enqueue([inl, val = v]() mutable {
        inl->target<ossia::value_port>()->write_value(std::move(val), 0);
      });
    });
  }

  std::weak_ptr<faust_type> weak_node = node;
  con(ctx.doc.coarseUpdateTimer, &QTimer::timeout, this, [weak_node, &proc] {
    if (auto node = weak_node.lock())
    {
      for (std::size_t i = 1; i < proc.inlets().size(); i++)
      {
        auto inlet = static_cast<Process::ControlInlet*>(proc.inlets()[i]);
        inlet->setValue(*node->controls[i - 1].second);
      }
    }
  });
}
}
W_OBJECT_IMPL(Execution::FaustEffectComponent)
#endif
