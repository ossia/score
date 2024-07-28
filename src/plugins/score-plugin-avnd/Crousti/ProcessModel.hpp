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
#include <Dataflow/Commands/CableHelpers.hpp>
#include <Media/Sound/Drop/SoundDrop.hpp>

#include <ossia/detail/typelist.hpp>

#include <boost/pfr.hpp>

#include <QTimer>

#include <avnd/binding/ossia/data_node.hpp>
#include <avnd/binding/ossia/dynamic_ports.hpp>
#include <avnd/common/for_nth.hpp>
#include <avnd/concepts/file_port.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/concepts/temporality.hpp>
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

template <typename T>
concept has_kind = requires { T::kind(); };

template <typename T>
auto get_kind()
{
  if constexpr(has_kind<T>)
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
#if defined(_MSC_VER)
#define if_exists(Expr, Else) []() noexcept { if(false) {} Else; } ()
#define if_attribute(Attr) QString{}
#else
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
#endif
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
      Process::ProcessFlags flags = Process::ProcessFlags(
          Process::ProcessFlags::SupportsLasting
          | Process::ProcessFlags::ControlSurface);

      if constexpr(avnd::tag_single_exec<Info>)
        flags |= Process::ProcessFlags::SupportsState;

      return flags;
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
inline void setupNewPort(Process::Port* obj)
{
  //FIXME
#if !defined(__APPLE__)
  constexpr auto name = avnd::get_name<T>();
  obj->setName(fromStringView(name));

  if constexpr(constexpr auto desc = avnd::get_description<T>(); !desc.empty())
    obj->setDescription(fromStringView(desc));
#else
  auto name = avnd::get_name<T>();
  obj->setName(fromStringView(name));
  if(auto desc = avnd::get_description<T>(); !desc.empty())
    obj->setDescription(fromStringView(desc));
#endif
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
    requires avnd::soundfile_port<T>
  void operator()(const T& in, auto idx)
  {
    constexpr auto name = avnd::get_name<T>();
    Process::FileChooserBase* p{};
    if constexpr(requires { T::waveform; })
    {
      p = new Process::AudioFileChooser{
          "", getFilters(in), QString::fromUtf8(name.data(), name.size()),
          Id<Process::Port>(inlet++), &self};
    }
    else
    {
      p = new Process::FileChooser{
          "", getFilters(in), QString::fromUtf8(name.data(), name.size()),
          Id<Process::Port>(inlet++), &self};
    }

    p->hidden = true;
    ins.push_back(p);

    if constexpr(avnd::tag_file_watch<T>)
    {
      p->enableFileWatch();
    }
  }

  template <typename T>
    requires avnd::midifile_port<T> || avnd::raw_file_port<T>
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
    if constexpr(avnd::control<T> || avnd::curve_port<T>)
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
  void operator()(const avnd::field_reflection<Idx, T>& in, auto dummy)
  {
    auto p = new Process::ValueInlet(Id<Process::Port>(inlet++), &self);
    setupNewPort(in, p);
    ins.push_back(p);
  }

  template <std::size_t Idx, avnd::unreflectable_message<Node> T>
  void operator()(const avnd::field_reflection<Idx, T>& in, auto dummy)
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

  void operator()(const avnd::curve_port auto& out, auto idx)
  {
    auto p = new Process::ValueOutlet(Id<Process::Port>(outlet++), &self);
    setupNewPort(out, p);
    outs.push_back(p);
  }

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

namespace
{
struct dummy_ui_callback
{
  void operator()(const QByteArray& arr) noexcept { }
};
}

template <avnd::has_processor_to_gui_bus Info>
struct MessageBusWrapperToUi<Info>
{
  std::function<void(QByteArray)> to_ui = dummy_ui_callback{};
};

template <avnd::has_gui_to_processor_bus Info>
struct MessageBusWrapperFromUi<Info>
{
  std::function<void(QByteArray)> from_ui = dummy_ui_callback{};
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
  [[no_unique_address]]
  oscr::dynamic_ports_storage<Info> dynamic_ports;

  std::conditional_t<oscr::has_dynamic_ports<Info>, Info, char>
      object_storage_for_ports_callbacks;

  ProcessModel(
      const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
      : Process::ProcessModel{
          duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
  {
    metadata().setInstanceName(*this);

    init_before_port_creation();
    init_all_ports();
    init_after_port_creation();
  }

  ProcessModel(
      const TimeVal& duration, const QString& custom,
      const Id<Process::ProcessModel>& id, QObject* parent)
      : Process::ProcessModel{
            duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
  {
    metadata().setInstanceName(*this);

    init_before_port_creation();
    init_all_ports();

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
    init_after_port_creation();
  }

  void setupInitialStringPort(int idx, const QString& custom) noexcept
  {
    Process::Inlet* port = avnd_input_idx_to_model_ports(idx)[0];
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
    init_before_port_creation();
    vis.writeTo(*this);
    check_all_ports();
    init_after_port_creation();
  }

  ~ProcessModel() override { }

private:
  void init_before_port_creation() { init_dynamic_ports(); }
  void init_after_port_creation() { init_controller_ports(); }
  void check_all_ports()
  {
    if(m_inlets.size() != expected_input_ports()
       || m_outlets.size() != expected_output_ports())
    {
      qDebug() << "Warning : process does not match spec.";

      std::vector<Dataflow::SavedPort> m_oldInlets, m_oldOutlets;
      for(auto& port : m_inlets)
        m_oldInlets.emplace_back(
            Dataflow::SavedPort{port->name(), port->type(), port->saveData()});
      for(auto& port : m_outlets)
        m_oldOutlets.emplace_back(
            Dataflow::SavedPort{port->name(), port->type(), port->saveData()});

      qDeleteAll(m_inlets);
      m_inlets.clear();
      qDeleteAll(m_outlets);
      m_outlets.clear();

      init_all_ports();

      Dataflow::reloadPortsInNewProcess(m_oldInlets, m_oldOutlets, *this);
    }
  }

  void init_controller_ports()
  {
    if constexpr(
        avnd::dynamic_ports_input_introspection<Info>::size > 0
        || avnd::dynamic_ports_input_introspection<Info>::size > 0)
    {
      avnd::input_introspection<Info>::for_all(
          [this]<std::size_t Idx, typename F>(avnd::field_reflection<Idx, F>) {
        if constexpr(requires { F::on_controller_setup(); })
        {
          auto controller_inlets = avnd_input_idx_to_model_ports(Idx);
          SCORE_ASSERT(controller_inlets.size() == 1);
          auto inlet = qobject_cast<Process::ControlInlet*>(controller_inlets[0]);
          decltype(F::value) current_value;
          oscr::from_ossia_value(inlet->value(), current_value);
          F::on_controller_setup()(
              this->object_storage_for_ports_callbacks, current_value);
        }
        if constexpr(requires { F::on_controller_interaction(); })
        {
          auto controller_inlets = avnd_input_idx_to_model_ports(Idx);
          SCORE_ASSERT(controller_inlets.size() == 1);
          auto inlet = qobject_cast<Process::ControlInlet*>(controller_inlets[0]);
          inlet->noValueChangeOnMove = true;
          connect(
              inlet, &Process::ControlInlet::valueChanged,
              [this](const ossia::value& val) {
            decltype(F::value) current_value;
            oscr::from_ossia_value(val, current_value);
            F::on_controller_interaction()(
                this->object_storage_for_ports_callbacks, current_value);
          });
        }
      });
    }
  }

  void init_dynamic_ports()
  {
    // To check:

    // - serialization
    // - OK: uses of total_input_count / total_output_count

    // - OK: uses of oscr::modelPort (btw this does not seem to match total_input_count)

    // - execution, resize the avnd object with the number of ports
    //  -> connect controls so that they change in the ui

    // - OK: Layer::createControl: uses index_in_struct and assumes it's a ProcessModel::inlet index

    // How are things going to happen UI-wise: all controls created one after each other ?
    // what if we want to create separate tabs with e.g.
    // control A 1, control B 1
    // control A 2, control B 2
    // we need to know the current state of dynamic ports

    // UI: recreating UI causes crash when editing e.g. context menu of spinbox
    // UI: recreating UI causes bug when editing e.g. spinbox

    // -> we have to find a way to detect that the currently edited object is being deleted

    // other option: mark widget as "unsafe", do not have it send value changes until mouse is released

    // REloading on crash: does crash, because the control change does not have
    // the time to be applied. In DocumentBuilder::loadCommandStack we have
    // to process the event loop in-between events.
    // FIxed if we put zero but then changing the widget with the mouses crashes because of the comment
    // below...
    // ->
    //   terminate called after throwing an instance of 'std::runtime_error'
    //   what():  Ongoing command mismatch: current command SetControlValue does not match new command MoveNodes

    if constexpr(avnd::dynamic_ports_input_introspection<Info>::size > 0)
    {
      auto& obj = object_storage_for_ports_callbacks;
      avnd::dynamic_ports_input_introspection<Info>::for_all_n2(
          avnd::get_inputs(obj),
          [&]<std::size_t N>(auto& port, auto pred_idx, avnd::field_index<N> field_idx) {
        port.request_port_resize = [this, &port](int new_count) {
          // We're in the ui thread, we can just push the request directly.
          // With some delay as otherwise we may be deleting the widget we
          // are clicking on before mouse release and Qt really doesn't like that
          QTimer::singleShot(0, this, [self = QPointer{this}, &port, new_count] {
            if(self)
              self->request_new_dynamic_input_count(
                  port, avnd::field_index<N>{}, new_count);
          });
        };
      });
    }

    if constexpr(avnd::dynamic_ports_output_introspection<Info>::size > 0)
    {
      auto& obj = object_storage_for_ports_callbacks;
      avnd::dynamic_ports_output_introspection<Info>::for_all_n2(
          avnd::get_outputs(obj),
          [&]<std::size_t N>(auto& port, auto pred_idx, avnd::field_index<N> field_idx) {
        port.request_port_resize = [this, &port](int new_count) {
          // We're in the ui thread, we can just push the request directly.
          // With some delay as otherwise we may be deleting the widget we
          // are clicking on before mouse release and Qt really doesn't like that
          QTimer::singleShot(0, this, [self = QPointer{this}, &port, new_count] {
            if(self)
              self->request_new_dynamic_output_count(
                  port, avnd::field_index<N>{}, new_count);
          });
        };
      });
    }
  }

  template <typename P, std::size_t N>
  void request_new_dynamic_input_count(P& port, avnd::field_index<N> idx, int count)
  {
    const int current_model_ports = dynamic_ports.num_in_ports(idx);
    if(current_model_ports == count || count < 0 || count > 512)
      return;

    ossia::small_pod_vector<Process::Inlet*, 4> to_delete;
    auto current_inlets = m_inlets;
    if(current_model_ports < count)
    {
      // Add new ports
      // 1. Find the location where to add them
      auto res = avnd_input_idx_to_iterator(idx);
      res += current_model_ports;
      Process::Inlets inlets_to_add;
      InletInitFunc<Info> inlets{*this, inlets_to_add};
      inlets.inlet = 10000 + N * 1000 + current_model_ports;
      int current_inlets = m_inlets.size();
      int sz = 0;
      for(int i = current_model_ports; i < count; i++)
      {
        inlets(port, idx);
        if(inlets_to_add.size() > sz)
        {
          sz = inlets_to_add.size();
          auto new_inlet = inlets_to_add.back();
          if(auto nm = new_inlet->name(); nm.contains("{}"))
          {
            nm.replace("{}", QString::number(i));
            new_inlet->setName(nm);
          }
        }
      }
      m_inlets.insert(res, inlets_to_add.begin(), inlets_to_add.end());
    }
    else if(current_model_ports > count)
    {
      // Delete the ports
      auto res = avnd_input_idx_to_iterator(idx);
      res += count;
      auto begin_deleted = res;
      for(int i = 0; i < (current_model_ports - count); i++)
      {
        to_delete.push_back(*res);
        ++res;
      }
      m_inlets.erase(begin_deleted, res);
    }

    dynamic_ports.num_in_ports(idx) = count;

    inletsChanged();
    for(auto port : to_delete)
      delete port;
  }

  template <typename P, std::size_t N>
  void request_new_dynamic_output_count(P& port, avnd::field_index<N> idx, int count)
  {
    const int current_model_ports = dynamic_ports.num_in_ports(idx);
    if(current_model_ports == count || count < 0 || count > 512)
      return;

    ossia::small_pod_vector<Process::Outlet*, 4> to_delete;
    auto current_outlets = m_outlets;
    if(current_model_ports < count)
    {
      // Add new ports
      // 1. Find the location where to add them
      auto res = avnd_output_idx_to_iterator(idx);
      res += current_model_ports;
      Process::Outlets outlets_to_add;
      OutletInitFunc<Info> outlets{*this, outlets_to_add};
      outlets.outlet = 10000 + N * 1000 + current_model_ports;
      int current_outlets = m_outlets.size();
      int sz = 0;
      for(int i = current_model_ports; i < count; i++)
      {
        outlets(port, idx);
        if(outlets_to_add.size() > sz)
        {
          sz = outlets_to_add.size();
          auto new_outlet = outlets_to_add.back();
          if(auto nm = new_outlet->name(); nm.contains("{}"))
          {
            nm.replace("{}", QString::number(i));
            new_outlet->setName(nm);
          }
        }
      }
      m_outlets.insert(res, outlets_to_add.begin(), outlets_to_add.end());
    }
    else if(current_model_ports > count)
    {
      // Delete the ports
      auto res = avnd_output_idx_to_iterator(idx);
      res += count;
      auto begin_deleted = res;
      for(int i = 0; i < (current_model_ports - count); i++)
      {
        to_delete.push_back(*res);
        ++res;
      }
      m_outlets.erase(begin_deleted, res);
    }

    dynamic_ports.num_in_ports(idx) = count;

    outletsChanged();
    for(auto port : to_delete)
      delete port;
  }

  void init_all_ports()
  {
    InletInitFunc<Info> inlets{*this, m_inlets};
    OutletInitFunc<Info> outlets{*this, m_outlets};
    avnd::port_visit_dispatcher<Info>([&inlets]<typename P>(P&& port, auto idx) {
      if constexpr(!avnd::dynamic_ports_port<P>)
        inlets(port, idx);
    }, [&outlets]<typename P>(P&& port, auto idx) {
      if constexpr(!avnd::dynamic_ports_port<P>)
        outlets(port, idx);
    });
  }

public:
  int expected_input_ports() const noexcept
  {
    int count = 0;

    // We have to adjust before accessing a port as there is the first "fake"
    // port if the processor takes audio by argument
    if constexpr(avnd::audio_argument_processor<Info>)
      count += 1;

    // The "messages" ports also go before
    count += avnd::messages_introspection<Info>::size;

    avnd::input_introspection<Info>::for_all([this, &count]<std::size_t Idx, typename P>(
                                                 avnd::field_reflection<Idx, P> field) {
      int num_ports = 1;
      if constexpr(avnd::dynamic_ports_port<P>)
        num_ports = dynamic_ports.num_in_ports(avnd::field_index<Idx>{});
      count += num_ports;
    });

    return count;
  }

  int expected_output_ports() const noexcept
  {
    int count = 0;

    // We have to adjust before accessing a port as there is the first "fake"
    // port if the processor takes audio by argument
    if constexpr(avnd::audio_argument_processor<Info>)
      count += 1;

    avnd::output_introspection<Info>::for_all(
        [this,
         &count]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P> field) {
      int num_ports = 1;
      if constexpr(avnd::dynamic_ports_port<P>)
        num_ports = dynamic_ports.num_in_ports(avnd::field_index<Idx>{});
      count += num_ports;
    });

    return count;
  }

  std::span<Process::Inlet*> avnd_input_idx_to_model_ports(int index) const noexcept
  {
    int model_index = 0;

    // We have to adjust before accessing a port as there is the first "fake"
    // port if the processor takes audio by argument
    if constexpr(avnd::audio_argument_processor<Info>)
      model_index += 1;

    // The "messages" ports also go before
    model_index += avnd::messages_introspection<Info>::size;

    std::span<Process::Inlet*> ret;
    if constexpr(avnd::dynamic_ports_input_introspection<Info>::size == 0)
    {
      ret = std::span<Process::Inlet*>(
          const_cast<Process::Inlet**>(this->m_inlets.data()) + model_index + index, 1);
    }
    else
    {
      avnd::input_introspection<Info>::for_all(
          [this, index, &model_index,
           &ret]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P> field) {
        if(Idx == index)
        {
          int num_ports = 1;
          if constexpr(avnd::dynamic_ports_port<P>)
          {
            num_ports = dynamic_ports.num_in_ports(avnd::field_index<Idx>{});
            if(num_ports == 0)
            {
              ret = {};
              return;
            }
          }
          ret = std::span<Process::Inlet*>(
              const_cast<Process::Inlet**>(this->m_inlets.data()) + model_index,
              num_ports);
        }
        else
        {
          if constexpr(avnd::dynamic_ports_port<P>)
          {
            model_index += dynamic_ports.num_in_ports(avnd::field_index<Idx>{});
          }
          else
          {
            model_index += 1;
          }
        }
      });
    }

    return ret;
  }

  std::span<Process::Outlet*> avnd_output_idx_to_model_ports(int index) const noexcept
  {
    int model_index = 0;

    // We have to adjust before accessing a port as there is the first "fake"
    // port if the processor takes audio by argument
    if constexpr(avnd::audio_argument_processor<Info>)
      model_index += 1;

    // The "messages" ports also go before
    model_index += avnd::messages_introspection<Info>::size;

    std::span<Process::Outlet*> ret;
    if constexpr(avnd::dynamic_ports_output_introspection<Info>::size == 0)
    {
      ret = std::span<Process::Outlet*>(
          const_cast<Process::Outlet**>(this->m_outlets.data()) + model_index + index,
          1);
    }
    else
    {
      avnd::output_introspection<Info>::for_all(
          [this, index, &model_index,
           &ret]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P> field) {
        if(Idx == index)
        {
          int num_ports = 1;
          if constexpr(avnd::dynamic_ports_port<P>)
          {
            num_ports = dynamic_ports.num_out_ports(avnd::field_index<Idx>{});
            if(num_ports == 0)
            {
              ret = {};
              return;
            }
          }
          ret = std::span<Process::Outlet*>(
              const_cast<Process::Outlet**>(this->m_outlets.data()) + model_index,
              num_ports);
        }
        else
        {
          if constexpr(avnd::dynamic_ports_port<P>)
          {
            model_index += dynamic_ports.num_out_ports(avnd::field_index<Idx>{});
          }
          else
          {
            model_index += 1;
          }
        }
      });
    }

    return ret;
  }

  Process::Inlets::iterator avnd_input_idx_to_iterator(int index) const noexcept
  {
    int model_index = 0;

    // We have to adjust before accessing a port as there is the first "fake"
    // port if the processor takes audio by argument
    if constexpr(avnd::audio_argument_processor<Info>)
      model_index += 1;

    // The "messages" ports also go before
    model_index += avnd::messages_introspection<Info>::size;

    Process::Inlets::iterator ret;
    if constexpr(avnd::dynamic_ports_input_introspection<Info>::size == 0)
    {
      ret = const_cast<ProcessModel*>(this)->m_inlets.begin() + model_index;
    }
    else
    {
      avnd::input_introspection<Info>::for_all(
          [this, index, &model_index,
           &ret]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P> field) {
        if(Idx == index)
        {
          ret = const_cast<ProcessModel*>(this)->m_inlets.begin() + model_index;
        }
        else
        {
          if constexpr(avnd::dynamic_ports_port<P>)
          {
            model_index += dynamic_ports.num_in_ports(avnd::field_index<Idx>{});
          }
          else
          {
            model_index += 1;
          }
        }
      });
    }
    return ret;
  }

  Process::Outlets::iterator avnd_output_idx_to_iterator(int index) const noexcept
  {
    int model_index = 0;

    // We have to adjust before accessing a port as there is the first "fake"
    // port if the processor takes audio by argument
    if constexpr(avnd::audio_argument_processor<Info>)
      model_index += 1;

    // The "messages" ports also go before
    model_index += avnd::messages_introspection<Info>::size;

    Process::Outlets::iterator ret;
    if constexpr(avnd::dynamic_ports_output_introspection<Info>::size == 0)
    {
      ret = const_cast<ProcessModel*>(this)->m_outlets.begin() + model_index;
    }
    else
    {
      avnd::output_introspection<Info>::for_all(
          [this, index, &model_index,
           &ret]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P> field) {
        if(Idx == index)
        {
          ret = const_cast<ProcessModel*>(this)->m_outlets.begin() + model_index;
        }
        else
        {
          if constexpr(avnd::dynamic_ports_port<P>)
          {
            model_index += dynamic_ports.num_in_ports(avnd::field_index<Idx>{});
          }
          else
          {
            model_index += 1;
          }
        }
      });
    }
    return ret;
  }
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
