
#include "Expr2.hpp"

#include <Process/Dataflow/PortFactory.hpp>
#include <Process/PresetHelpers.hpp>

#include <JitCpp/Compiler/Driver.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/dataflow/port.hpp>

#include <QPlainTextEdit>
#include <QSyntaxStyle>
#include <QVBoxLayout>

#include <wobjectimpl.h>

#include <iostream>

W_OBJECT_IMPL(Jit::Expr2Model)
struct expr_factory
{
  using alloc_f = void*();
  using dealloc_f = void(void*);
  using process_f = void(void*);

  alloc_f* allocate;
  dealloc_f* deallocate;
  process_f* process;
};
extern "C" expr_factory* score_expr2();

namespace Jit
{
/* Example: 
 * Expression:
 *   [[input]] float x;
 *   [[output]] float o; 
 *   
 *   [[control]] float a;
 *   [[control]] int b;
 *   
 *   o = x * a + b;
 *   
 *   ->
 *   
 *   struct expr {
 *     float x; float o; float a; int b;   
 *     void operator()() {
 *       o = x * a + b;
 *     }
 *   };
 *   
 *   expr* allocate_expr2() {
 *     return new expr;
 *   }
 *   expr* deallocate_expr2(expr* e) {
 *     delete e;
 *   }
 *   void process_expr2(expr* e) {
 *     (*e)();
 *   }
 *   
 */
QString generateExpr2Function(QString bb)
{
  QString computed = "(";
  static const QRegularExpression cpp_comm_1(
      "/\\*(.*?)\\*/", QRegularExpression::DotMatchesEverythingOption);
  static const QRegularExpression cpp_comm_2("//.*\n");
  bb.remove(cpp_comm_1);
  bb.remove(cpp_comm_2);

  auto res = QStringLiteral(R"_(
#if __has_include(<cmath>)
#include <cmath>
#endif
struct expr {
  float x; float o;
  void operator()() { o = 2 * x; }
};

extern "C" {
expr* allocate_expr2() {
  return new expr;
}
expr* deallocate_expr2(expr* e) {
  delete e;
}
void process_expr2(expr* e) {
  (*e)();
}
struct expr_factory
{
  using alloc_f = void*();
  using dealloc_f = void(void*);
  using process_f = void(void*);

  alloc_f* allocate = (alloc_f*)allocate_expr2;
  dealloc_f* deallocate = (dealloc_f*) deallocate_expr2;
  process_f* process = (process_f*) process_expr2;
};
expr_factory* score_expr2() {
  static expr_factory e;
  return &e;
}
}
)_")
                 .arg(computed);
  return res;
}

Expr2Model::Expr2Model(
    TimeVal t, const QString& jitProgram, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{t, id, "Jit", parent}
{
  auto audio_out = new Process::AudioOutlet{Id<Process::Port>{0}, this};
  audio_out->setPropagate(true);
  this->m_outlets.push_back(audio_out);
  init();
  if(jitProgram.isEmpty())
    setScript(
        Process::EffectProcessFactory_T<Jit::Expr2Model>{}.customConstructionData());
  else
    setScript(jitProgram);

  metadata().setInstanceName(*this);
}

Expr2Model::~Expr2Model() { }

Expr2Model::Expr2Model(JSONObject::Deserializer& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

Expr2Model::Expr2Model(DataStream::Deserializer& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

Expr2Model::Expr2Model(JSONObject::Deserializer&& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

Expr2Model::Expr2Model(DataStream::Deserializer&& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

Process::ScriptChangeResult Expr2Model::setScript(const QString& txt)
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

bool Expr2Model::validate(const QString& txt) const noexcept
{
  SCORE_TODO;
  return true;
}

void Expr2Model::init() { }

QString Expr2Model::prettyName() const noexcept
{
  return "Expr2";
}

Process::ScriptChangeResult Expr2Model::reload()
{
  Process::ScriptChangeResult res;
  // FIXME dispos of them once unused at execution
  static std::list<std::shared_ptr<Expr2Compiler>> old_compilers;
  if(m_compiler)
  {
    old_compilers.push_front(std::move(m_compiler));
    if(old_compilers.size() > 5)
      old_compilers.pop_back();
  }

  m_compiler = std::make_unique<Expr2Compiler>("score_expr2");
  auto fx_text = Jit::generateExpr2Function(m_text).toLocal8Bit();
  Expr2Factory jit_factory;
  if(fx_text.isEmpty())
    return res;

  try
  {
    jit_factory = (*m_compiler)
                      .operator()<Expr2Function>(
                          fx_text.toStdString(), {}, CompilerOptions{true});
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
  {

    auto fun = factory.target<void* (*)()>();
    if(fun)
    {
      auto obj = reinterpret_cast<decltype(&score_expr2)>(*fun)();
      auto ptr = (float*)obj->allocate();
      ptr[0] = 13.f;
      ptr[1] = 0.f;
      obj->process(ptr);

      qDebug() << ptr[1];
    }
    else
    {
      qDebug("oh no!");
    }

    std::terminate();
  }
  return res;
}

QString Expr2Model::effect() const noexcept
{
  return m_text;
}

void Expr2Model::loadPreset(const Process::Preset& preset)
{
  Process::loadScriptProcessPreset<Expr2Model::p_script>(*this, preset);
}

Process::Preset Expr2Model::savePreset() const noexcept
{
  return Process::saveScriptProcessPreset(*this, this->m_text);
}

class Expr2_node : public ossia::nonowning_graph_node
{
public:
  Expr2_node() { m_outlets.push_back(&audio_out); }

  [[nodiscard]] std::string label() const noexcept override { return "Expr2"; }
  void set_function(Expr2Function* func) { this->func = func; }
  void run(const ossia::token_request& t, ossia::exec_state_facade f) noexcept override
  {
    ossia::audio_port& o = *audio_out;
    o.set_channels(2);
    o.channel(0).resize(f.bufferSize());
    double* data = o.channel(0).data();
    int N = f.bufferSize();

    if(func)
    {
      //   func(data, N, time);
    }
    time += N;
    o.channel(1) = o.channel(0);
  }

  int time = 0;
  Expr2Function* func = nullptr;
  ossia::audio_outlet audio_out;
};

Expr2Executor::Expr2Executor(
    Jit::Expr2Model& proc, const Execution::Context& ctx, QObject* parent)
    : ProcessComponent_T{proc, ctx, "JitComponent", parent}
{
  auto bb = ossia::make_node<Expr2_node>(*ctx.execState);
  this->node = bb;

  auto fun = proc.factory.target<void* (*)()>();
  if(fun)
  {
    auto obj = reinterpret_cast<decltype(&score_expr2)>(*fun)();
    auto ptr = (float*)obj->allocate();
    ptr[0] = 13.f;
    ptr[1] = 0.f;
    obj->process(ptr);

    qDebug() << ptr[1];
  }
  else
  {
    qDebug("oh no!");
  }

  std::terminate();
  // if(auto tgt = proc.factory.target<void (*)(double*, int, int)>())
  //   bb->set_function(*tgt);

  m_ossia_process = std::make_shared<ossia::node_process>(node);

  // con(proc, &Jit::Expr2Model::programChanged, this, [this, &proc, bb] {
  //   if(auto tgt = proc.factory.target<void (*)(double*, int, int)>())
  //   {
  //     in_exec([tgt, bb] { bb->set_function(*tgt); });
  //   }
  // });
}

Expr2Executor::~Expr2Executor() { }
}

template <>
void DataStreamReader::read(const Jit::Expr2Model& eff)
{
  m_stream << eff.m_text;
  readPorts(*this, eff.m_inlets, eff.m_outlets);
}

template <>
void DataStreamWriter::write(Jit::Expr2Model& eff)
{
  m_stream >> eff.m_text;
  eff.reload();
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets,
      eff.m_outlets, &eff);
}

template <>
void JSONReader::read(const Jit::Expr2Model& eff)
{
  obj["Text"] = eff.script();
  readPorts(*this, eff.m_inlets, eff.m_outlets);
}

template <>
void JSONWriter::write(Jit::Expr2Model& eff)
{
  eff.m_text = obj["Text"].toString();
  eff.reload();
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets,
      eff.m_outlets, &eff);
}

namespace Process
{

template <>
QString EffectProcessFactory_T<Jit::Expr2Model>::customConstructionData() const noexcept
{
  return R"_(
/* one per line */
(((t<<1)^((t<<1)+(t>>7)&t>>12))|t>>(4-(1^7&(t>>19)))|t>>7)
t*(t>>10&((t>>16)+1))
)_";
}

template <>
Process::Descriptor
EffectProcessFactory_T<Jit::Expr2Model>::descriptor(QString d) const noexcept
{
  return Metadata<Descriptor_k, Jit::Expr2Model>::get();
}

template <>
Process::Descriptor EffectProcessFactory_T<Jit::Expr2Model>::descriptor(
    const Process::ProcessModel& d) const noexcept
{
  return descriptor(d.effect());
}
}
