#pragma once

#include <Automation/AutomationProcessMetadata.hpp>
#include <Automation/State/AutomationState.hpp>
#include <Curve/Process/CurveProcessModel.hpp>
#include <QByteArray>
#include <QString>
#include <State/Address.hpp>

#include <Process/TimeValue.hpp>
#include <State/Unit.hpp>
#include <score/serialization/VisitorInterface.hpp>

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

  Q_OBJECT
  Q_PROPERTY(::State::AddressAccessor address READ address WRITE setAddress NOTIFY addressChanged)
  // Min and max to scale the curve with at execution
  Q_PROPERTY(double min READ min WRITE setMin NOTIFY minChanged)
  Q_PROPERTY(double max READ max WRITE setMax NOTIFY maxChanged)
  Q_PROPERTY(bool tween READ tween WRITE setTween NOTIFY tweenChanged)
  Q_PROPERTY(State::Unit unit READ unit WRITE setUnit NOTIFY unitChanged)

public:
  ProcessModel(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);
  ~ProcessModel();

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

  bool tween() const
  {
    return m_tween;
  }
  void setTween(bool tween)
  {
    if (m_tween == tween)
      return;

    m_tween = tween;
    emit tweenChanged(tween);
  }

  QString prettyName() const override;

  std::unique_ptr<Process::Outlet> outlet;
signals:
  void addressChanged(const ::State::AddressAccessor&);
  void minChanged(double);
  void maxChanged(double);
  void tweenChanged(bool tween);
  void unitChanged(const State::Unit&);

private:
  void init();

  //// ProcessModel ////
  Process::Inlets inlets() const override;
  Process::Outlets outlets() const override;
  void setDurationAndScale(const TimeVal& newDuration) override;
  void setDurationAndGrow(const TimeVal& newDuration) override;
  void setDurationAndShrink(const TimeVal& newDuration) override;

  bool contentHasDuration() const override;
  TimeVal contentDuration() const override;

  /// States
  ProcessState* startStateData() const override;
  ProcessState* endStateData() const override;

  void setCurve_impl() override;

  double m_min{};
  double m_max{};

  ProcessState* m_startState{};
  ProcessState* m_endState{};
  bool m_tween = false;
};

}
