#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessMetadata.hpp>

#include <Crousti/Attributes.hpp>
#include <Crousti/Concepts.hpp>
#include <Crousti/Metadatas.hpp>
#include <Media/Sound/Drop/SoundDrop.hpp>

#include <ossia/detail/typelist.hpp>

#include <boost/pfr.hpp>

#include <avnd/common/for_nth.hpp>
#include <avnd/concepts/file_port.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/concepts/ui.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/wrappers/bus_host_process_adapter.hpp>
#include <avnd/wrappers/metadatas.hpp>

#include <score_plugin_engine.hpp>

#if SCORE_PLUGIN_GFX
#include <Gfx/TexturePort.hpp>
#endif
/**
 * This file instantiates the classes that are provided by this plug-in.
 */

inline QString fromStringView(std::string_view v)
{
  return QString::fromUtf8(v.data(), v.size());
}
////////// METADATA ////////////
namespace oscr
{
template <typename Info>
class ProcessModel;
}
template <typename Info>
  requires avnd::has_name<Info>
struct Metadata<PrettyName_k, oscr::ProcessModel<Info>>
{
  static constexpr const char* get() noexcept { return avnd::get_name<Info>().data(); }
};
template <typename Info>
  requires avnd::has_category<Info>
struct Metadata<Category_k, oscr::ProcessModel<Info>>
{
  static constexpr const char* get() noexcept
  {
    return avnd::get_category<Info>().data();
  }
};
template <typename Info>
  requires(!avnd::has_category<Info>)
struct Metadata<Category_k, oscr::ProcessModel<Info>>
{
  static constexpr const char* get() noexcept { return ""; }
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

template<typename T>
concept has_kind = requires { T::kind(); };

template<typename T>
auto get_kind()
{ 
  if constexpr (has_kind<T>)
    return T::kind();
  else
    return Process::ProcessCategory::Other;
}

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
#define if_exists(Expr, Else)        \
  []() noexcept {                    \
    if constexpr(requires { Expr; }) \
      return Expr;                   \
    Else;                            \
      }()

#define if_attribute(Attr)                             \
  []() noexcept -> QString {                           \
    if constexpr(avnd::has_##Attr<Info>)               \
      return fromStringView(avnd::get_##Attr<Info>()); \
    else                                               \
      return QString{};                                \
  }()
    static Process::Descriptor desc
    {
      Metadata<PrettyName_k, oscr::ProcessModel<Info>>::get(),
          if_exists(Info::kind(), else return Process::ProcessCategory::Other;),
          if_attribute(category), if_attribute(description), if_attribute(author),
          Metadata<Tags_k, oscr::ProcessModel<Info>>::get(), inletDescription(),
          outletDescription()
    };
    return desc;
  }
};
template <typename Info>
struct Metadata<Process::ProcessFlags_k, oscr::ProcessModel<Info>>
{
  static Process::ProcessFlags get() noexcept
  {
    if constexpr(requires { Info::flags(); })
    {
      return Info::flags();
    }
    else
    {
      return Process::ProcessFlags(
          Process::ProcessFlags::SupportsLasting
          | Process::ProcessFlags::ControlSurface);
    }
  }
};
template <typename Info>
struct Metadata<ObjectKey_k, oscr::ProcessModel<Info>>
{
  static constexpr auto get() noexcept { return avnd::get_c_name<Info>().data(); }
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

template <typename T>
auto& modelPort(auto& ports, int index)
{
  // We have to adjust before accessing a port as there is the first "fake"
  // port if the processor takes audio by argument
  if constexpr(avnd::audio_argument_processor<T>)
    index += 1;

  // The "messages" ports also go before
  index += avnd::messages_introspection<T>::size;

  return ports[index];
}

template <typename T>
inline void setupNewPort(Process::Port* obj)
{
  //FIXME
#if !defined(__APPLE__)
  constexpr
#endif
      auto name
      = avnd::get_name<T>();
  obj->setName(fromStringView(name));

  if constexpr(constexpr auto desc = avnd::get_description<T>(); !desc.empty())
    obj->setDescription(fromStringView(desc));
}

template <std::size_t N, typename T>
inline void setupNewPort(const avnd::field_reflection<N, T>& spec, Process::Port* obj)
{
  setupNewPort<T>(obj);
}

template <typename T>
inline void setupNewPort(const T& spec, Process::Port* obj)
{
  setupNewPort<T>(obj);
}

template <typename Node>
struct InletInitFunc
{
  Process::ProcessModel& self;
  Process::Inlets& ins;
  int inlet = 0;

