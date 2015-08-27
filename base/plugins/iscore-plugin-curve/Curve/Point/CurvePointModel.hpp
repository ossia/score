#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/selection/Selectable.hpp>
#include "Curve/StateMachine/CurvePoint.hpp"

class CurveSegmentModel;

class CurvePointModel : public IdentifiedObject<CurvePointModel>
{
    public:
        Selectable selection;
        CurvePointModel(const Id<CurvePointModel>& id, QObject* parent):
            IdentifiedObject<CurvePointModel>{id, "CurvePointModel", parent}
        {

        }

        const Id<CurveSegmentModel>& previous() const;
        void setPrevious(const Id<CurveSegmentModel> &previous);

        const Id<CurveSegmentModel> &following() const;
        void setFollowing(const Id<CurveSegmentModel> &following);

        CurvePoint pos() const;
        void setPos(const CurvePoint &pos);

    private:
        Id<CurveSegmentModel> m_previous, m_following;

        CurvePoint m_pos;
};
