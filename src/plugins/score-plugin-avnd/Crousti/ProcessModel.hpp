#pragma once
#include <boost/pfr.hpp>
#include <Crousti/Concepts.hpp>
#include <Crousti/Attributes.hpp>
#include <Crousti/Metadatas.hpp>

#include <avnd/wrappers/metadatas.hpp>
#include <avnd/concepts/gfx.hpp>

#include <Process/ProcessMetadata.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <score_plugin_engine.hpp>
#include <ossia/detail/typelist.hpp>

#include <avnd/common/for_nth.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/wrappers/bus_host_process_adapter.hpp>

#if SCORE_PLUGIN_GFX
#include <Gfx/TexturePort.hpp>
#endif
/**
 * This file instantiates the classes that are provided by this plug-in.
 */

inline QString fromStringView(std::string_view v) {
  return QString::fromUtf8(v.data(), v.size());
}
////////// METADATA ////////////
namespace oscr
{
template <typename Info>
class ProcessModel;
}
template <typename Info>
struct Metadata<PrettyName_k, oscr::ProcessModel<Info>>
{
  static constexpr const char* get() noexcept
  {
    return avnd::get_name<Info>().data();
  }
};
template <typename Info>
struct Metadata<Category_k, oscr::ProcessModel<Info>>
{
  static constexpr const char* get() noexcept
  {
    return avnd::get_category<Info>().data();
  }
};
template <typename Info>
struct Metadata<Tags_k, oscr::ProcessModel<Info>>
{
  static QStringList get() noexcept
  {
    QStringList lst;
    for(std::string_view tag : avnd::get_tags<Info>())
      lst.push_back(QString::fromUtf8(tag.data(), tag.size()));
    return lst;
  }
};

template <typename Info>
struct Metadata<Process::Descriptor_k, oscr::ProcessModel<Info>>
{
  static std::vector<Process::PortType> inletDescription()
  {
    std::vector<Process::PortType> port;
    /*
    for (std::size_t i = 0; i < info::audio_in_count; i++)
      port.push_back(Process::PortType::Audio);
    for (std::size_t i = 0; i < info::midi_in_count; i++)
      port.push_back(Process::PortType::Midi);
    for (std::size_t i = 0; i < info::value_in_count; i++)
      port.push_back(Process::PortType::Message);
    for (std::size_t i = 0; i < info::control_in_count; i++)
      port.push_back(Process::PortType::Message);
    */
    return port;
  }
  static std::vector<Process::PortType> outletDescription()
  {
    std::vector<Process::PortType> port;
    /*
    for (std::size_t i = 0; i < info::audio_out_count; i++)
      port.push_back(Process::PortType::Audio);
    for (std::size_t i = 0; i < info::midi_out_count; i++)
      port.push_back(Process::PortType::Midi);
    for (std::size_t i = 0; i < info::value_out_count; i++)
      port.push_back(Process::PortType::Message);
    for (std::size_t i = 0; i < info::control_out_count; i++)
      port.push_back(Process::PortType::Message);
    */
    return port;
  }
  static Process::Descriptor get()
  {
// literate programming goes brr
#define if_exists(Expr, Else) [] { if constexpr(requires { Expr; }) return Expr; Else; }()
    static Process::Descriptor desc{
        Metadata<PrettyName_k, oscr::ProcessModel<Info>>::get(),
        if_exists(Info::kind(), else return Process::ProcessCategory::Other;),
        Metadata<Category_k, oscr::ProcessModel<Info>>::get(),
        fromStringView(avnd::get_description<Info>()),
        fromStringView(avnd::get_author<Info>()),
        Metadata<Tags_k, oscr::ProcessModel<Info>>::get(),
        inletDescription(),
        outletDescription()};
    return desc;
  }
};
template <typename Info>
struct Metadata<Process::ProcessFlags_k, oscr::ProcessModel<Info>>
{
  static Process::ProcessFlags get() noexcept {
    if constexpr(requires { Info::flags(); }) {
      return Info::flags();
    } else {
      return Process::ProcessFlags(Process::ProcessFlags::SupportsLasting | Process::ProcessFlags::ControlSurface);
    }
  }
};
template <typename Info>
struct Metadata<ObjectKey_k, oscr::ProcessModel<Info>>
{
  static constexpr auto get() noexcept
  {
    return avnd::get_c_name<Info>().data();
  }
};
template <typename Info>
struct Metadata<ConcreteKey_k, oscr::ProcessModel<Info>>
{
  static Q_DECL_RELAXED_CONSTEXPR UuidKey<Process::ProcessModel> get()
  {
    return oscr::uuid_from_string<Info>();
  }
};

namespace oscr
{

void setupNewPort(const auto& spec, auto* obj)
{
  obj->setName(fromStringView(avnd::get_name(spec)));
}


struct InletInitFunc
{
  Process::ProcessModel& self;
  Process::Inlets& ins;
  int inlet = 0;

