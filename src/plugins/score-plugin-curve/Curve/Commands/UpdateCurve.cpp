// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "UpdateCurve.hpp"

#include <Curve/CurveModel.hpp>
#include <Curve/Segment/CurveSegmentData.hpp>

#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

#include <cmath>
namespace Curve
{
UpdateCurve::UpdateCurve(const Model& model, std::vector<SegmentData>&& segments)
    : m_model{std::move(model)}
    , m_oldCurveData{model.toCurveData()}
    , m_newCurveData{std::move(segments)}
{
  for(auto it = m_newCurveData.begin(); it != m_newCurveData.end();  ++it)
  {
    auto& sgt = *it;
    {
      if(std::isnan(sgt.start.x()))
        sgt.start.setX(0.);
      if(std::isnan(sgt.start.y()))
        sgt.start.setY(0.);
      if(std::isnan(sgt.end.x()))
        sgt.end.setX(1.);
      if(std::isnan(sgt.end.y()))
        sgt.end.setY(0.);
    }
  }

  {

    std::sort(m_newCurveData.begin(), m_newCurveData.end());

    /*
    qDebug() << "Printing map: ";
    for(auto elt : map)
    {
      QString log = QStringLiteral("id: %1 [%2; %3] prev: %4 following: %5")
                  .arg(elt.id.val())
                  .arg(elt.start.x())
                  .arg(elt.end.x())
                  .arg(elt.previous.value_or(Id<Curve::SegmentModel>{-1}).val())
                  .arg(elt.following.value_or(Id<Curve::SegmentModel>{-1}).val());
      qDebug() << log;
    }
    std::cout << std::endl;
    std::cerr << std::endl;
    */

    SCORE_ASSERT(m_newCurveData.empty() || (!m_newCurveData.front().previous && !m_newCurveData.back().following));
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
