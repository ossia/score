// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "UpdateCurve.hpp"

#include <Curve/CurveModel.hpp>
#include <Curve/Palette/CommandObjects/CurveCommandObjectBase.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>

#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/Debug.hpp>

#include <cmath>
namespace Curve
{
UpdateCurve::UpdateCurve(const Model& model, std::vector<SegmentData>&& segments)
    : m_model{std::move(model)}
    , m_oldCurveData{model.toCurveData()}
    , m_newCurveData{std::move(segments)}
{
  checkValidity(m_oldCurveData);
  checkValidity(m_newCurveData);

  auto on_invalid_curve = [&]() {
    qDebug() << "Curve error !";
    for(auto elt : m_newCurveData)
    {
      QString log = QStringLiteral("id: %1 [%2; %3] prev: %4 following: %5")
                        .arg(elt.id.val())
                        .arg(elt.start.x())
                        .arg(elt.end.x())
                        .arg(elt.previous.value_or(Id<Curve::SegmentModel>{-1}).val())
                        .arg(elt.following.value_or(Id<Curve::SegmentModel>{-1}).val());
      qDebug() << log;
    }
    m_newCurveData = m_oldCurveData;
  };
  for(auto it = m_newCurveData.begin(); it != m_newCurveData.end(); ++it)
  {
    auto& sgt = *it;
    {
      if(std::isnan(sgt.start.x()))
      {
        on_invalid_curve();
        return;
      }
      if(std::isnan(sgt.start.y()))
      {
        on_invalid_curve();
        return;
      }
      if(std::isnan(sgt.end.x()))
      {
        on_invalid_curve();
        return;
      }
      if(std::isnan(sgt.end.y()))
      {
        on_invalid_curve();
        return;
      }
    }
  }

  {
    std::sort(m_newCurveData.begin(), m_newCurveData.end());
    if(!m_newCurveData.empty())
    {
      if(m_newCurveData.front().previous)
      {
        on_invalid_curve();
        return;
      }
      if(m_newCurveData.back().following)
      {
        on_invalid_curve();
        return;
      }
    }
  }
}

void UpdateCurve::undo(const score::DocumentContext& ctx) const
{
  auto& curve = m_model.find(ctx);
  curve.fromCurveData(m_oldCurveData);
}

void UpdateCurve::redo(const score::DocumentContext& ctx) const
{
  auto& curve = m_model.find(ctx);
  curve.fromCurveData(m_newCurveData);
}

void UpdateCurve::serializeImpl(DataStreamInput& s) const
{
  s << m_model << m_oldCurveData << m_newCurveData;
}

void UpdateCurve::deserializeImpl(DataStreamOutput& s)
{
  s >> m_model >> m_oldCurveData >> m_newCurveData;
}
}
