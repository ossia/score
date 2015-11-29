#pragma once
#include <boost/optional/optional.hpp>
#include <iscore/selection/Selectable.hpp>
#include <iscore/tools/IdentifiedObject.hpp>

#include <Curve/Palette/CurvePoint.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class CurveSegmentModel;
class QObject;

class CurvePointModel final : public IdentifiedObject<CurvePointModel>
{
        Q_OBJECT
    public:
        Selectable selection;
        CurvePointModel(const Id<CurvePointModel>& id, QObject* parent);

        const Id<CurveSegmentModel>& previous() const;
        void setPrevious(const Id<CurveSegmentModel> &previous);

        const Id<CurveSegmentModel> &following() const;
        void setFollowing(const Id<CurveSegmentModel> &following);

        Curve::Point pos() const;
        void setPos(const Curve::Point &pos);

    signals:
        void posChanged();

    private:
        Id<CurveSegmentModel> m_previous, m_following;

        Curve::Point m_pos;
};
