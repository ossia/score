
#include "Texgen.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSyntaxStyle>
#include <QVBoxLayout>

#include <JitCpp/Compiler/Driver.hpp>
#include <JitCpp/EditScript.hpp>

#include <Process/Dataflow/PortFactory.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/port.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExec.hpp>
#include <Gfx/TexturePort.hpp>
#include <Gfx/Graph/texgennode.hpp>

#include <iostream>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Jit::TexgenModel)
namespace Jit
{
TexgenModel::TexgenModel(
    TimeVal t,
    const QString& jitProgram,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{t, id, "Jit", parent}
{
  auto audio_out = new Gfx::TextureOutlet{Id<Process::Port>{0}, this};
  this->m_outlets.push_back(audio_out);
  init();
  if(jitProgram.isEmpty())
    setScript(Process::EffectProcessFactory_T<Jit::TexgenModel>{}.customConstructionData());
  else
    setScript(jitProgram);
}

TexgenModel::~TexgenModel() {}

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

void TexgenModel::setScript(const QString& txt)
{
  if(m_text != txt)
  {
    m_text = txt;
    reload();
    scriptChanged(txt);
  }
}

void TexgenModel::init() {}

QString TexgenModel::prettyName() const noexcept
{
  return "Texgen";
}

bool TexgenModel::validate(const QString& txt) const noexcept
{
  SCORE_TODO;
  return true;
}

void TexgenModel::reload()
{
  // FIXME dispos of them once unused at execution
  static std::list<std::shared_ptr<TexgenCompiler>> old_compilers;
  if (m_compiler)
  {
    old_compilers.push_front(std::move(m_compiler));
    if (old_compilers.size() > 5)
      old_compilers.pop_back();
  }

  m_compiler = std::make_unique<TexgenCompiler>("score_rgba");
  auto fx_text = m_text.toLocal8Bit();
  TexgenFactory jit_factory;
  if (fx_text.isEmpty())
    return;

  try
  {
    jit_factory = (*m_compiler)(fx_text.toStdString(), {}, CompilerOptions{true});
    assert(jit_factory);

    if (!jit_factory)
      return;
  }
  catch (const std::exception& e)
  {
    errorMessage(0, e.what());
    return;
  }
  catch (...)
  {
    errorMessage(0, "JIT error");
    return;
  }

  factory = std::move(jit_factory);
  changed();
}


class texgen_node final : public Gfx::gfx_exec_node
{
public:
  TexgenNode* gfxNode{};
  texgen_node(Gfx::GfxExecutionAction& ctx)
      : gfx_exec_node{ctx}
  {
    root_outputs().push_back(new ossia::texture_outlet);

    auto n = std::make_unique<TexgenNode>();
    gfxNode = n.get();
    id = exec_context->ui->register_node(std::move(n));
  }

  void set_function(TexgenFunction* func)
  {
    gfxNode->function = func;
  }

  ~texgen_node()
  {
    if (id >= 0)
      exec_context->ui->unregister_node(id);
  }

  std::string label() const noexcept override { return "texgen_node"; }
};

TexgenExecutor::TexgenExecutor(
    Jit::TexgenModel& proc,
    const Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : ProcessComponent_T{proc, ctx, id, "JitComponent", parent}
{
  auto bb = new texgen_node{ctx.doc.plugin<Gfx::DocumentPlugin>().exec};
  this->node.reset(bb);

  if(auto tgt = proc.factory.target<void(*)(unsigned char* rgb, int width, int height, int t)>())
    bb->set_function(*tgt);

  m_ossia_process = std::make_shared<ossia::node_process>(node);

  con(proc, &Jit::TexgenModel::changed,
      this, [this, &proc, bb] {
        if(auto tgt = proc.factory.target<void(*)(unsigned char* rgb, int width, int height, int t)>())
        {
          in_exec([tgt, bb] {
            bb->set_function(*tgt);
          });
        }
  });

}

TexgenExecutor::~TexgenExecutor() {}


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
  eff.reload();

  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      eff.m_inlets,
      eff.m_outlets,
      &eff);
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
  eff.reload();
  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      eff.m_inlets,
      eff.m_outlets,
      &eff);
}

namespace Process
{

template <>
QString
EffectProcessFactory_T<Jit::TexgenModel>::customConstructionData() const
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
EffectProcessFactory_T<Jit::TexgenModel>::descriptor(QString d) const
{
  return Metadata<Descriptor_k, Jit::TexgenModel>::get();
}

}

