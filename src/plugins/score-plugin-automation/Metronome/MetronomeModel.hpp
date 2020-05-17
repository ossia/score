#pragma once
#include <Curve/Process/CurveProcessModel.hpp>
#include <Process/TimeValue.hpp>
#include <State/Address.hpp>

#include <score/model/Identifier.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/serialization/VisitorInterface.hpp>

#include <QObject>
#include <QString>

#include <Metronome/MetronomeProcessMetadata.hpp>
#include <score_plugin_automation_export.h>

#include <verdigris>

namespace Metronome
{
class SCORE_PLUGIN_AUTOMATION_EXPORT ProcessModel final : public Curve::CurveProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Metronome::ProcessModel)

  W_OBJECT(ProcessModel)

  // Min and max to scale the curve with at execution

public:
  ProcessModel(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent);
  ~ProcessModel() override;

  template <typename Impl>
  ProcessModel(Impl& vis, QObject* parent) : CurveProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  State::Address address() const;

  double min() const;
  double max() const;

  void setAddress(const State::Address& arg);
  void setMin(double arg);
  void setMax(double arg);

  QString prettyName() const noexcept override;
  QString prettyValue(double x, double y) const noexcept override;

  std::unique_ptr<Process::Outlet> outlet;

  void init();

public:
  void addressChanged(const State::Address& arg_1)
      E_SIGNAL(SCORE_PLUGIN_AUTOMATION_EXPORT, addressChanged, arg_1);
  void minChanged(double arg_1) E_SIGNAL(SCORE_PLUGIN_AUTOMATION_EXPORT, minChanged, arg_1);
  void maxChanged(double arg_1) E_SIGNAL(SCORE_PLUGIN_AUTOMATION_EXPORT, maxChanged, arg_1);

private:
  //// ProcessModel ////
  void setDurationAndScale(const TimeVal& newDuration) noexcept override;
  void setDurationAndGrow(const TimeVal& newDuration) noexcept override;
  void setDurationAndShrink(const TimeVal& newDuration) noexcept override;

  void setCurve_impl() override;

  double m_min{};
  double m_max{};

  W_PROPERTY(double, max READ max WRITE setMax NOTIFY maxChanged)

  W_PROPERTY(double, min READ min WRITE setMin NOTIFY minChanged)

  W_PROPERTY(State::Address, address READ address WRITE setAddress NOTIFY addressChanged)
};
}
