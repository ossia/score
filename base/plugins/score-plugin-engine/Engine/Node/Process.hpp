#pragma once
#include <ossia/dataflow/safe_nodes/node.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessMetadata.hpp>
#include <Process/ProcessFactory.hpp>

////////// METADATA ////////////
namespace Control
{
struct is_control {};
template<typename Info, typename = is_control>
class ControlProcess;
}
template <typename Info>
struct Metadata<PrettyName_k, Control::ControlProcess<Info>>
{
    static Q_DECL_RELAXED_CONSTEXPR auto get()
    {
      return Info::Metadata::prettyName;
    }
};
template <typename Info>
struct Metadata<Category_k, Control::ControlProcess<Info>>
{
    static Q_DECL_RELAXED_CONSTEXPR auto get()
    {
      return Info::Metadata::category;
    }
};
template <typename Info>
struct Metadata<Tags_k, Control::ControlProcess<Info>>
{
    static QStringList get()
    {
      QStringList lst;
      for(auto str : Info::Metadata::tags)
        lst.append(str);
      return lst;
    }
};
template <typename Info>
struct Metadata<Process::ProcessFlags_k, Control::ControlProcess<Info>>
{
    static Process::ProcessFlags get()
    {
      return Process::ProcessFlags::SupportsAll;
    }
};
template <typename Info>
struct Metadata<ObjectKey_k, Control::ControlProcess<Info>>
{
    static Q_DECL_RELAXED_CONSTEXPR auto get()
    {
      return Info::Metadata::objectKey;
    }
};
template <typename Info>
struct Metadata<ConcreteKey_k, Control::ControlProcess<Info>>
{
    static Q_DECL_RELAXED_CONSTEXPR UuidKey<Process::ProcessModel> get()
    {
      return Info::Metadata::uuid;
    }
};


namespace Control
{

struct PortSetup
{
    template<typename Node_T, typename T>
    static void init(T& self)
    {
      auto& ins = self.m_inlets;
      auto& outs = self.m_outlets;
      int inlet = 0;
      for(const auto& in : ossia::safe_nodes::get_ports<ossia::safe_nodes::audio_in, Node_T>{}())
      {
        auto p = new Process::Inlet(Id<Process::Port>(inlet++), &self);
        p->type = Process::PortType::Audio;
        p->setCustomData(QString::fromUtf8(in.name.data(), in.name.size()));
        ins.push_back(p);
      }
      for(const auto& in : ossia::safe_nodes::get_ports<ossia::safe_nodes::midi_in, Node_T>{}())
      {
        auto p = new Process::Inlet(Id<Process::Port>(inlet++), &self);
        p->type = Process::PortType::Midi;
        p->setCustomData(QString::fromUtf8(in.name.data(), in.name.size()));
        ins.push_back(p);
      }
      for(const auto& in : ossia::safe_nodes::get_ports<ossia::safe_nodes::value_in, Node_T>{}())
      {
        auto p = new Process::Inlet(Id<Process::Port>(inlet++), &self);
        p->type = Process::PortType::Message;
        p->setCustomData(QString::fromUtf8(in.name.data(), in.name.size()));
        ins.push_back(p);
      }
      for(const auto& in : ossia::safe_nodes::get_ports<ossia::safe_nodes::address_in, Node_T>{}())
      {
        auto p = new Process::Inlet(Id<Process::Port>(inlet++), &self);
        p->type = Process::PortType::Message;
        p->setCustomData(QString::fromUtf8(in.name.data(), in.name.size()));
        ins.push_back(p);
      }
      ossia::for_each_in_tuple(ossia::safe_nodes::get_controls<Node_T>{}(),
                               [&] (const auto& ctrl) {
        if(auto p = ctrl.create_inlet(Id<Process::Port>(inlet++), &self))
        {
          p->hidden = true;
          ins.push_back(p);
        }
      });

      int outlet = 0;
      for(const auto& out : ossia::safe_nodes::get_ports<ossia::safe_nodes::audio_out, Node_T>{}())
      {
        auto p = new Process::Outlet(Id<Process::Port>(outlet++), &self);
        p->type = Process::PortType::Audio;
        p->setCustomData(QString::fromUtf8(out.name.data(), out.name.size()));
        if(outlet == 1)
          p->setPropagate(true);
        outs.push_back(p);
      }
      for(const auto& out : ossia::safe_nodes::get_ports<ossia::safe_nodes::midi_out, Node_T>{}())
      {
        auto p = new Process::Outlet(Id<Process::Port>(outlet++), &self);
        p->type = Process::PortType::Midi;
        p->setCustomData(QString::fromUtf8(out.name.data(), out.name.size()));
        outs.push_back(p);
      }
      for(const auto& out : ossia::safe_nodes::get_ports<ossia::safe_nodes::value_out, Node_T>{}())
      {
        auto p = new Process::Outlet(Id<Process::Port>(outlet++), &self);
        p->type = Process::PortType::Message;
        p->setCustomData(QString::fromUtf8(out.name.data(), out.name.size()));
        outs.push_back(p);
      }
    }

};

template<typename Info, typename>
class ControlProcess final: public Process::ProcessModel
{
    SCORE_SERIALIZE_FRIENDS
    PROCESS_METADATA_IMPL(ControlProcess<Info>)
    friend struct TSerializer<DataStream, Control::ControlProcess<Info>>;
    friend struct TSerializer<JSONObject, Control::ControlProcess<Info>>;
    friend struct Control::PortSetup;

