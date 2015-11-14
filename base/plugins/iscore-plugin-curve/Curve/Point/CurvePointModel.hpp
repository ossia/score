#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/selection/Selectable.hpp>
#include "Curve/StateMachine/CurvePoint.hpp"

class CurveSegmentModel;

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
