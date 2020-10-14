#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessMetadata.hpp>

#include <score/application/ApplicationComponents.hpp>
#include <score/model/EntityMapSerialization.hpp>
#include <score/plugins/DeserializeKnownSubType.hpp>
#include <score/plugins/SerializableHelpers.hpp>

#include <ossia/dataflow/safe_nodes/node.hpp>
#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/for_each.hpp>

////////// METADATA ////////////
namespace Control
{
struct is_control
{
};
template <typename Info, typename = is_control>
class ControlProcess;
}
template <typename Info>
struct Metadata<PrettyName_k, Control::ControlProcess<Info>>
{
  static Q_DECL_RELAXED_CONSTEXPR auto get() { return Info::Metadata::prettyName; }
};
template <typename Info>
struct Metadata<Category_k, Control::ControlProcess<Info>>
{
  static Q_DECL_RELAXED_CONSTEXPR auto get() { return Info::Metadata::category; }
};
template <typename Info>
struct Metadata<Tags_k, Control::ControlProcess<Info>>
{
  static QStringList get()
  {
    QStringList lst;
    for (auto str : Info::Metadata::tags)
      lst.append(str);
    return lst;
  }
};

template <typename Info>
struct Metadata<Process::Descriptor_k, Control::ControlProcess<Info>>
{
  static std::vector<Process::PortType> inletDescription()
  {
    std::vector<Process::PortType> port;
    for (std::size_t i = 0; i < std::size(Info::Metadata::audio_ins); i++)
    {
      port.push_back(Process::PortType::Audio);
    }
    for (std::size_t i = 0; i < std::size(Info::Metadata::midi_ins); i++)
    {
      port.push_back(Process::PortType::Midi);
    }
    for (std::size_t i = 0; i < std::size(Info::Metadata::value_ins); i++)
    {
      port.push_back(Process::PortType::Message);
    }
    for (std::size_t i = 0; i < std::size(Info::Metadata::address_ins); i++)
    {
      port.push_back(Process::PortType::Message);
    }
    for (std::size_t i = 0; i < std::tuple_size_v<decltype(Info::Metadata::controls)>; i++)
      port.push_back(Process::PortType::Message);
    return port;
  }
  static std::vector<Process::PortType> outletDescription()
  {
    std::vector<Process::PortType> port;
    for (std::size_t i = 0; i < std::size(Info::Metadata::audio_outs); i++)
    {
      port.push_back(Process::PortType::Audio);
    }
    for (std::size_t i = 0; i < std::size(Info::Metadata::midi_outs); i++)
    {
      port.push_back(Process::PortType::Midi);
    }
    for (std::size_t i = 0; i < std::size(Info::Metadata::value_outs); i++)
    {
      port.push_back(Process::PortType::Message);
    }
    return port;
  }
  static Process::Descriptor get()
  {
    static Process::Descriptor desc{
        Info::Metadata::prettyName,
        Info::Metadata::kind,
        Info::Metadata::category,
        Info::Metadata::description,
        Info::Metadata::author,
        Metadata<Tags_k, Control::ControlProcess<Info>>::get(),
        inletDescription(),
        outletDescription()};
    return desc;
  }
};
template <typename Info>
struct Metadata<Process::ProcessFlags_k, Control::ControlProcess<Info>>
{
  static Process::ProcessFlags get() noexcept { return Info::Metadata::flags; }
};
template <typename Info>
struct Metadata<ObjectKey_k, Control::ControlProcess<Info>>
{
  static Q_DECL_RELAXED_CONSTEXPR auto get() noexcept { return Info::Metadata::objectKey; }
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
  template <typename Node_T, typename T>
  static void init(T& self)
  {
    auto& ins = self.m_inlets;
    auto& outs = self.m_outlets;
    int inlet = 0;
    for (const auto& in : Node_T::Metadata::audio_ins)
    {
      auto p = new Process::AudioInlet(Id<Process::Port>(inlet++), &self);
      p->setCustomData(QString::fromUtf8(in.name.data(), in.name.size()));
      ins.push_back(p);
    }
    for (const auto& in : Node_T::Metadata::midi_ins)
    {
      auto p = new Process::MidiInlet(Id<Process::Port>(inlet++), &self);
      p->setCustomData(QString::fromUtf8(in.name.data(), in.name.size()));
      ins.push_back(p);
    }
    for (const auto& in : Node_T::Metadata::value_ins)
    {
      auto p = new Process::ValueInlet(Id<Process::Port>(inlet++), &self);
      p->setCustomData(QString::fromUtf8(in.name.data(), in.name.size()));
      ins.push_back(p);
    }
    for (const auto& in : Node_T::Metadata::address_ins)
    {
      auto p = new Process::ValueInlet(Id<Process::Port>(inlet++), &self);
      p->setCustomData(QString::fromUtf8(in.name.data(), in.name.size()));
      ins.push_back(p);
    }
    ossia::for_each_in_tuple(Node_T::Metadata::controls, [&](const auto& ctrl) {
      if (auto p = ctrl.create_inlet(Id<Process::Port>(inlet++), &self))
      {
        p->hidden = true;
        ins.push_back(p);
      }
    });

    int outlet = 0;
    for (const auto& out : Node_T::Metadata::audio_outs)
    {
      auto p = new Process::AudioOutlet(Id<Process::Port>(outlet++), &self);
      p->setCustomData(QString::fromUtf8(out.name.data(), out.name.size()));
      if (outlet == 1)
        p->setPropagate(true);
      outs.push_back(p);
    }
    for (const auto& out : Node_T::Metadata::midi_outs)
    {
      auto p = new Process::MidiOutlet(Id<Process::Port>(outlet++), &self);
      p->setCustomData(QString::fromUtf8(out.name.data(), out.name.size()));
      outs.push_back(p);
    }
    for (const auto& out : Node_T::Metadata::value_outs)
    {
      auto p = new Process::ValueOutlet(Id<Process::Port>(outlet++), &self);
      p->setCustomData(QString::fromUtf8(out.name.data(), out.name.size()));
      outs.push_back(p);
    }
    ossia::for_each_in_tuple(Node_T::Metadata::control_outs, [&](const auto& ctrl) {
      if (auto p = ctrl.create_outlet(Id<Process::Port>(outlet++), &self))
      {
        p->hidden = true;
        outs.push_back(p);
      }
    });
  }

