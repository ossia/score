#include "JitModel.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <JitCpp/Compiler/Driver.hpp>
#include <JitCpp/EditScript.hpp>
//#include <JitCpp/Commands/EditJitEffect.hpp>

#include <Process/Dataflow/PortFactory.hpp>

#include <score/tools/DeleteAll.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/port.hpp>

#include <iostream>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Jit::JitEffectModel)

namespace Jit
{

JitEffectModel::JitEffectModel(
    TimeVal t,
    const QString& jitProgram,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{t, id, "Jit", parent}
{
  init();
  if(jitProgram.isEmpty())
    setScript(Process::EffectProcessFactory_T<Jit::JitEffectModel>{}.customConstructionData());
  else
    setScript(jitProgram);
}

JitEffectModel::~JitEffectModel() {}

JitEffectModel::JitEffectModel(JSONObject::Deserializer& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

JitEffectModel::JitEffectModel(DataStream::Deserializer& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

JitEffectModel::JitEffectModel(JSONObject::Deserializer&& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

JitEffectModel::JitEffectModel(DataStream::Deserializer&& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

bool JitEffectModel::validate(const QString& txt) const noexcept
{
  SCORE_TODO;
  return true;
}

void JitEffectModel::setScript(const QString& txt)
{
  if(m_text != txt)
  {
    m_text = txt;
    reload();
    scriptChanged(txt);
  }
}

void JitEffectModel::init() {}

QString JitEffectModel::prettyName() const noexcept
{
  return "Jit";
}

struct inlet_vis
{
  JitEffectModel& self;
  Process::Inlet* operator()(const ossia::audio_port& p) const noexcept
  {
    auto i = new Process::AudioInlet{getStrongId(self.inlets()), &self};
    return i;
  }

  Process::Inlet* operator()(const ossia::midi_port& p) const noexcept
  {
    auto i = new Process::MidiInlet{getStrongId(self.inlets()), &self};
    return i;
  }

  Process::Inlet* operator()(const ossia::value_port& p) const noexcept
  {
    if(!p.is_event)
    {
      auto i = new Process::ValueInlet{getStrongId(self.inlets()), &self};
      return i;
    }
    else
    {
      auto i = new Process::ControlInlet{getStrongId(self.inlets()), &self};
      i->setDomain(State::Domain{p.domain});
      return i;
    }
  }

  Process::Inlet* operator()() const noexcept { return nullptr; }
};

struct outlet_vis
{
  JitEffectModel& self;
  Process::Outlet* operator()(const ossia::audio_port& p) const noexcept
  {
    auto i = new Process::AudioOutlet{getStrongId(self.outlets()), &self};
    return i;
  }

  Process::Outlet* operator()(const ossia::midi_port& p) const noexcept
  {
    auto i = new Process::MidiOutlet{getStrongId(self.outlets()), &self};
    return i;
  }

  Process::Outlet* operator()(const ossia::value_port& p) const noexcept
  {
    auto i = new Process::ValueOutlet{getStrongId(self.outlets()), &self};
    return i;
  }
  Process::Outlet* operator()() const noexcept { return nullptr; }
};

void JitEffectModel::reload()
{
  // FIXME dispos of them once unused at execution
  static std::list<std::shared_ptr<NodeCompiler>> old_compilers;
  if (m_compiler)
  {
    old_compilers.push_front(std::move(m_compiler));
    if (old_compilers.size() > 5)
      old_compilers.pop_back();
  }

  m_compiler = std::make_unique<NodeCompiler>("score_graph_node_factory");

  auto fx_text = m_text.toLocal8Bit();
  if (fx_text.isEmpty())
    return;

  NodeFactory jit_factory;
  try
  {
    jit_factory = (*m_compiler)(fx_text.toStdString(), {}, CompilerOptions{false});

    if (!jit_factory)
      return;
  }
  catch (const std::exception& e)
  {
    qDebug() << e.what();
    errorMessage(0, e.what());
    return;
  }
  catch (...)
  {
    errorMessage(0, "JIT error");
    return;
  }

  std::unique_ptr<ossia::graph_node> jit_object{jit_factory()};
  if (!jit_object)
  {
    jit_factory = {};
    return;
  }
  // creating a new dsp

  factory = std::move(jit_factory);

  auto inls = score::clearAndDeleteLater(m_inlets);
  auto outls = score::clearAndDeleteLater(m_outlets);


  for (ossia::inlet* port : jit_object->root_inputs())
  {
    if (auto inl = port->visit(inlet_vis{*this}))
    {
      m_inlets.push_back(inl);
    }
  }
  for (ossia::outlet* port : jit_object->root_outputs())
  {
    if (auto inl = port->visit(outlet_vis{*this}))
    {
      m_outlets.push_back(inl);
    }
  }

  if (!m_outlets.empty()
      && m_outlets.front()->type() == Process::PortType::Audio)
    safe_cast<Process::AudioOutlet*>(m_outlets.front())->setPropagate(true);

  inletsChanged();
  outletsChanged();
  changed();
}

}

template <>
void DataStreamReader::read(const Jit::JitEffectModel& eff)
{
  readPorts(*this, eff.m_inlets, eff.m_outlets);
  m_stream << eff.m_text;
}

template <>
void DataStreamWriter::write(Jit::JitEffectModel& eff)
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
void JSONReader::read(const Jit::JitEffectModel& eff)
{
  obj["Text"] = eff.script();
  readPorts(*this, eff.m_inlets, eff.m_outlets);
}

template <>
void JSONWriter::write(Jit::JitEffectModel& eff)
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
EffectProcessFactory_T<Jit::JitEffectModel>::customConstructionData() const
{
  return R"_(
#include <ossia/dataflow/data.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <iostream>
#include <vector>

struct example : ossia::nonowning_graph_node {
  ossia::value_inlet in;
  ossia::value_outlet out;
  example()
  {
    m_inlets.push_back(&in);
    m_outlets.push_back(&out);
  }

  void run(
      const ossia::token_request& t,
      ossia::exec_state_facade st) noexcept override
  {
    for(auto& val : in.data.get_data())
    {
      float new_value = ossia::convert<float>(val.value) * 2.;
      int64_t time = val.timestamp;
      out.data.write_value(new_value, time);
    }
  }
};

extern "C" ossia::graph_node* score_graph_node_factory() {
  return new example;
}
)_";
}

template <>
Process::Descriptor
EffectProcessFactory_T<Jit::JitEffectModel>::descriptor(QString d) const
{
  return Metadata<Descriptor_k, Jit::JitEffectModel>::get();
}

}
namespace Execution
{

Execution::JitEffectComponent::JitEffectComponent(
    Jit::JitEffectModel& proc,
    const Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : ProcessComponent_T{proc, ctx, id, "JitComponent", parent}
{
  auto reset = [this, &proc, &ctx] {

    if (proc.factory)
    {
      this->node.reset(proc.factory());
      if (this->node)
      {
        m_ossia_process = std::make_shared<ossia::node_process>(node);

        for (std::size_t i = 0; i < proc.inlets().size(); i++)
        {
          auto inlet = dynamic_cast<Process::ControlInlet*>(proc.inlets()[i]);
          if(!inlet)
            continue;

          auto inl = node->root_inputs()[i];
          inl->target<ossia::value_port>()->write_value(inlet->value(), {});
          connect(inlet, &Process::ControlInlet::valueChanged,
                  this, [this, inl] (const ossia::value& v) {

            system().executionQueue.enqueue([inl, val = v]() mutable {
              inl->target<ossia::value_port>()->write_value(
                  std::move(val), 1);
            });

          });
        }
      }
    }
  };
  reset();
  con(proc, &Jit::JitEffectModel::changed,
      this, reset);

}

JitEffectComponent::~JitEffectComponent() {}
}
