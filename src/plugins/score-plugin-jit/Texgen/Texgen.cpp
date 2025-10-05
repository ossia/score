
#include "Texgen.hpp"

#include <Process/Dataflow/PortFactory.hpp>
#include <Process/PresetHelpers.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <Gfx/Graph/TexgenNode.hpp>
#include <Gfx/TexturePort.hpp>
#include <JitCpp/Compiler/Driver.hpp>
#include <JitCpp/EditScript.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/port.hpp>

#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSyntaxStyle>
#include <QVBoxLayout>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Jit::TexgenModel)
namespace Jit
{
TexgenModel::TexgenModel(
    TimeVal t, const QString& jitProgram, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{t, id, "Jit", parent}
{
  metadata().setName("Texgen");
  metadata().setLabel(metadata().getName());
  auto audio_out = new Gfx::TextureOutlet{"Texture Out", Id<Process::Port>{0}, this};
  this->m_outlets.push_back(audio_out);
  init();
  if(jitProgram.isEmpty())
    (void)setScript(
        Process::EffectProcessFactory_T<Jit::TexgenModel>{}.customConstructionData());
  else
    (void)setScript(jitProgram);
}

TexgenModel::~TexgenModel() { }

TexgenModel::TexgenModel(JSONObject::Deserializer& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

TexgenModel::TexgenModel(DataStream::Deserializer& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

TexgenModel::TexgenModel(JSONObject::Deserializer&& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

TexgenModel::TexgenModel(DataStream::Deserializer&& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

Process::ScriptChangeResult TexgenModel::setScript(const QString& txt)
{
  if(m_text != txt)
  {
    m_text = txt;
    auto res = reload();
    scriptChanged(txt);
    return res;
  }
  return {};
}

void TexgenModel::init() { }

QString TexgenModel::prettyName() const noexcept
{
  return "Texgen";
}

bool TexgenModel::validate(const QString& txt) const noexcept
{
  SCORE_TODO;
  return true;
}

Process::ScriptChangeResult TexgenModel::reload()
{
  Process::ScriptChangeResult res;
  // FIXME dispos of them once unused at execution
  static std::list<std::shared_ptr<TexgenCompiler>> old_compilers;
  if(m_compiler)
  {
    old_compilers.push_front(std::move(m_compiler));
    if(old_compilers.size() > 5)
      old_compilers.pop_back();
  }

  m_compiler = std::make_unique<TexgenCompiler>("score_rgba");
  auto fx_text = m_text.toLocal8Bit();
  TexgenFactory jit_factory;
  if(fx_text.isEmpty())
    return res;

  try
  {
    jit_factory = (*m_compiler).operator()<TexgenFunction>(fx_text.toStdString(), {}, CompilerOptions{true});
    assert(jit_factory);

    if(!jit_factory)
      return res;
  }
  catch(const std::exception& e)
  {
    errorMessage(0, e.what());
    return res;
  }
  catch(...)
  {
    errorMessage(0, "JIT error");
    return res;
  }

  factory = std::move(jit_factory);
  res.valid = true;
  return res;
}

QString TexgenModel::effect() const noexcept
{
  return m_text;
}

void TexgenModel::loadPreset(const Process::Preset& preset)
{
  Process::loadScriptProcessPreset<TexgenModel::p_script>(*this, preset);
}

Process::Preset TexgenModel::savePreset() const noexcept
{
  return Process::saveScriptProcessPreset(*this, this->m_text);
}

class texgen_node final : public Gfx::gfx_exec_node
{
public:
  score::gfx::TexgenNode* gfxNode{};
  texgen_node(Gfx::GfxExecutionAction& ctx)
      : gfx_exec_node{ctx}
  {
    root_outputs().push_back(new ossia::texture_outlet);

    auto n = std::make_unique<score::gfx::TexgenNode>();
    gfxNode = n.get();
    id = exec_context->ui->register_node(std::move(n));
  }

  void set_function(TexgenFunction* func) { gfxNode->function = func; }

  ~texgen_node()
  {
    if(id >= 0)
      exec_context->ui->unregister_node(id);
  }

  std::string label() const noexcept override { return "texgen_node"; }
};

TexgenExecutor::TexgenExecutor(
    Jit::TexgenModel& proc, const Execution::Context& ctx, QObject* parent)
    : ProcessComponent_T{proc, ctx, "JitComponent", parent}
{
  auto bb = ossia::make_node<texgen_node>(
      *ctx.execState, ctx.doc.plugin<Gfx::DocumentPlugin>().exec);
  this->node = bb;

  if(auto tgt
     = proc.factory.target<void (*)(unsigned char* rgb, int width, int height, int t)>())
    bb->set_function(*tgt);

  m_ossia_process = std::make_shared<ossia::node_process>(node);

  con(proc, &Jit::TexgenModel::programChanged, this, [this, &proc, bb] {
    if(auto tgt
       = proc.factory
             .target<void (*)(unsigned char* rgb, int width, int height, int t)>())
    {
      in_exec([tgt, bb] { bb->set_function(*tgt); });
    }
  });
}

TexgenExecutor::~TexgenExecutor() { }

}

template <>
void DataStreamReader::read(const Jit::TexgenModel& eff)
{
  m_stream << eff.m_text;
  readPorts(*this, eff.m_inlets, eff.m_outlets);
}

template <>
void DataStreamWriter::write(Jit::TexgenModel& eff)
{
  m_stream >> eff.m_text;
  (void)eff.reload();

  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets,
      eff.m_outlets, &eff);
}

template <>
void JSONReader::read(const Jit::TexgenModel& eff)
{
  obj["Text"] = eff.script();
  readPorts(*this, eff.m_inlets, eff.m_outlets);
}

template <>
void JSONWriter::write(Jit::TexgenModel& eff)
{
  eff.m_text = obj["Text"].toString();
  (void)eff.reload();
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets,
      eff.m_outlets, &eff);
}

namespace Process
{

template <>
QString EffectProcessFactory_T<Jit::TexgenModel>::customConstructionData() const noexcept
{
  return R"_(extern "C"
void score_rgba(unsigned char* rgba, int width, int height, int t)
{
  int k = 0;
  for(int j = 0; j < height; j++)
  {
    for(int i = 0; i < width; i++)
    {
      rgba[k++] = 255 * t * k / (width * height);
      rgba[k++] = 255 * t * k / (width * height);
      rgba[k++] = 255 * t * k / (width * height);
      rgba[k++] = 255 * t * k / (width * height);
    }
  }
}
)_";
}

template <>
Process::Descriptor
EffectProcessFactory_T<Jit::TexgenModel>::descriptor(QString d) const noexcept
{
  return Metadata<Descriptor_k, Jit::TexgenModel>::get();
}

template <>
Process::Descriptor EffectProcessFactory_T<Jit::TexgenModel>::descriptor(
    const Process::ProcessModel& d) const noexcept
{
  return descriptor(d.effect());
}
}
