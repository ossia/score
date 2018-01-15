#pragma once
#include <Automation/Metronome/MetronomeProcessMetadata.hpp>
#include <Curve/Process/CurveProcessModel.hpp>
#include <QByteArray>
#include <QString>
#include <QObject>
#include <State/Address.hpp>

#include <Process/TimeValue.hpp>
#include <score/serialization/VisitorInterface.hpp>

#include <score/model/Identifier.hpp>
#include <score_plugin_automation_export.h>

namespace Metronome
{
class SCORE_PLUGIN_AUTOMATION_EXPORT ProcessModel final
    : public Curve::CurveProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Metronome::ProcessModel)

  Q_OBJECT
  Q_PROPERTY(State::Address address READ address WRITE setAddress
                 NOTIFY addressChanged)
  // Min and max to scale the curve with at execution
  Q_PROPERTY(double min READ min WRITE setMin NOTIFY minChanged)
  Q_PROPERTY(double max READ max WRITE setMax NOTIFY maxChanged)

public:
  ProcessModel(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);
  ~ProcessModel();

  template <typename Impl>
  ProcessModel(Impl& vis, QObject* parent)
      : CurveProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  State::Address address() const;

  double min() const;
  double max() const;

  void setAddress(const State::Address& arg);
  void setMin(double arg);
  void setMax(double arg);

  QString prettyName() const override;

Q_SIGNALS:
  void addressChanged(const State::Address&);
  void minChanged(double);
  void maxChanged(double);

private:
  //// ProcessModel ////
  void setDurationAndScale(const TimeVal& newDuration) override;
  void setDurationAndGrow(const TimeVal& newDuration) override;
  void setDurationAndShrink(const TimeVal& newDuration) override;

  bool contentHasDuration() const override;
  TimeVal contentDuration() const override;

  void setCurve_impl() override;
  State::Address m_address;

  double m_min{};
  double m_max{};
};
}
