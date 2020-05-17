#pragma once
#include <Curve/Palette/CurvePoint.hpp>

#include <score/model/IdentifiedObject.hpp>
#include <score/model/Identifier.hpp>
#include <score/selection/Selectable.hpp>
#include <score/tools/std/Optional.hpp>

#include <score_plugin_curve_export.h>

#include <verdigris>

class QObject;
namespace Curve
{
class SegmentModel;
class SCORE_PLUGIN_CURVE_EXPORT PointModel final : public IdentifiedObject<PointModel>
{
  W_OBJECT(PointModel)
public:
  Selectable selection;
  PointModel(const Id<PointModel>& id, QObject* parent);

  const OptionalId<SegmentModel>& previous() const;
  void setPrevious(const OptionalId<SegmentModel>& previous);

  const OptionalId<SegmentModel>& following() const;
  void setFollowing(const OptionalId<SegmentModel>& following);

  Curve::Point pos() const;
  void setPos(const Curve::Point& pos);

public:
  void posChanged() E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, posChanged)

private:
  OptionalId<SegmentModel> m_previous, m_following;

  Curve::Point m_pos;
};
}

W_REGISTER_ARGTYPE(const Curve::PointModel&)
W_REGISTER_ARGTYPE(Curve::PointModel)
W_REGISTER_ARGTYPE(Id<Curve::PointModel>)
