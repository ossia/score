#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <State/Address.hpp>
#include <State/Unit.hpp>

#include <score/serialization/VisitorCommon.hpp>

#include <ossia/detail/flat_map.hpp>

#include <Color/GradientMetadata.hpp>
#include <score_plugin_automation_export.h>

#include <verdigris>

namespace Gradient
{
class SCORE_PLUGIN_AUTOMATION_EXPORT ProcessModel final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Gradient::ProcessModel)

  W_OBJECT(ProcessModel)

public:
  ProcessModel(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent);
  ~ProcessModel() override;

  template <typename Impl>
  ProcessModel(Impl& vis, QObject* parent) : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  void init();

  const State::AddressAccessor& address() const;

  bool tween() const;
  void setTween(bool tween);

  QString prettyName() const noexcept override;

  using gradient_colors = ossia::flat_map<double, QColor>;
  const gradient_colors& gradient() const;
  void setGradient(const gradient_colors& c);

  std::unique_ptr<Process::Outlet> outlet;

public:
  void tweenChanged(bool tween) E_SIGNAL(SCORE_PLUGIN_AUTOMATION_EXPORT, tweenChanged, tween)
  void gradientChanged() E_SIGNAL(SCORE_PLUGIN_AUTOMATION_EXPORT, gradientChanged)

  PROPERTY(bool, tween READ tween WRITE setTween NOTIFY tweenChanged)
private:
  //// ProcessModel ////
  void setDurationAndScale(const TimeVal& newDuration) noexcept override;
  void setDurationAndGrow(const TimeVal& newDuration) noexcept override;
  void setDurationAndShrink(const TimeVal& newDuration) noexcept override;

  TimeVal contentDuration() const noexcept override;

  ossia::flat_map<double, QColor> m_colors;

  bool m_tween = false;
};
}
