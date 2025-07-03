#include "JitModel.hpp"

#include <Process/Dataflow/WidgetInlets.hpp>

#include <JitCpp/Compiler/Driver.hpp>
#include <JitCpp/EditScript.hpp>

#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
//#include <JitCpp/Commands/EditJitEffect.hpp>

#include <Process/Dataflow/PortFactory.hpp>
#include <Process/PresetHelpers.hpp>

#if __has_include(<Gfx/TexturePort.hpp>)
#include <Gfx/TexturePort.hpp>
#endif

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/tools/DeleteAll.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/port.hpp>

#include <wobjectimpl.h>

#include <iostream>
W_OBJECT_IMPL(Jit::JitEffectModel)

namespace Jit
{

JitEffectModel::JitEffectModel(
    TimeVal t, const QString& jitProgram, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{t, id, "Jit", parent}
{
  init();
  if(jitProgram.isEmpty())
    (void)setScript(
        Process::EffectProcessFactory_T<Jit::JitEffectModel>{}.customConstructionData());
  else
    (void)setScript(jitProgram);
}

JitEffectModel::~JitEffectModel() { }

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

Process::ScriptChangeResult JitEffectModel::setScript(const QString& txt)
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

void JitEffectModel::init() { }

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

  Process::Inlet* operator()(const ossia::geometry_port& p) const noexcept
  {
#if __has_include(<Gfx/TexturePort.hpp>)
    auto i = new Gfx::GeometryInlet{getStrongId(self.inlets()), &self};
    return i;
#else
    return nullptr;
#endif
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
      Process::ControlInlet* i{};

      const auto id = getStrongId(self.inlets());
      QString name{"Control"};

      if(auto u = p.type.target<ossia::unit_t>())
      {
        if(*u == ossia::frequency_u{})
        {
          auto min = ossia::convert<float>(ossia::get_min(p.domain));
          auto max = ossia::convert<float>(ossia::get_max(p.domain));
          auto init = min;

          i = new Process::FloatKnob{min, max, init, "Frequency", id, &self};
        }
        else if(u->v.target<ossia::color_u>())
        {
          i = new Process::HSVSlider{ossia::vec4f{}, "Color", id, &self};
        }
        else if(u->v.target<ossia::position_u>())
        {
          if(*u == ossia::cartesian_2d_u{} || *u == ossia::ad_u{}
             || *u == ossia::polar_u{})
            i = new Process::XYSlider{ossia::vec2f{}, "Position", id, &self};
          else
            i = new Process::XYZSlider{ossia::vec3f{}, "Position", id, &self};
        }
        else if(u->v.target<ossia::orientation_u>())
        {
          i = new Process::XYZSlider{ossia::vec3f{}, "Orientation", id, &self};
        }
        else
        {
          auto min = ossia::convert<float>(ossia::get_min(p.domain));
          auto max = ossia::convert<float>(ossia::get_max(p.domain));
          auto init = min;

          i = new Process::FloatKnob{min, max, init, "Frequency", id, &self};
        }
      }
      else if(auto tp = p.type.target<ossia::val_type>())
      {
        switch(*tp)
        {
          case ossia::val_type::INT: {
            auto min = ossia::convert<int>(ossia::get_min(p.domain));
            auto max = ossia::convert<int>(ossia::get_max(p.domain));
            auto init = min;

            i = new Process::IntSlider{min, max, init, name, id, &self};
            break;
          }
          case ossia::val_type::FLOAT: {
            auto min = ossia::convert<float>(ossia::get_min(p.domain));
            auto max = ossia::convert<float>(ossia::get_max(p.domain));
            auto init = min;

            i = new Process::FloatSlider{min, max, init, name, id, &self};
            break;
          }
          case ossia::val_type::STRING: {
            i = new Process::LineEdit{{}, "String input", id, &self};
            break;
          }
          case ossia::val_type::BOOL: {
            i = new Process::Toggle{{}, name, id, &self};
            break;
          }
          case ossia::val_type::IMPULSE: {
            i = new Process::ImpulseButton{name, id, &self};
            break;
          }
          case ossia::val_type::VEC2F: {
            i = new Process::XYSlider{{}, name, id, &self};
            break;
          }
          case ossia::val_type::VEC3F: {
            i = new Process::XYZSlider{{}, name, id, &self};
            break;
          }
          default:
            break;
        }
      }

      if(!i)
      {
        i = new Process::ControlInlet{getStrongId(self.inlets()), &self};
        i->setDomain(State::Domain{p.domain});
      }

      return i;
    }
  }

  Process::Inlet* operator()() const noexcept
  {
    return nullptr;
  }
};

struct outlet_vis
{
  JitEffectModel& self;
  Process::Outlet* operator()(const ossia::audio_port& p) const noexcept
  {
    auto i = new Process::AudioOutlet{getStrongId(self.outlets()), &self};
    return i;
  }

