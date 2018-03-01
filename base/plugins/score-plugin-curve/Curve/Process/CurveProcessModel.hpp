#pragma once
#include <Curve/CurveModel.hpp>
#include <Curve/Point/CurvePointModel.hpp>
#include <Process/Process.hpp>
#include <QString>

#include <Curve/Segment/CurveSegmentModel.hpp>
#include <score/selection/Selection.hpp>
#include <score_plugin_curve_export.h>

namespace Curve
{
class SCORE_PLUGIN_CURVE_EXPORT CurveProcessModel
    : public Process::ProcessModel
{
  Q_OBJECT
public:
  CurveProcessModel(
      TimeVal duration,
      const Id<ProcessModel>& id,
      const QString& name,
      QObject* parent)
      : Process::ProcessModel(duration, id, name, parent)
  {
  }

  CurveProcessModel(DataStream::Deserializer& vis, QObject* p)
      : Process::ProcessModel(vis, p)
  {
  }

  CurveProcessModel(JSONObject::Deserializer& vis, QObject* p)
      : Process::ProcessModel(vis, p)
  {
  }

  Model& curve() const
  {
    return *m_curve;
  }

  ~CurveProcessModel() override;

  void startExecution() final override
  {
  }

  void stopExecution() final override
  {
  }

  void reset() final override
  {
  }

  Selection selectableChildren() const final override
  {
    Selection s;
    for (auto& segment : m_curve->segments())
      s.append(&segment);
    for (auto& point : m_curve->points())
      s.append(point);
    return s;
  }

  Selection selectedChildren() const final override
  {
    return m_curve->selectedChildren();
  }

  void setSelection(const Selection& s) const final override
  {
    m_curve->setSelection(s);
  }

Q_SIGNALS:
  void curveChanged();

protected:
  void setCurve(Model* newCurve)
  {
    delete m_curve;
    m_curve = newCurve;

    setCurve_impl();

    connect(m_curve, &Curve::Model::changed, this, &CurveProcessModel::curveChanged);
    m_curve->changed();
  }

  virtual void setCurve_impl()
  {
  }

  Model* m_curve{};
};
}
