#pragma once
#include <iscore/tools/std/Optional.hpp>
#include <iscore/selection/Selectable.hpp>
#include <iscore/tools/IdentifiedObject.hpp>

#include <Curve/Palette/CurvePoint.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_curve_export.h>

class QObject;
namespace Curve
{
class SegmentModel;
class ISCORE_PLUGIN_CURVE_EXPORT PointModel final : public IdentifiedObject<PointModel>
{
        Q_OBJECT
    public:
        Selectable selection;
        PointModel(const Id<PointModel>& id, QObject* parent);

        const Id<SegmentModel>& previous() const;
        void setPrevious(const Id<SegmentModel> &previous);

        const Id<SegmentModel> &following() const;
        void setFollowing(const Id<SegmentModel> &following);

        Curve::Point pos() const;
        void setPos(const Curve::Point &pos);

    signals:
        void posChanged();

    private:
        Id<SegmentModel> m_previous, m_following;

        Curve::Point m_pos;
};
}