  Process::Outlet* operator()(const ossia::geometry_port& p) const noexcept
  {
#if __has_include(<Gfx/TexturePort.hpp>)
    auto i = new Gfx::GeometryOutlet{getStrongId(self.inlets()), &self};
    return i;
#else
    return nullptr;
#endif
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
  Process::Outlet* operator()() const noexcept
  {
    return nullptr;
  }
};

std::shared_ptr<NodeFactory> JitEffectModel::getJitFactory()
{
  static ossia::flat_map<QByteArray, std::shared_ptr<NodeFactory>> facts;

  auto fx_text = m_text.toUtf8();
  if(fx_text.isEmpty())
    return nullptr;

  if(auto it = facts.find(fx_text); it != facts.end())
  {
    return it->second;
  }

  // FIXME dispos of them once unused at execution
  static std::list<std::shared_ptr<NodeCompiler>> old_compilers;
  if(m_compiler)
  {
    old_compilers.push_front(std::move(m_compiler));
    // if (old_compilers.size() > 5)
    //   old_compilers.pop_back();
  }

  m_compiler = std::make_unique<NodeCompiler>("score_graph_node_factory");

  std::shared_ptr<NodeFactory> jit_factory;
  try
  {
    jit_factory = std::make_shared<NodeFactory>(
        (*m_compiler).operator()<ossia::graph_node*()>(fx_text.toStdString(), {}, CompilerOptions{false}));

    if(!(*jit_factory))
      return nullptr;
  }
  catch(const std::exception& e)
  {
    qDebug() << e.what();
    errorMessage(0, e.what());
    return nullptr;
  }
  catch(...)
  {
    errorMessage(0, "JIT error");
    return nullptr;
  }

  const auto& [it, b] = facts.emplace(std::move(fx_text), std::move(jit_factory));
  return it->second;
}

Process::ScriptChangeResult JitEffectModel::reload()
{
  Process::ScriptChangeResult res;
  auto jit_fac = getJitFactory();
  if(!jit_fac)
    return res;
  auto& jit_factory = *jit_fac;

  std::unique_ptr<ossia::graph_node> jit_object{jit_factory()};
  if(!jit_object)
  {
    jit_factory = {};
    return res;
  }
  // creating a new dsp

  factory = std::move(jit_fac);

  res.inlets = score::clearAndDeleteLater(m_inlets);
  res.outlets = score::clearAndDeleteLater(m_outlets);
  res.valid = true;

  for(ossia::inlet* port : jit_object->root_inputs())
  {
    if(auto inl = port->visit(inlet_vis{*this}))
    {
      m_inlets.push_back(inl);
    }
  }
  for(ossia::outlet* port : jit_object->root_outputs())
  {
    if(auto inl = port->visit(outlet_vis{*this}))
    {
      m_outlets.push_back(inl);
    }
  }

  if(!m_outlets.empty() && m_outlets.front()->type() == Process::PortType::Audio)
    safe_cast<Process::AudioOutlet*>(m_outlets.front())->setPropagate(true);

  return res;
}

QString JitEffectModel::effect() const noexcept
{
  return m_text;
}

void JitEffectModel::loadPreset(const Process::Preset& preset)
{
  Process::loadScriptProcessPreset<JitEffectModel::p_script>(*this, preset);
}

Process::Preset JitEffectModel::savePreset() const noexcept
{
  return Process::saveScriptProcessPreset(*this, this->m_text);
}

}

template <>
void DataStreamReader::read(const Jit::JitEffectModel& eff)
{
  m_stream << eff.m_text;

  readPorts(*this, eff.m_inlets, eff.m_outlets);
}

template <>
void DataStreamWriter::write(Jit::JitEffectModel& eff)
{
  m_stream >> eff.m_text;
  (void)eff.reload();

  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets,
      eff.m_outlets, &eff);
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
  (void)eff.reload();

  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets,
      eff.m_outlets, &eff);
}

namespace Process
{

template <>
QString
EffectProcessFactory_T<Jit::JitEffectModel>::customConstructionData() const noexcept
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

  std::string label() const noexcept override { return "example"; }
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
EffectProcessFactory_T<Jit::JitEffectModel>::descriptor(QString d) const noexcept
{
  return Metadata<Descriptor_k, Jit::JitEffectModel>::get();
}

template <>
Process::Descriptor EffectProcessFactory_T<Jit::JitEffectModel>::descriptor(
    const Process::ProcessModel& d) const noexcept
{
  return descriptor(d.effect());
}
}

namespace Execution
{

Execution::JitEffectComponent::JitEffectComponent(
    Jit::JitEffectModel& proc, const Execution::Context& ctx, QObject* parent)
    : ProcessComponent_T{proc, ctx, "JitComponent", parent}
{
  auto reset = [this, &proc] {
    if(proc.factory && *proc.factory)
    {
      auto pf = (*proc.factory)();
      this->node.reset(pf);
      if(this->node)
      {
        m_ossia_process = std::make_shared<ossia::node_process>(node);

        for(std::size_t i = 0; i < proc.inlets().size(); i++)
        {
          auto inlet = dynamic_cast<Process::ControlInlet*>(proc.inlets()[i]);
          if(!inlet)
            continue;

          auto inl = node->root_inputs()[i];
          inl->target<ossia::value_port>()->write_value(inlet->value(), {});
          connect(
              inlet, &Process::ControlInlet::valueChanged, this,
              [this, inl](const ossia::value& v) {
            system().executionQueue.enqueue([inl, val = v]() mutable {
              inl->target<ossia::value_port>()->write_value(std::move(val), 1);
            });
              });
        }
      }
    }
  };
  reset();
  con(proc, &Jit::JitEffectModel::programChanged, this, reset, Qt::QueuedConnection);
}

JitEffectComponent::~JitEffectComponent() { }
}