  template <typename Node_T, typename T>
  static void load(DataStream::Deserializer& s, T& self)
  {
    auto& ins = self.m_inlets;
    auto& outs = self.m_outlets;
    for ([[maybe_unused]] const auto& in : Node_T::Metadata::audio_ins)
    {
      ins.push_back(deserialize_known_interface<Process::AudioInlet>(s, &self));
    }
    for ([[maybe_unused]] const auto& in : Node_T::Metadata::midi_ins)
    {
      ins.push_back(deserialize_known_interface<Process::MidiInlet>(s, &self));
    }
    for ([[maybe_unused]] const auto& in : Node_T::Metadata::value_ins)
    {
      ins.push_back(deserialize_known_interface<Process::ValueInlet>(s, &self));
    }
    for ([[maybe_unused]] const auto& in : Node_T::Metadata::address_ins)
    {
      ins.push_back(deserialize_known_interface<Process::ValueInlet>(s, &self));
    }
    ossia::for_each_in_tuple(Node_T::Metadata::controls, [&](const auto& ctrl) {
      if (auto p = ctrl.create_inlet(s, &self))
      {
        p->hidden = true;
        ins.push_back(p);
      }
    });

    for ([[maybe_unused]] const auto& out : Node_T::Metadata::audio_outs)
    {
      outs.push_back(deserialize_known_interface<Process::AudioOutlet>(s, &self));
    }
    for ([[maybe_unused]] const auto& out : Node_T::Metadata::midi_outs)
    {
      outs.push_back(deserialize_known_interface<Process::MidiOutlet>(s, &self));
    }
    for ([[maybe_unused]] const auto& out : Node_T::Metadata::value_outs)
    {
      outs.push_back(deserialize_known_interface<Process::ValueOutlet>(s, &self));
    }
    ossia::for_each_in_tuple(Node_T::Metadata::control_outs, [&](const auto& ctrl) {
      if (auto p = ctrl.create_outlet(s, &self))
      {
        p->hidden = true;
        outs.push_back(p);
      }
    });
  }