  void operator()(const avnd::audio_port auto& in, auto idx)
  {
    auto p = new Process::AudioInlet(Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
  }
  void operator()(const avnd::midi_port auto& in, auto idx)
  {
    auto p = new Process::MidiInlet(Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
  }

  static QString toFilters(const QSet<QString>& exts)
  {
    QString res;
    for(const auto& s : exts)
    {
      res += "*.";
      res += s;
      res += " ";
    }
    if(!res.isEmpty())
      res.resize(res.size() - 1);
    return res;
  }

  template <typename P>
  static QString getFilters(P& p)
  {
    if constexpr(requires { P::filters(); })
    {
      using filter = decltype(P::filters());
      if constexpr(
          requires { filter::sound; } || requires { filter::audio; })
      {
        return QString{"Sound files (%1)"}.arg(
            toFilters(Media::Sound::DropHandler{}.fileExtensions()));
      }
      else if constexpr(requires { filter::video; })
      {
        // FIXME refactor supported formats with Video process
        QSet<QString> files = {"mkv",  "mov", "mp4", "h264", "avi",  "hap", "mpg",
                               "mpeg", "imf", "mxf", "mts",  "m2ts", "mj2", "webm"};
        return QString{"Videos (%1)"}.arg(toFilters(files));
      }
      else if constexpr(requires { filter::image; })
      {
        // FIXME refactor supported formats with Image List Chooser
        return QString{"Images (*.png *.jpg *.jpeg *.gif *.bmp *.tiff)"};
      }
      else if constexpr(requires { filter::midi; })
      {
        return "MIDI (*.mid)";
      }
      else if constexpr(avnd::string_ish<filter>)
      {
        constexpr auto text_filters = P::filters();
        return fromStringView(P::filters());
      }
      else
      {
        return "";
      }
    }
    else
    {
      return "";
    }
  }
  template <typename T>
    requires avnd::soundfile_port<T> || avnd::midifile_port<T> || avnd::raw_file_port<T>
  void operator()(const T& in, auto idx)
  {
    constexpr auto name = avnd::get_name<T>();

    auto p = new Process::FileChooser{
        "", getFilters(in), QString::fromUtf8(name.data(), name.size()),
        Id<Process::Port>(inlet++), &self};
    p->hidden = true;
    ins.push_back(p);

    if constexpr(avnd::tag_file_watch<T>)
    {
      p->enableFileWatch();
    }
  }

  template <avnd::parameter T, std::size_t N>
  void operator()(const T& in, avnd::field_index<N>)
  {
    if constexpr(avnd::control<T>)
    {
      auto p = oscr::make_control_in<Node, T>(
          avnd::field_index<N>{}, Id<Process::Port>(inlet), &self);
      if constexpr(!std::is_same_v<std::decay_t<decltype(p)>, std::nullptr_t>)
      {
        p->hidden = true;
        ins.push_back(p);
      }
      else
      {
        auto vp = new Process::ValueInlet(Id<Process::Port>(inlet), &self);
        setupNewPort(in, vp);
        ins.push_back(vp);
      }
    }
    else
    {
      auto vp = new Process::ValueInlet(Id<Process::Port>(inlet), &self);
      setupNewPort(in, vp);
      ins.push_back(vp);
    }
    inlet++;
  }

#if SCORE_PLUGIN_GFX
  void operator()(const avnd::texture_port auto& in, auto idx)
  {
    auto p = new Gfx::TextureInlet(Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
  }
  void operator()(const avnd::geometry_port auto& in, auto idx)
  {
    auto p = new Gfx::GeometryInlet(Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
  }
#endif

  template <std::size_t Idx, avnd::message T>
  void operator()(const avnd::field_reflection<Idx, T>& in)
  {
    auto p = new Process::ValueInlet(Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
  }

  template <std::size_t Idx, avnd::unreflectable_message<Node> T>
  void operator()(const avnd::field_reflection<Idx, T>& in)
  {
    auto p = new Process::ValueInlet(Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
  }

  void operator()(const auto& ctrl, auto idx)
  {
    //(avnd::message<std::decay_t<decltype(ctrl)>>);
    qDebug() << fromStringView(avnd::get_name(ctrl)) << "unhandled";
  }
};

template <typename Node>
struct OutletInitFunc
{
  Process::ProcessModel& self;
  Process::Outlets& outs;
  int outlet = 0;

  void operator()(const avnd::audio_port auto& out, auto idx)
  {
    auto p = new Process::AudioOutlet(Id<Process::Port>(outlet++), &self);
    setupNewPort(out, p);
    if(outlet == 1)
      p->setPropagate(true);
    outs.push_back(p);
  }

  void operator()(const avnd::midi_port auto& out, auto idx)
  {
    auto p = new Process::MidiOutlet(Id<Process::Port>(outlet++), &self);
    setupNewPort(out, p);
    outs.push_back(p);
  }

  template <avnd::parameter T, std::size_t N>
  void operator()(const T& out, avnd::field_index<N>)
  {
    if constexpr(avnd::control<T>)
    {
      if(auto p = oscr::make_control_out<T>(
             avnd::field_index<N>{}, Id<Process::Port>(outlet), &self))
      {
        p->hidden = true;
        outs.push_back(p);
      }
      else
      {
        auto vp = new Process::ValueOutlet(Id<Process::Port>(outlet), &self);
        setupNewPort(out, vp);
        outs.push_back(vp);
      }
    }
    else
    {
      auto vp = new Process::ValueOutlet(Id<Process::Port>(outlet), &self);
      setupNewPort(out, vp);
      outs.push_back(vp);
    }
    outlet++;
  }

#if SCORE_PLUGIN_GFX
  void operator()(const avnd::texture_port auto& out, auto idx)
  {
    auto p = new Gfx::TextureOutlet(Id<Process::Port>(outlet++), &self);
    setupNewPort(out, p);
    outs.push_back(p);
  }
  void operator()(const avnd::geometry_port auto& out, auto idx)
  {
    auto p = new Gfx::GeometryOutlet(Id<Process::Port>(outlet++), &self);
    setupNewPort(out, p);
    outs.push_back(p);
  }
#endif

  void operator()(const avnd::callback auto& out, auto idx)
  {
    auto p = new Process::ValueOutlet(Id<Process::Port>(outlet++), &self);
    setupNewPort(out, p);
    outs.push_back(p);
  }

  void operator()(const auto& ctrl, auto idx)
  {
    qDebug() << fromStringView(avnd::get_name(ctrl)) << "unhandled";
  }
};

template <typename Info>
struct MessageBusWrapperToUi
{
};

template <typename Info>
struct MessageBusWrapperFromUi
{
};
template <avnd::has_processor_to_gui_bus Info>
struct MessageBusWrapperToUi<Info>
{
  std::function<void(QByteArray)> to_ui;
};

template <avnd::has_gui_to_processor_bus Info>
struct MessageBusWrapperFromUi<Info>
{
  std::function<void(QByteArray)> from_ui;
};

template <typename Info>
class ProcessModel final
    : public Process::ProcessModel
    , public MessageBusWrapperFromUi<Info>
    , public MessageBusWrapperToUi<Info>
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(ProcessModel<Info>)
  friend struct TSerializer<DataStream, oscr::ProcessModel<Info>>;
  friend struct TSerializer<JSONObject, oscr::ProcessModel<Info>>;

public:
  ProcessModel(
      const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
      : Process::ProcessModel{
          duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
  {
    metadata().setInstanceName(*this);

    avnd::port_visit_dispatcher<Info>(
        InletInitFunc<Info>{*this, m_inlets}, OutletInitFunc<Info>{*this, m_outlets});

    if constexpr(requires { this->from_ui; })
    {
      this->from_ui = [](QByteArray arr) {};
    }
    if constexpr(requires { this->to_ui; })
    {
      this->to_ui = [](QByteArray arr) {};
    }
  }
  ProcessModel(
      const TimeVal& duration, const QString& custom,
      const Id<Process::ProcessModel>& id, QObject* parent)
      : Process::ProcessModel{
          duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
  {
    metadata().setInstanceName(*this);

    avnd::port_visit_dispatcher<Info>(
        InletInitFunc<Info>{*this, m_inlets}, OutletInitFunc<Info>{*this, m_outlets});

    if constexpr(requires { this->from_ui; })
    {
      this->from_ui = [](QByteArray arr) {};
    }
    if constexpr(requires { this->to_ui; })
    {
      this->to_ui = [](QByteArray arr) {};
    }

    if constexpr(avnd::file_input_introspection<Info>::size > 0)
    {
      constexpr auto idx = avnd::file_input_introspection<Info>::index_to_field_index(0);
      setupInitialStringPort(idx, custom);
    }
    else if constexpr(avnd::control_input_introspection<Info>::size > 0)
    {
      constexpr auto idx
          = avnd::control_input_introspection<Info>::index_to_field_index(0);
      using type =
          typename avnd::control_input_introspection<Info>::template nth_element<0>;
      if constexpr(avnd::string_ish<decltype(type::value)>)
        setupInitialStringPort(idx, custom);
    }
  }

  void setupInitialStringPort(int idx, const QString& custom) noexcept
  {
    Process::Inlet* port = modelPort<Info>(this->inlets(), idx);
    auto pp = safe_cast<Process::ControlInlet*>(port);

    if(pp->value().target<std::string>())
    {
      pp->setValue(custom.toStdString());
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
    Process::writePorts(
        s, s.components.interfaces<Process::PortFactoryList>(), obj.m_inlets,
        obj.m_outlets, &obj);
    s.checkDelimiter();
  }
};

template <typename Info>
struct TSerializer<JSONObject, oscr::ProcessModel<Info>>
{
  using model_type = oscr::ProcessModel<Info>;
  static void readFrom(JSONObject::Serializer& s, const model_type& obj)
  {
    Process::readPorts(s, obj.m_inlets, obj.m_outlets);
  }

  static void writeTo(JSONObject::Deserializer& s, model_type& obj)
  {
    Process::writePorts(
        s, s.components.interfaces<Process::PortFactoryList>(), obj.m_inlets,
        obj.m_outlets, &obj);
  }
};
