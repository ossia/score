// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurveProcessModel.hpp"

#include <Curve/Point/CurvePointModel.hpp>

#include <score/selection/Selection.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Curve::CurveProcessModel)
namespace Curve
{
CurveProcessModel::CurveProcessModel(
    TimeVal duration,
    const Id<Process::ProcessModel>& id,
    const QString& name,
    QObject* parent)
    : Process::ProcessModel(duration, id, name, parent)
{
}

CurveProcessModel::CurveProcessModel(DataStream::Deserializer& vis, QObject* p)
    : Process::ProcessModel(vis, p)
{
}

CurveProcessModel::CurveProcessModel(JSONObject::Deserializer& vis, QObject* p)
    : Process::ProcessModel(vis, p)
{
}

Model& CurveProcessModel::curve() const
{
  return *m_curve;
}

CurveProcessModel::~CurveProcessModel()
{
  identified_object_destroying(this);
}

Selection CurveProcessModel::selectableChildren() const noexcept
{
  Selection s;
  for (auto& segment : m_curve->segments())
    s.append(segment);
  for (auto& point : m_curve->points())
    s.append(point);
  return s;
}

Selection CurveProcessModel::selectedChildren() const noexcept
{
  return m_curve->selectedChildren();
}

void CurveProcessModel::setSelection(const Selection& s) const noexcept
{
  m_curve->setSelection(s);
}

void CurveProcessModel::setCurve(Model* newCurve)
{
  delete m_curve;
  m_curve = newCurve;

  setCurve_impl();

  connect(m_curve, &Curve::Model::changed, this, &CurveProcessModel::curveChanged);

  m_curve->changed();
}

TimeVal CurveProcessModel::contentDuration() const noexcept
{
  return TimeVal(duration().impl * double(std::max(1., m_curve->lastPointPos())));
}

void CurveProcessModel::setCurve_impl() { }
}
