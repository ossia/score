#pragma once
#include <Curve/CurveModel.hpp>
#include <Process/Process.hpp>

#include <score_plugin_curve_export.h>

namespace Curve
{
class SCORE_PLUGIN_CURVE_EXPORT CurveProcessModel : public Process::ProcessModel
{
  W_OBJECT(CurveProcessModel)
public:
  CurveProcessModel(
      TimeVal duration,
      const Id<ProcessModel>& id,
      const QString& name,
      QObject* parent);

  CurveProcessModel(DataStream::Deserializer& vis, QObject* p);
  CurveProcessModel(JSONObject::Deserializer& vis, QObject* p);

  Model& curve() const;

  ~CurveProcessModel() override;

  Selection selectableChildren() const noexcept override;
  Selection selectedChildren() const noexcept override;
  void setSelection(const Selection& s) const noexcept override;

  virtual QString prettyValue(double x, double y) const noexcept = 0;

  void curveChanged() E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, curveChanged)

protected:
  void setCurve(Model* newCurve);
  TimeVal contentDuration() const noexcept override;
  virtual void setCurve_impl();

  Model* m_curve{};
};
}
