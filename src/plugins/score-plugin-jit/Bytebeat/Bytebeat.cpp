
#include "Bytebeat.hpp"

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

#include <iostream>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Jit::BytebeatModel)
namespace Jit
{
QString generateBytebeatFunction(const QString& bb)
{
  return QString(R"_(
extern "C"
void score_bytebeat(double* input, int size, int T)
{
  for(auto end = input + size; input < end; ++input, ++T)
  {
    const int t = T / 4;
    signed char expr = (%1);
    *(input) = double(expr) / double(128.);
  }
}
)_").arg(bb);
}

BytebeatModel::BytebeatModel(
    TimeVal t,
    const QString& jitProgram,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{t, id, "Jit", parent}
{
  auto audio_out = new Process::AudioOutlet{Id<Process::Port>{0}, this};
  audio_out->setPropagate(true);
  this->m_outlets.push_back(audio_out);
  init();
  if(jitProgram.isEmpty())
    setScript(Process::EffectProcessFactory_T<Jit::BytebeatModel>{}.customConstructionData());
  else
    setScript(jitProgram);
}

BytebeatModel::~BytebeatModel() {}

BytebeatModel::BytebeatModel(JSONObject::Deserializer& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

BytebeatModel::BytebeatModel(DataStream::Deserializer& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

BytebeatModel::BytebeatModel(JSONObject::Deserializer&& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

BytebeatModel::BytebeatModel(DataStream::Deserializer&& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

void BytebeatModel::setScript(const QString& txt)
{
  if(m_text != txt)
  {
    m_text = txt;
    reload();
    scriptChanged(txt);
  }
}

bool BytebeatModel::validate(const QString& txt) const noexcept
{
  SCORE_TODO;
  return true;
}

void BytebeatModel::init() {}

QString BytebeatModel::prettyName() const noexcept
{
  return "Bytebeat";
}

void BytebeatModel::reload()
{
  // FIXME dispos of them once unused at execution
  static std::list<std::shared_ptr<BytebeatCompiler>> old_compilers;
  if (m_compiler)
  {
    old_compilers.push_front(std::move(m_compiler));
    if (old_compilers.size() > 5)
      old_compilers.pop_back();
  }

  m_compiler = std::make_unique<BytebeatCompiler>("score_bytebeat");
  auto fx_text = Jit::generateBytebeatFunction(m_text).toLocal8Bit();
  BytebeatFactory jit_factory;
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

class bytebeat_node
    : public ossia::nonowning_graph_node
{
public:
  bytebeat_node()
  {
    m_outlets.push_back(&audio_out);
  }

  void set_function(BytebeatFunction* func)
  {
    this->func = func;
  }
  void run(const ossia::token_request& t, ossia::exec_state_facade f) noexcept override
  {
      ossia::audio_port& o = *audio_out;
      o.samples.resize(2);
      o.samples[0].resize(f.bufferSize());
      double* data = o.samples[0].data();
      int N = f.bufferSize();

      if(func)
      {
        func(data, N, time);
      }
      time += N;
      o.samples[1] = o.samples[0];
  }

  int time = 0;
  BytebeatFunction* func = nullptr;
  ossia::audio_outlet audio_out;
};


BytebeatExecutor::BytebeatExecutor(
    Jit::BytebeatModel& proc,
    const Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : ProcessComponent_T{proc, ctx, id, "JitComponent", parent}
{
  auto bb = new bytebeat_node;
  this->node.reset(bb);

  if(auto tgt = proc.factory.target<void(*)(double* , int , int )>())
    bb->set_function(*tgt);

  m_ossia_process = std::make_shared<ossia::node_process>(node);

  con(proc, &Jit::BytebeatModel::changed,
      this, [this, &proc, bb] {
        if(auto tgt = proc.factory.target<void(*)(double* , int , int )>())
        {
          in_exec([tgt, bb] {
            bb->set_function(*tgt);
          });
        }
  });

}

BytebeatExecutor::~BytebeatExecutor() {}


}

template <>
void DataStreamReader::read(const Jit::BytebeatModel& eff)
{
  m_stream << eff.m_text;
  readPorts(*this, eff.m_inlets, eff.m_outlets);
}

template <>
void DataStreamWriter::write(Jit::BytebeatModel& eff)
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
void JSONReader::read(const Jit::BytebeatModel& eff)
{
  obj["Text"] = eff.script();
  readPorts(*this, eff.m_inlets, eff.m_outlets);
}

template <>
void JSONWriter::write(Jit::BytebeatModel& eff)
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
EffectProcessFactory_T<Jit::BytebeatModel>::customConstructionData() const
{
  return "(((t<<1)^((t<<1)+(t>>7)&t>>12))|t>>(4-(1^7&(t>>19)))|t>>7)";
}

template <>
Process::Descriptor
EffectProcessFactory_T<Jit::BytebeatModel>::descriptor(QString d) const
{
  return Metadata<Descriptor_k, Jit::BytebeatModel>::get();
}

}

