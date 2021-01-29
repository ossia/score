#pragma once
#include <Faust/FaustUtils.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>

#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/node_process.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>

namespace FaustDSP
{
template <typename DSP>
class Fx;
}

template <typename DSP>
struct Metadata<Category_k, FaustDSP::Fx<DSP>>
{
  static Q_DECL_RELAXED_CONSTEXPR const char* get() { return "Audio"; }
};
template <typename DSP>
struct Metadata<Tags_k, FaustDSP::Fx<DSP>>
{
  static QStringList get()
  {
    QStringList lst{"Audio"};
    return lst;
  }
};
template <typename DSP>
struct Metadata<Process::ProcessFlags_k, FaustDSP::Fx<DSP>>
{
  static Process::ProcessFlags get() { return Process::ProcessFlags::SupportsAll; }
};
template <typename DSP>
struct Metadata<Process::Descriptor_k, FaustDSP::Fx<DSP>>
{
  static std::vector<Process::PortType> inletDescription()
  {
    std::vector<Process::PortType> port;
    port.push_back(Process::PortType::Audio);
    return port;
  }
  static std::vector<Process::PortType> outletDescription()
  {
    std::vector<Process::PortType> port;
    port.push_back(Process::PortType::Audio);
    return port;
  }
  static Process::Descriptor get()
  {
    static Process::Descriptor desc{
        Metadata<PrettyName_k, FaustDSP::Fx<DSP>>::get(),
        Process::ProcessCategory::AudioEffect,
        Metadata<Category_k, FaustDSP::Fx<DSP>>::get(),
        "Faust effect",
        "Faust",
        Metadata<Tags_k, FaustDSP::Fx<DSP>>::get(),
        inletDescription(),
        outletDescription()};
    return desc;
  }
};

namespace FaustDSP
{
template <typename T>
struct Wrap final : UI
{
  template <typename... Args>
  Wrap(Args&&... args) : t{args...}
  {
  }

  T t;
  void openTabBox(const char* label) override { t.openTabBox(label); }
  void openHorizontalBox(const char* label) override { t.openHorizontalBox(label); }
  void openVerticalBox(const char* label) override { t.openVerticalBox(label); }
  void closeBox() override { t.closeBox(); }
  void declare(FAUSTFLOAT* zone, const char* key, const char* val) override
  {
    t.declare(zone, key, val);
  }
  void addSoundfile(const char* label, const char* filename, Soundfile** sf_zone) override
  {
    t.addSoundfile(label, filename, sf_zone);
  }
  void addButton(const char* label, FAUSTFLOAT* zone) override { t.addButton(label, zone); }
  void addCheckButton(const char* label, FAUSTFLOAT* zone) override
  {
    t.addCheckButton(label, zone);
  }
  void addVerticalSlider(
      const char* label,
      FAUSTFLOAT* zone,
      FAUSTFLOAT init,
      FAUSTFLOAT min,
      FAUSTFLOAT max,
      FAUSTFLOAT step) override
  {
    t.addVerticalSlider(label, zone, init, min, max, step);
  }
  void addHorizontalSlider(
      const char* label,
      FAUSTFLOAT* zone,
      FAUSTFLOAT init,
      FAUSTFLOAT min,
      FAUSTFLOAT max,
      FAUSTFLOAT step) override
  {
    t.addHorizontalSlider(label, zone, init, min, max, step);
  }
  void addNumEntry(
      const char* label,
      FAUSTFLOAT* zone,
      FAUSTFLOAT init,
      FAUSTFLOAT min,
      FAUSTFLOAT max,
      FAUSTFLOAT step) override
  {
    t.addNumEntry(label, zone, init, min, max, step);
  }
  void addHorizontalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max)
      override
  {
    t.addHorizontalBargraph(label, zone, min, max);
  }
  void
  addVerticalBargraph(const char* label, FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max) override
  {
    t.addVerticalBargraph(label, zone, min, max);
  }
};

template <typename DSP>
class Fx final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Fx)

public:
  Fx(TimeVal t, const Id<Process::ProcessModel>& id, QObject* parent)
      : Process::ProcessModel{t, id, Metadata<ObjectKey_k, Fx>::get(), parent}
  {
    Wrap<Faust::UI<decltype(*this)>> ui{*this};
    DSP d;

    m_inlets.push_back(new Process::Inlet{getStrongId(m_inlets), this});
    m_inlets.back()->type = Process::PortType::Audio;
    m_outlets.push_back(new Process::Outlet{getStrongId(m_outlets), this});
    m_outlets.back()->type = Process::PortType::Audio;
    m_outlets.back()->setPropagate(true);

    d.buildUserInterface(&ui);
  }

  ~Fx() override { }

  template <typename Impl>
  Fx(Impl& vis, QObject* parent) : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  QString prettyName() const noexcept override { return Metadata<PrettyName_k, Fx>::get(); }

  Process::Inlets& inlets() { return m_inlets; }
  Process::Outlets& outlets() { return m_outlets; }
  const Process::Inlets& inlets() const { return m_inlets; }
  const Process::Outlets& outlets() const { return m_outlets; }

  bool hasExternalUI() const noexcept { return false; }
};

