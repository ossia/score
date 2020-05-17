#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

namespace Process
{
class MinMaxFloatOutlet;
}

UUID_METADATA(
    SCORE_LIB_PROCESS_EXPORT,
    Process::Port,
    Process::MinMaxFloatOutlet,
    "047e4cc2-4d99-4e8b-bf98-206018d02274")
namespace Process
{
class SCORE_LIB_PROCESS_EXPORT MinMaxFloatOutlet : public ValueOutlet
{
  W_OBJECT(MinMaxFloatOutlet)

  SCORE_SERIALIZE_FRIENDS
public:
  MODEL_METADATA_IMPL_HPP(MinMaxFloatOutlet)
  MinMaxFloatOutlet() = delete;
  ~MinMaxFloatOutlet() override;
  MinMaxFloatOutlet(const MinMaxFloatOutlet&) = delete;
  MinMaxFloatOutlet(Id<Process::Port> c, QObject* parent);

  MinMaxFloatOutlet(DataStream::Deserializer& vis, QObject* parent);
  MinMaxFloatOutlet(JSONObject::Deserializer& vis, QObject* parent);
  MinMaxFloatOutlet(DataStream::Deserializer&& vis, QObject* parent);
  MinMaxFloatOutlet(JSONObject::Deserializer&& vis, QObject* parent);

  void forChildInlets(const smallfun::function<void(Inlet&)>& f) const noexcept override;
  void mapExecution(ossia::outlet& e, const smallfun::function<void(Inlet&, ossia::inlet&)>& f)
      const noexcept override;

  VIRTUAL_CONSTEXPR PortType type() const noexcept override { return Process::PortType::Message; }

  std::unique_ptr<Process::FloatSlider> minInlet;
  std::unique_ptr<Process::FloatSlider> maxInlet;
};
}
