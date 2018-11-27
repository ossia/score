#pragma once
#include <Curve/Process/CurveProcessModel.hpp>
#include <Process/ProcessMetadata.hpp>
#include <Process/State/MessageNode.hpp>
#include <Process/State/ProcessStateDataInterface.hpp>
#include <State/Address.hpp>
#include <State/Message.hpp>
#include <State/Unit.hpp>
#include <State/Value.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <score_plugin_scenario_export.h>
#include <wobjectdefs.h>
namespace Interpolation
{
class ProcessModel;
}

PROCESS_METADATA(
    , Interpolation::ProcessModel, "aa569e11-03a9-4023-92c2-b590e88fec90",
    "Interpolation", "Interpolation", Process::ProcessCategory::Automation,
    "Automations", "Interpolate between two states", "ossia score",
    (QStringList{"Curve", "Automation"}), {},
    {std::vector<Process::PortType>{Process::PortType::Message}},
    Process::ProcessFlags::SupportsTemporal)
namespace Interpolation
{
class SCORE_PLUGIN_SCENARIO_EXPORT ProcessModel final
    : public Curve::CurveProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Interpolation::ProcessModel)

  W_OBJECT(ProcessModel)

public:
  ProcessModel(
      const TimeVal& duration, const Id<Process::ProcessModel>& id,
      QObject* parent);

  ~ProcessModel();

  template <typename Impl>
  ProcessModel(Impl& vis, QObject* parent)
      : CurveProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  State::AddressAccessor address() const;
  const State::Unit& sourceUnit() const;

  ossia::value start() const;
  ossia::value end() const;

  void setAddress(const ::State::AddressAccessor& arg);
  void setSourceUnit(const State::Unit&);
  void setStart(ossia::value arg);
  void setEnd(ossia::value arg);

  QString prettyName() const noexcept override;
  QString prettyValue(double x, double y) const noexcept override;

  bool tween() const
  {
    return m_tween;
  }
  void setTween(bool tween)
  {
    if (m_tween == tween)
      return;

    m_tween = tween;
    tweenChanged(tween);
  }

public:
  void addressChanged(const ::State::AddressAccessor& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, addressChanged, arg_1);
  void startChanged(const ossia::value& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, startChanged, arg_1);
  void endChanged(const ossia::value& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, endChanged, arg_1);
  void tweenChanged(bool tween)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, tweenChanged, tween);

private:
  //// ProcessModel ////
  void setDurationAndScale(const TimeVal& newDuration) noexcept override;
  void setDurationAndGrow(const TimeVal& newDuration) noexcept override;
  void setDurationAndShrink(const TimeVal& newDuration) noexcept override;
  bool contentHasDuration() const noexcept override;
  TimeVal contentDuration() const noexcept override;

  ::State::AddressAccessor m_address;
  State::Unit m_sourceUnit;

  ossia::value m_start{};
  ossia::value m_end{};

  bool m_tween = false;

  W_PROPERTY(bool, tween READ tween WRITE setTween NOTIFY tweenChanged)

  W_PROPERTY(ossia::value, end READ end WRITE setEnd NOTIFY endChanged)

  W_PROPERTY(ossia::value, start READ start WRITE setStart NOTIFY startChanged)

  W_PROPERTY(
      State::AddressAccessor,
      address READ address WRITE setAddress NOTIFY addressChanged)
};
}
