#pragma once
#include <Engine/Node/Node.hpp>
#include <Process/Dataflow/Port.hpp>
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
    template<typename Info, typename T>
    static void init(T& self)
    {
      auto& ins = self.m_inlets;
      auto& outs = self.m_outlets;
      int inlet = 0;
      for(const auto& in : get_ports<AudioInInfo>(Info::info))
      {
        auto p = new Process::Inlet(Id<Process::Port>(inlet++), &self);
        p->type = Process::PortType::Audio;
        p->setCustomData(in.name);
        ins.push_back(p);
      }
      for(const auto& in : get_ports<MidiInInfo>(Info::info))
      {
        auto p = new Process::Inlet(Id<Process::Port>(inlet++), &self);
        p->type = Process::PortType::Midi;
        p->setCustomData(in.name);
        ins.push_back(p);
      }
      for(const auto& in : get_ports<ValueInInfo>(Info::info))
      {
        auto p = new Process::Inlet(Id<Process::Port>(inlet++), &self);
        p->type = Process::PortType::Message;
        p->setCustomData(in.name);
        ins.push_back(p);
      }
      for(const auto& in : get_ports<AddressInInfo>(Info::info))
      {
        auto p = new Process::Inlet(Id<Process::Port>(inlet++), &self);
        p->type = Process::PortType::Message;
        p->setCustomData(in.name);
        ins.push_back(p);
      }
      ossia::for_each_in_tuple(get_controls(Info::info),
                               [&] (const auto& ctrl) {
        if(auto p = ctrl.create_inlet(Id<Process::Port>(inlet++), &self))
        {
          p->hidden = true;
          ins.push_back(p);
        }
      });

      int outlet = 0;
      for(const auto& out : get_ports<AudioOutInfo>(Info::info))
      {
        auto p = new Process::Outlet(Id<Process::Port>(outlet++), &self);
        p->type = Process::PortType::Audio;
        p->setCustomData(out.name);
        if(outlet == 0)
          p->setPropagate(true);
        outs.push_back(p);
      }
      for(const auto& out : get_ports<MidiOutInfo>(Info::info))
      {
        auto p = new Process::Outlet(Id<Process::Port>(outlet++), &self);
        p->type = Process::PortType::Midi;
        p->setCustomData(out.name);
        outs.push_back(p);
      }
      for(const auto& out : get_ports<ValueOutInfo>(Info::info))
      {
        auto p = new Process::Outlet(Id<Process::Port>(outlet++), &self);
        p->type = Process::PortType::Message;
        p->setCustomData(out.name);
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
    Process::Inlets m_inlets;
    Process::Outlets m_outlets;

    Process::Inlets inlets() const final override
    {
      return m_inlets;
    }

    Process::Outlets outlets() const final override
    {
      return m_outlets;
    }

  public:

    const Process::Inlets& inlets_ref() const { return m_inlets; }
    const Process::Outlets& outlets_ref() const { return m_outlets; }

    ossia::value control(std::size_t i) const
    {
      static_assert(InfoFunctions<Info>::control_count != 0);
      constexpr auto start = InfoFunctions<Info>::control_start;

      return static_cast<Process::ControlInlet*>(m_inlets[start + i])->value();
    }

    void setControl(std::size_t i, ossia::value v)
    {
      static_assert(InfoFunctions<Info>::control_count != 0);
      constexpr auto start = InfoFunctions<Info>::control_start;

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
      {
        for(std::size_t i = 0; i < InfoFunctions<Info>::control_start; i++)
        {
          s.stream() << *obj.inlets_ref()[i];
        }
        for(auto i = InfoFunctions<Info>::control_start; i < InfoFunctions<Info>::inlet_size; i++)
        {
          s.stream() << *static_cast<Process::ControlInlet*>(obj.inlets_ref()[i]);
        }
      }

      for (auto obj : obj.outlets_ref())
      {
        s.stream() << *obj;
      }
      s.insertDelimiter();
    }

    static void writeTo(DataStream::Deserializer& s, model_type& obj)
    {
      using namespace Control;

      for(std::size_t i = 0; i < InfoFunctions<Info>::control_start; i++)
      {
        obj.m_inlets.push_back(new Process::Inlet(s, &obj));
      }
      for(auto i = InfoFunctions<Info>::control_start; i < InfoFunctions<Info>::inlet_size; i++)
      {
        obj.m_inlets.push_back(new Process::ControlInlet(s, &obj));
      }

      for(std::size_t i = 0; i < InfoFunctions<Info>::outlet_size; i++)
      {
        obj.m_outlets.push_back(new Process::Outlet(s, &obj));
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
      {
        QJsonArray arr;
        for(std::size_t i = 0; i < InfoFunctions<Info>::control_start; i++)
        {
          arr.push_back(toJsonObject(*obj.inlets_ref()[i]));
        }
        for(auto i = InfoFunctions<Info>::control_start; i < InfoFunctions<Info>::inlet_size; i++)
        {
          arr.push_back(toJsonObject(*static_cast<Process::ControlInlet*>(obj.inlets_ref()[i])));
        }
        s.obj["Inlets"] = std::move(arr);
      }

      s.obj["Outlets"] = toJsonArray(obj.outlets_ref());
    }

    static void writeTo(JSONObject::Deserializer& s, model_type& obj)
    {
      using namespace Control;

      {
        auto inlets = s.obj["Inlets"].toArray();

        for(std::size_t i = 0; i < InfoFunctions<Info>::control_start; i++)
        {
          obj.m_inlets.push_back(new Process::Inlet(JSONObjectWriter{inlets[i].toObject()}, &obj));
        }
        for(auto i = InfoFunctions<Info>::control_start; i < InfoFunctions<Info>::inlet_size; i++)
        {
          obj.m_inlets.push_back(new Process::ControlInlet(JSONObjectWriter{inlets[i].toObject()}, &obj));
          SCORE_ASSERT(obj.m_inlets.back()->hidden);
        }
      }


      {
        auto outlets = s.obj["Outlets"].toArray();
        for(std::size_t i = 0; i < InfoFunctions<Info>::outlet_size; i++)
        {
          obj.m_outlets.push_back(new Process::Outlet(JSONObjectWriter{outlets[i].toObject()}, &obj));
        }
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