  template <typename Node_T, typename T>
  static void load(
      const rapidjson::Value::ConstArray& inlets,
      const rapidjson::Value::ConstArray& outlets,
      T& self)
  {
    auto& ins = self.m_inlets;
    auto& outs = self.m_outlets;
    int inlet = 0;
    for ([[maybe_unused]] const auto& in : Node_T::Metadata::audio_ins)
    {
      ins.push_back(
          deserialize_known_interface<Process::AudioInlet>(JSONWriter{inlets[inlet++]}, &self));
    }
    for ([[maybe_unused]] const auto& in : Node_T::Metadata::midi_ins)
    {
      ins.push_back(
          deserialize_known_interface<Process::MidiInlet>(JSONWriter{inlets[inlet++]}, &self));
    }
    for ([[maybe_unused]] const auto& in : Node_T::Metadata::value_ins)
    {
      ins.push_back(
          deserialize_known_interface<Process::ValueInlet>(JSONWriter{inlets[inlet++]}, &self));
    }
    for ([[maybe_unused]] const auto& in : Node_T::Metadata::address_ins)
    {
      ins.push_back(
          deserialize_known_interface<Process::ValueInlet>(JSONWriter{inlets[inlet++]}, &self));
    }
    ossia::for_each_in_tuple(Node_T::Metadata::controls, [&](const auto& ctrl) {
      if (auto p = ctrl.create_inlet(JSONWriter{inlets[inlet++]}, &self))
      {
        p->hidden = true;
        ins.push_back(p);
      }
    });

    int outlet = 0;
    for ([[maybe_unused]] const auto& out : Node_T::Metadata::audio_outs)
    {
      outs.push_back(
          deserialize_known_interface<Process::AudioOutlet>(JSONWriter{outlets[outlet++]}, &self));
    }
    for ([[maybe_unused]] const auto& out : Node_T::Metadata::midi_outs)
    {
      outs.push_back(
          deserialize_known_interface<Process::MidiOutlet>(JSONWriter{outlets[outlet++]}, &self));
    }
    for ([[maybe_unused]] const auto& out : Node_T::Metadata::value_outs)
    {
      outs.push_back(
          deserialize_known_interface<Process::ValueOutlet>(JSONWriter{outlets[outlet++]}, &self));
    }
    ossia::for_each_in_tuple(Node_T::Metadata::control_outs, [&](const auto& ctrl) {
      if (auto p = ctrl.create_outlet(JSONWriter{outlets[outlet++]}, &self))
      {
        p->hidden = true;
        outs.push_back(p);
      }
    });
  }
};

template <typename Info, typename>
class ControlProcess final : public Process::ProcessModel
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

  ossia::value controlOut(std::size_t i) const
  {
    static_assert(ossia::safe_nodes::info_functions<Info>::control_out_count != 0);
    constexpr auto start = ossia::safe_nodes::info_functions<Info>::control_out_start;

    return static_cast<Process::ControlOutlet*>(m_outlets[start + i])->value();
  }

  void setControlOut(std::size_t i, ossia::value v)
  {
    static_assert(ossia::safe_nodes::info_functions<Info>::control_out_count != 0);
    constexpr auto start = ossia::safe_nodes::info_functions<Info>::control_out_start;

    static_cast<Process::ControlOutlet*>(m_outlets[start + i])->setValue(std::move(v));
  }

  ControlProcess(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent)
      : Process::ProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
  {
    metadata().setInstanceName(*this);

    Control::PortSetup::init<Info>(*this);
  }

  template <typename Impl>
  explicit ControlProcess(Impl& vis, QObject* parent) : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  ~ControlProcess() override { }
};
}

template <typename Info>
struct is_custom_serialized<Control::ControlProcess<Info>> : std::true_type
{
};

template <template <typename, typename> class Model, typename Info>
struct TSerializer<DataStream, Model<Info, Control::is_control>>
{
  using model_type = Model<Info, Control::is_control>;
  static void readFrom(DataStream::Serializer& s, const model_type& obj)
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

    Control::PortSetup::load<Info>(s, obj);
    s.checkDelimiter();
  }
};

template <template <typename, typename> class Model, typename Info>
struct TSerializer<JSONObject, Model<Info, Control::is_control>>
{
  using model_type = Model<Info, Control::is_control>;
  static void readFrom(JSONObject::Serializer& s, const model_type& obj)
  {
    using namespace Control;
    Process::readPorts(s, obj.inlets(), obj.outlets());
  }

  static void writeTo(JSONObject::Deserializer& s, model_type& obj)
  {
    using namespace Control;

    const auto& inlets = s.obj["Inlets"].toArray();
    const auto& outlets = s.obj["Outlets"].toArray();

    Control::PortSetup::load<Info>(inlets, outlets, obj);
  }
};

namespace score
{
template <typename Vis, typename Info>
void serialize_dyn_impl(Vis& v, const Control::ControlProcess<Info>& t)
{
  TSerializer<typename Vis::type, Control::ControlProcess<Info>>::readFrom(v, t);
}
}