  public:
    ossia::value control(std::size_t i) const
    {
      static_assert(ossia::safe_nodes::info_functions<Info>::control_count != 0);
      constexpr auto start = ossia::safe_nodes::info_functions<Info>::control_start;

      return static_cast<Process::ControlInlet*>(m_inlets[start + i])->value();
    }

    void setControl(std::size_t i, ossia::value v)
    {
      static_assert(ossia::safe_nodes::info_functions<Info>::control_count != 0);
      constexpr auto start = ossia::safe_nodes::info_functions<Info>::control_start;

      static_cast<Process::ControlInlet*>(m_inlets[start + i])->setValue(std::move(v));
    }

    ControlProcess(
        const TimeVal& duration,
        const Id<Process::ProcessModel>& id,
        QObject* parent):
      Process::ProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    {
      metadata().setInstanceName(*this);

      Control::PortSetup::init<Info>(*this);
    }


    template<typename Impl>
    explicit ControlProcess(
        Impl& vis,
        QObject* parent) :
      Process::ProcessModel{vis, parent}
    {
      vis.writeTo(*this);
    }

    ~ControlProcess() override
    {

    }
};
}

template<typename Info>
struct is_custom_serialized<Control::ControlProcess<Info>>: std::true_type { };



template <template<typename, typename> class Model, typename Info>
struct TSerializer<DataStream, Model<Info, Control::is_control>>
{
    using model_type = Model<Info, Control::is_control>;
    static void
    readFrom(DataStream::Serializer& s, const model_type& obj)
    {
      using namespace Control;
      for (auto obj : obj.inlets())
      {
        s.stream() << *obj;
      }

      for (auto obj : obj.outlets())
      {
        s.stream() << *obj;
      }
      s.insertDelimiter();
    }

    static void writeTo(DataStream::Deserializer& s, model_type& obj)
    {
      using namespace Control;

      auto& pl = s.components.template interfaces<Process::PortFactoryList>();
      for(std::size_t i = 0; i < ossia::safe_nodes::info_functions<Info>::inlet_size; i++)
      {
        obj.m_inlets.push_back((Process::Inlet*)deserialize_interface(pl, s, &obj));
      }

      for(std::size_t i = 0; i < ossia::safe_nodes::info_functions<Info>::outlet_size; i++)
      {
        obj.m_outlets.push_back((Process::Outlet*)deserialize_interface(pl, s, &obj));
      }
      s.checkDelimiter();
    }
};

template <template<typename, typename> class Model, typename Info>
struct TSerializer<JSONObject, Model<Info, Control::is_control>>
{
    using model_type = Model<Info, Control::is_control>;
    static void
    readFrom(JSONObject::Serializer& s, const model_type& obj)
    {
      using namespace Control;
      s.obj["Inlets"] = toJsonArray(obj.inlets());
      s.obj["Outlets"] = toJsonArray(obj.outlets());
    }

    static void writeTo(JSONObject::Deserializer& s, model_type& obj)
    {
      using namespace Control;
      auto& pl = s.components.template interfaces<Process::PortFactoryList>();

      auto inlets = s.obj["Inlets"].toArray();
      auto outlets = s.obj["Outlets"].toArray();

      for(std::size_t i = 0; i < ossia::safe_nodes::info_functions<Info>::inlet_size; i++)
      {
        obj.m_inlets.push_back((Process::Inlet*)deserialize_interface(pl, JSONObjectWriter{inlets[i].toObject()}, &obj));
      }

      for(std::size_t i = 0; i < ossia::safe_nodes::info_functions<Info>::outlet_size; i++)
      {
        obj.m_outlets.push_back((Process::Outlet*)deserialize_interface(pl, JSONObjectWriter{outlets[i].toObject()}, &obj));
      }
    }
};

namespace score
{
template<typename Vis, typename Info>
void serialize_dyn_impl(Vis& v, const Control::ControlProcess<Info>& t)
{
  TSerializer<typename Vis::type, Control::ControlProcess<Info>>::readFrom(v, t);
}
}

