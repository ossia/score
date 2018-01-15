#pragma once
#include <score/selection/Selectable.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/tools/std/Optional.hpp>

#include <Curve/Palette/CurvePoint.hpp>
#include <score/model/Identifier.hpp>
#include <score_plugin_curve_export.h>

class QObject;
namespace Curve
{
class SegmentModel;
class SCORE_PLUGIN_CURVE_EXPORT PointModel final
    : public IdentifiedObject<PointModel>
{
  Q_OBJECT
public:
  Selectable selection;
  PointModel(const Id<PointModel>& id, QObject* parent);

  const OptionalId<SegmentModel>& previous() const;
  void setPrevious(const OptionalId<SegmentModel>& previous);

  const OptionalId<SegmentModel>& following() const;
  void setFollowing(const OptionalId<SegmentModel>& following);

  Curve::Point pos() const;
  void setPos(const Curve::Point& pos);

Q_SIGNALS:
  void posChanged();

private:
  OptionalId<SegmentModel> m_previous, m_following;

  Curve::Point m_pos;
};
}
