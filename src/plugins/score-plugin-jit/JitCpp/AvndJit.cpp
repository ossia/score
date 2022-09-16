#include "AvndJit.hpp"

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
W_OBJECT_IMPL(AvndJit::Model)

namespace AvndJit
{

Model::Model(
    TimeVal t, const QString& jitProgram, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{t, id, "Jit", parent}
{
  init();
  if(jitProgram.isEmpty())
    setScript(
        Process::EffectProcessFactory_T<AvndJit::Model>{}.customConstructionData());
  else
    setScript(jitProgram);
}

Model::~Model() { }

Model::Model(JSONObject::Deserializer& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

Model::Model(DataStream::Deserializer& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

Model::Model(JSONObject::Deserializer&& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

Model::Model(DataStream::Deserializer&& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  init();
}

bool Model::validate(const QString& txt) const noexcept
{
  SCORE_TODO;
  return true;
}

void Model::setScript(const QString& txt)
{
  if(m_text != txt)
  {
    m_text = txt;
    reload();
    scriptChanged(txt);
  }
}

void Model::init() { }

QString Model::prettyName() const noexcept
{
  return "Jit";
}

struct inlet_vis
{
  Model& self;
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
  Model& self;
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

std::shared_ptr<NodeFactory> Model::getJitFactory()
{
  static std::map<QByteArray, std::shared_ptr<NodeFactory>> facts;

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

  m_compiler = std::make_unique<NodeCompiler>("avnd_factory");

  std::shared_ptr<NodeFactory> jit_factory;
  try
  {
    auto str = fx_text.toStdString();
    str += R"_(
#include <avnd/binding/ossia/all.hpp>
#include <avnd/binding/ossia/node.hpp>
#include <avnd/binding/ossia/data_node.hpp>
#include <avnd/binding/ossia/mono_audio_node.hpp>
#include <avnd/binding/ossia/ossia_audio_node.hpp>
#include <avnd/binding/ossia/poly_audio_node.hpp>
#include <avnd/binding/ossia/configure.hpp>
__attribute__ ((visibility("default")))
extern "C" ossia::graph_node* avnd_factory(int buffersize, double rate) {{
  using type = decltype(avnd::configure<oscr::config, Node>())::type;
  auto n = new oscr::safe_node<type>(buffersize, rate, 0);
  n->finish_init();
  n->audio_configuration_changed();
  return n;
}}
)_";
    jit_factory = std::make_shared<NodeFactory>(
        (*m_compiler).operator()<ossia::graph_node*(int, double)>(str, {}, Jit::CompilerOptions{false}));

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

void Model::reload()
{
  auto jit_fac = getJitFactory();
  if(!jit_fac)
    return;
  auto& jit_factory = *jit_fac;

  std::unique_ptr<ossia::graph_node> jit_object{jit_factory(512, 44100)};
  if(!jit_object)
  {
    jit_factory = {};
    return;
  }
  // creating a new dsp

  factory = std::move(jit_fac);

  auto inls = score::clearAndDeleteLater(m_inlets);
  auto outls = score::clearAndDeleteLater(m_outlets);

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

  inletsChanged();
  outletsChanged();
  changed();
}

QString Model::effect() const noexcept
{
  return m_text;
}

void Model::loadPreset(const Process::Preset& preset)
{
  Process::loadScriptProcessPreset<Model::p_script>(*this, preset);
}

Process::Preset Model::savePreset() const noexcept
{
  return Process::saveScriptProcessPreset(*this, this->m_text);
}

}

template <>
void DataStreamReader::read(const AvndJit::Model& eff)
{
  m_stream << eff.m_text;

  readPorts(*this, eff.m_inlets, eff.m_outlets);
}

template <>
void DataStreamWriter::write(AvndJit::Model& eff)
{
  m_stream >> eff.m_text;
  eff.reload();

  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), eff.m_inlets,
      eff.m_outlets, &eff);
}

template <>
void JSONReader::read(const AvndJit::Model& eff)
{
  obj["Text"] = eff.script();
  readPorts(*this, eff.m_inlets, eff.m_outlets);
}

template <>
void JSONWriter::write(AvndJit::Model& eff)
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
QString EffectProcessFactory_T<AvndJit::Model>::customConstructionData() const noexcept
{
  return R"_(
struct Node
{
  static consteval auto name() { return "Addition"; }
  static consteval auto c_name() { return "avnd_addition"; }
  static consteval auto uuid() { return "fbfae018-6fd3-4b1f-9963-90e0133ac268"; }

  struct
  {
    struct
    {
      float value;
    } a;
    struct
    {
      float value;
    } b;
  } inputs;

  struct
  {
    struct
    {
      float value;
    } out;
  } outputs;

  void operator()() { outputs.out.value = inputs.a.value + inputs.b.value; }
};
)_";
}

template <>
Process::Descriptor
EffectProcessFactory_T<AvndJit::Model>::descriptor(QString d) const noexcept
{
  return Metadata<Descriptor_k, AvndJit::Model>::get();
}

}
namespace AvndJit
{

Executor::Executor(AvndJit::Model& proc, const Execution::Context& ctx, QObject* parent)
    : ProcessComponent_T{proc, ctx, "JitComponent", parent}
{
  auto reset = [this, &ctx, &proc] {
    if(proc.factory && *proc.factory)
    {
      auto pf = (*proc.factory)(ctx.execState->bufferSize, ctx.execState->sampleRate);
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
  con(proc, &AvndJit::Model::changed, this, reset, Qt::QueuedConnection);
}

Executor::~Executor() { }
}