  void operator()(const avnd::audio_port auto& in)
  {
    auto p = new Process::AudioInlet(Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
  }
  void operator()(const avnd::midi_port auto& in)
  {
    auto p = new Process::MidiInlet(Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
  }
  template<avnd::parameter T>
  void operator()(const T& in)
  {
    if constexpr(avnd::control<T>)
    {
      constexpr auto ctl = oscr::make_control_in<T>();
      if(auto p = ctl.create_inlet(Id<Process::Port>(inlet++), &self))
      {
        p->hidden = true;
        ins.push_back(p);
      }
    }
    else
    {
      auto p = new Process::ValueInlet(Id<Process::Port>(inlet++), &self);
      setupNewPort(in, p);
      ins.push_back(p);
    }
  }
#if SCORE_PLUGIN_GFX
  void operator()(const avnd::texture_port auto& in)
  {
    auto p = new Gfx::TextureInlet(Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
  }
#endif

  template<std::size_t Idx, avnd::message T>
  void operator()(const avnd::field_reflection<Idx, T>& in)
  {
    auto p = new Process::ValueInlet(Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
  }
  template<std::size_t Idx, avnd::unreflectable_message T>
  void operator()(const avnd::field_reflection<Idx, T>& in)
  {
    auto p = new Process::ValueInlet(Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
  }

  void operator()(const auto& ctrl)
  {
    //(avnd::message<std::decay_t<decltype(ctrl)>>);
    qDebug() << fromStringView(avnd::get_name(ctrl)) << "unhandled";
  }
};

struct OutletInitFunc
{
  Process::ProcessModel& self;
  Process::Outlets& outs;
  int outlet = 0;
  void operator()(const avnd::audio_port auto& out)
  {
    auto p = new Process::AudioOutlet(Id<Process::Port>(outlet++), &self);
    setupNewPort(out, p);
    if (outlet == 1)
      p->setPropagate(true);
    outs.push_back(p);
  }
  void operator()(const avnd::midi_port auto& out)
  {
    auto p = new Process::MidiOutlet(Id<Process::Port>(outlet++), &self);
    setupNewPort(out, p);
    outs.push_back(p);
  }
  template<avnd::parameter T>
  void operator()(const T& out)
  {
    if constexpr(avnd::control<T>)
    {
      constexpr auto ctl = oscr::make_control_out<T>();
      if(auto p = ctl.create_outlet(Id<Process::Port>(outlet++), &self))
      {
        p->hidden = true;
        outs.push_back(p);
      }
    }
    else
    {
      auto p = new Process::ValueOutlet(Id<Process::Port>(outlet++), &self);
      setupNewPort(out, p);
      outs.push_back(p);
    }
  }
#if SCORE_PLUGIN_GFX
  void operator()(const avnd::texture_port auto& out)
  {
    auto p = new Gfx::TextureOutlet(Id<Process::Port>(outlet++), &self);
    setupNewPort(out, p);
    outs.push_back(p);
  }
#endif
  void operator()(const avnd::callback auto& out)
  {
    auto p = new Process::ValueOutlet(Id<Process::Port>(outlet++), &self);
    setupNewPort(out, p);
    outs.push_back(p);
  }
  void operator()(const auto& ctrl)
  {
    qDebug() << fromStringView(avnd::get_name(ctrl)) << "unhandled";
  }
};

template <typename Info>
struct MessageBusWrapper {

};

template <typename Info>
requires (!std::is_void_v<typename Info::ui::bus>)
struct MessageBusWrapper<Info> {
  std::function<void(QByteArray)> from_ui;
  std::function<void(QByteArray)> to_ui;
};

template <typename Info>
class ProcessModel final
    : public Process::ProcessModel
    , public MessageBusWrapper<Info>
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(ProcessModel<Info>)
  friend struct TSerializer<DataStream, oscr::ProcessModel<Info>>;
  friend struct TSerializer<JSONObject, oscr::ProcessModel<Info>>;

public:
  ProcessModel(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent)
      : Process::ProcessModel{
          duration,
          id,
          Metadata<ObjectKey_k, ProcessModel>::get(),
          parent}
  {
    metadata().setInstanceName(*this);

    avnd::port_visit_dispatcher<Info>(
      InletInitFunc{*this, m_inlets},
      OutletInitFunc{*this, m_outlets}
    );

    if constexpr(requires { this->from_ui; }) {
      this->from_ui = [] (QByteArray arr) { };
    }
    if constexpr(requires { this->to_ui; }) {
      this->to_ui = [] (QByteArray arr) { };
    }
  }

  template <typename Impl>
  explicit ProcessModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  ~ProcessModel() override { }
};
}

template <typename Info>
struct is_custom_serialized<oscr::ProcessModel<Info>> : std::true_type
{
};

template <typename Info>
struct TSerializer<DataStream, oscr::ProcessModel<Info>>
{
  using model_type = oscr::ProcessModel<Info>;
  static void readFrom(DataStream::Serializer& s, const model_type& obj)
  {
    Process::readPorts(s, obj.m_inlets, obj.m_outlets);
    s.insertDelimiter();
  }

  static void writeTo(DataStream::Deserializer& s, model_type& obj)
  {
    Process::writePorts(s, s.components.interfaces<Process::PortFactoryList>(), obj.m_inlets, obj.m_outlets, &obj);
    s.checkDelimiter();
  }
};

template <typename Info>
struct TSerializer<JSONObject, oscr::ProcessModel<Info>>
{
  using model_type = oscr::ProcessModel<Info>;
  static void readFrom(JSONObject::Serializer& s, const model_type& obj)
  {
    using namespace Control;

    Process::readPorts(s, obj.m_inlets, obj.m_outlets);
  }

  static void writeTo(JSONObject::Deserializer& s, model_type& obj)
  {
    using namespace Control;

    Process::writePorts(s, s.components.interfaces<Process::PortFactoryList>(), obj.m_inlets, obj.m_outlets, &obj);
  }
};

namespace score
{
template <typename Vis, typename Info>
void serialize_dyn_impl(Vis& v, const oscr::ProcessModel<Info>& t)
{
  TSerializer<typename Vis::type, oscr::ProcessModel<Info>>::readFrom(
      v, t);
}
}

