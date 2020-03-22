#pragma once

#include <Automation/AutomationProcessMetadata.hpp>
#include <Automation/State/AutomationState.hpp>
#include <Curve/Process/CurveProcessModel.hpp>
#include <Process/TimeValue.hpp>
#include <State/Address.hpp>
#include <State/Unit.hpp>

#include <score/serialization/VisitorInterface.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <QString>

#include <verdigris>

class DataStream;
class JSONObject;

namespace Process
{
class ProcessModel;
class Outlet;
}
class QObject;
#include <score/model/Identifier.hpp>

#include <score_plugin_automation_export.h>

namespace Automation
{
class SCORE_PLUGIN_AUTOMATION_EXPORT ProcessModel final
    : public Curve::CurveProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Automation::ProcessModel)

  W_OBJECT(ProcessModel)

  // Min and max to scale the curve with at execution

public:
  ProcessModel(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);
  ~ProcessModel() override;

  ProcessModel(DataStream::Deserializer& vis, QObject* parent);
  ProcessModel(JSONObject::Deserializer& vis, QObject* parent);

  const ::State::AddressAccessor& address() const;

  double min() const;
  double max() const;

  void setAddress(const ::State::AddressAccessor& arg);
  void setMin(double arg);
  void setMax(double arg);

  State::Unit unit() const;
  void setUnit(const State::Unit&);

  bool tween() const;
  void setTween(bool tween);

  QString prettyName() const noexcept override;
  QString prettyValue(double x, double y) const noexcept override;
  std::unique_ptr<Process::Outlet> outlet;

public:
  void addressChanged(const ::State::AddressAccessor& arg_1)
      E_SIGNAL(SCORE_PLUGIN_AUTOMATION_EXPORT, addressChanged, arg_1)
  void minChanged(double arg_1)
      E_SIGNAL(SCORE_PLUGIN_AUTOMATION_EXPORT, minChanged, arg_1)
  void maxChanged(double arg_1)
      E_SIGNAL(SCORE_PLUGIN_AUTOMATION_EXPORT, maxChanged, arg_1)
  void tweenChanged(bool tween)
      E_SIGNAL(SCORE_PLUGIN_AUTOMATION_EXPORT, tweenChanged, tween)
  void unitChanged(const State::Unit& arg_1)
      E_SIGNAL(SCORE_PLUGIN_AUTOMATION_EXPORT, unitChanged, arg_1)

  PROPERTY(State::Unit, unit READ unit WRITE setUnit NOTIFY unitChanged)
  PROPERTY(bool, tween READ tween WRITE setTween NOTIFY tweenChanged)
  PROPERTY(double, max READ max WRITE setMax NOTIFY maxChanged)
  PROPERTY(double, min READ min WRITE setMin NOTIFY minChanged)
  PROPERTY(
      ::State::AddressAccessor,
      address READ address WRITE setAddress NOTIFY addressChanged)

private:
  void init();

  //// ProcessModel ////
  void setDurationAndScale(const TimeVal& newDuration) noexcept override;
  void setDurationAndGrow(const TimeVal& newDuration) noexcept override;
  void setDurationAndShrink(const TimeVal& newDuration) noexcept override;

  void loadPreset(const Process::Preset& preset) override;
  Process::Preset savePreset() const noexcept override;

  /// States
  ProcessState* startStateData() const noexcept override;
  ProcessState* endStateData() const noexcept override;

  void setCurve_impl() override;

  ProcessState* m_startState{};
  ProcessState* m_endState{};
  bool m_tween = false;
};
}