template <typename DSP>
class Executor final : public Execution::ProcessComponent_T<Fx<DSP>, ossia::node_process>
{

public:
  static constexpr bool is_unique = true;

  class exec_node final : public ossia::graph_node
  {
  public:
    DSP dsp;
    ossia::small_vector<std::pair<ossia::value_port*, FAUSTFLOAT*>, 8> controls;
    exec_node()
    {
      m_inlets.push_back(new ossia::audio_inlet);
      m_outlets.push_back(new ossia::audio_outlet);
      Wrap<ossia::nodes::faust_exec_ui<exec_node>> ex{*this};
      dsp.buildUserInterface(&ex);
    }

    void run(const ossia::token_request& tk, ossia::exec_state_facade) noexcept override
    {
      ossia::nodes::faust_exec(*this, dsp, tk);
    }

    std::string label() const noexcept override { return "Faust"; }

    void all_notes_off() noexcept override { }
  };

  static Q_DECL_RELAXED_CONSTEXPR score::Component::Key static_key() noexcept
  {
    return Metadata<ConcreteKey_k, Fx<DSP>>::get().impl();
  }

  score::Component::Key key() const noexcept final override { return static_key(); }

  bool key_match(score::Component::Key other) const noexcept final override
  {
    return static_key() == other || Execution::ProcessComponent::base_key_match(other);
  }

  Executor(
      Fx<DSP>& proc,
      const Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent)
      : Execution::ProcessComponent_T<Fx<DSP>, ossia::node_process>{
          proc,
          ctx,
          id,
          "FaustComponent",
          parent}
  {
    auto node = std::make_shared<exec_node>();
    this->node = node;
    this->m_ossia_process = std::make_shared<ossia::node_process>(node);
    node->dsp.instanceInit(ctx.execState->sampleRate);

    for (std::size_t i = 1; i < proc.inlets().size(); i++)
    {
      auto inlet = static_cast<Process::ControlInlet*>(proc.inlets()[i]);
      *node->controls[i - 1].second = ossia::convert<double>(inlet->value());
      auto inl = this->node->inputs()[i];
      QObject::connect(
          inlet, &Process::ControlInlet::valueChanged, this, [this, inl](const ossia::value& v) {
            this->system().executionQueue.enqueue([inl, val = v]() mutable {
              inl->data.template target<ossia::value_port>()->write_value(std::move(val), 0);
            });
          });
    }
  }
};
}

template <typename DSP>
struct is_custom_serialized<FaustDSP::Fx<DSP>> : std::true_type
{
};

template <typename DSP>
struct TSerializer<DataStream, FaustDSP::Fx<DSP>>
{
  using model_type = FaustDSP::Fx<DSP>;
  static void readFrom(DataStream::Serializer& s, const model_type& eff)
  {
    readPorts(s, eff.inlets(), eff.outlets());
    s.insertDelimiter();
  }

  static void writeTo(DataStream::Deserializer& s, model_type& eff)
  {
    writePorts(
        s, s.components.interfaces<Process::PortFactoryList>(), eff.inlets(), eff.outlets(), &eff);

    s.checkDelimiter();
  }
};

template <typename DSP>
struct TSerializer<JSONObject, FaustDSP::Fx<DSP>>
{
  using model_type = FaustDSP::Fx<DSP>;
  static void readFrom(JSONObject::Serializer& s, const model_type& eff)
  {
    readPorts(s.obj, eff.inlets(), eff.outlets());
  }

  static void writeTo(JSONObject::Deserializer& s, model_type& eff)
  {
    writePorts(
        s.obj,
        s.components.interfaces<Process::PortFactoryList>(),
        eff.inlets(),
        eff.outlets(),
        &eff);
  }
};

namespace score
{
template <typename Vis, typename DSP>
void serialize_dyn_impl(Vis& v, const FaustDSP::Fx<DSP>& t)
{
  TSerializer<typename Vis::type, FaustDSP::Fx<DSP>>::readFrom(v, t);
}
}

namespace FaustDSP
{
template <typename DSP>
using ProcessFactory = Process::ProcessFactory_T<Fx<DSP>>;

template <typename DSP>
using LayerFactory = Process::EffectLayerFactory_T<Fx<DSP>, Process::DefaultEffectItem>;

template <typename DSP>
using ExecutorFactory = Execution::ProcessComponentFactory_T<Executor<DSP>>;
}
