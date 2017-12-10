#pragma once
#include <Engine/Node/Node.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessMetadata.hpp>
#include <Process/ProcessFactory.hpp>

////////// METADATA ////////////
namespace Process
{
template<typename Info>
class ControlProcess;
}
template <typename Info>
struct Metadata<PrettyName_k, Process::ControlProcess<Info>>
{
    static Q_DECL_RELAXED_CONSTEXPR auto get()
    {
      return Info::Metadata::prettyName;
    }
};
template <typename Info>
struct Metadata<Category_k, Process::ControlProcess<Info>>
{
    static Q_DECL_RELAXED_CONSTEXPR auto get()
    {
      return Info::Metadata::category;
    }
};
template <typename Info>
struct Metadata<Tags_k, Process::ControlProcess<Info>>
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
struct Metadata<ObjectKey_k, Process::ControlProcess<Info>>
{
    static Q_DECL_RELAXED_CONSTEXPR auto get()
    {
      return Info::Metadata::objectKey;
    }
};
template <typename Info>
struct Metadata<ConcreteKey_k, Process::ControlProcess<Info>>
{
    static Q_DECL_RELAXED_CONSTEXPR UuidKey<Process::ProcessModel> get()
    {
      return Info::Metadata::uuid;
    }
};


namespace Process
{
template<typename Info>
class ControlProcess final: public Process::ProcessModel
{
    SCORE_SERIALIZE_FRIENDS
    PROCESS_METADATA_IMPL(ControlProcess<Info>)
    friend struct TSerializer<DataStream, Process::ControlProcess<Info>>;
    friend struct TSerializer<JSONObject, Process::ControlProcess<Info>>;
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

      return static_cast<ControlInlet*>(m_inlets[start + i])->value();
    }

    void setControl(std::size_t i, ossia::value v)
    {
      static_assert(InfoFunctions<Info>::control_count != 0);
      constexpr auto start = InfoFunctions<Info>::control_start;

      static_cast<ControlInlet*>(m_inlets[start + i])->setValue(std::move(v));
    }

    ControlProcess(
        const TimeVal& duration,
        const Id<Process::ProcessModel>& id,
        QObject* parent):
      Process::ProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    {
      metadata().setInstanceName(*this);

      setup_ports();
    }

    const auto& test()
    {
      return Info::info;
    }
    auto setup_ports()
    {
      int inlet = 0;
      for(const auto& in : get_ports<AudioInInfo>(Info::info))
      {
        auto p = new Process::Inlet(Id<Process::Port>(inlet++), this);
        p->type = Process::PortType::Audio;
        p->setCustomData(in.name);
        m_inlets.push_back(p);
      }
      for(const auto& in : get_ports<MidiInInfo>(Info::info))
      {
        auto p = new Process::Inlet(Id<Process::Port>(inlet++), this);
        p->type = Process::PortType::Midi;
        p->setCustomData(in.name);
        m_inlets.push_back(p);
      }
      for(const auto& in : get_ports<ValueInInfo>(Info::info))
      {
        auto p = new Process::Inlet(Id<Process::Port>(inlet++), this);
        p->type = Process::PortType::Message;
        p->setCustomData(in.name);
        m_inlets.push_back(p);
      }
      ossia::for_each_in_tuple(get_controls(Info::info),
                               [&] (const auto& ctrl) {
        if(auto p = ctrl.create_inlet(Id<Process::Port>(inlet++), this))
        {
          p->hidden = true;
          m_inlets.push_back(p);
        }
      });

      int outlet = 0;
      for(const auto& out : get_ports<AudioOutInfo>(Info::info))
      {
        auto p = new Process::Outlet(Id<Process::Port>(outlet++), this);
        p->type = Process::PortType::Audio;
        p->setCustomData(out.name);
        if(outlet == 0)
          p->setPropagate(true);
        m_outlets.push_back(p);
      }
      for(const auto& out : get_ports<MidiOutInfo>(Info::info))
      {
        auto p = new Process::Outlet(Id<Process::Port>(outlet++), this);
        p->type = Process::PortType::Midi;
        p->setCustomData(out.name);
        m_outlets.push_back(p);
      }
      for(const auto& out : get_ports<ValueOutInfo>(Info::info))
      {
        auto p = new Process::Outlet(Id<Process::Port>(outlet++), this);
        p->type = Process::PortType::Message;
        p->setCustomData(out.name);
        m_outlets.push_back(p);
      }
    }

    ControlProcess(
        const ControlProcess& source,
        const Id<Process::ProcessModel>& id,
        QObject* parent):
      Process::ProcessModel{
        source,
        id,
        Metadata<ObjectKey_k, ProcessModel>::get(),
        parent}
    {
      metadata().setInstanceName(*this);

      for(std::size_t i = 0; i < InfoFunctions<Info>::control_start; i++)
      {
        m_inlets.push_back(new Process::Inlet(source.m_inlets[i]->id(), *source.m_inlets[i], this));
      }
      for(auto i = InfoFunctions<Info>::control_start; i < InfoFunctions<Info>::inlet_size; i++)
      {
        m_inlets.push_back(new Process::ControlInlet(source.m_inlets[i]->id(), *source.m_inlets[i], this));
      }

      for(std::size_t i = 0; i < InfoFunctions<Info>::outlet_size; i++)
      {
        m_outlets.push_back(new Process::Outlet(source.m_outlets[i]->id(), *source.m_outlets[i], this));
      }
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
struct is_custom_serialized<Process::ControlProcess<Info>>: std::true_type { };


template <typename Info>
struct TSerializer<DataStream, Process::ControlProcess<Info>>
{
    static void
    readFrom(DataStream::Serializer& s, const Process::ControlProcess<Info>& obj)
    {
      using namespace Process;
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
    }

    static void writeTo(DataStream::Deserializer& s, Process::ControlProcess<Info>& obj)
    {
      using namespace Process;

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
    }
};

template <typename Info>
struct TSerializer<JSONObject, Process::ControlProcess<Info>>
{
    static void
    readFrom(JSONObject::Serializer& s, const Process::ControlProcess<Info>& obj)
    {
      using namespace Process;
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

    static void writeTo(JSONObject::Deserializer& s, Process::ControlProcess<Info>& obj)
    {
      using namespace Process;

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
void serialize_dyn_impl(Vis& v, const Process::ControlProcess<Info>& t)
{
  TSerializer<typename Vis::type, Process::ControlProcess<Info>>::readFrom(v, t);
}
}

