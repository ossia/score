#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/selection/Selectable.hpp>
#include "Curve/StateMachine/CurvePoint.hpp"

class CurveSegmentModel;

class CurvePointModel : public IdentifiedObject<CurvePointModel>
{
    public:
        Selectable selection;
        CurvePointModel(const id_type<CurvePointModel>& id, QObject* parent):
            IdentifiedObject<CurvePointModel>{id, "CurvePointModel", parent}
        {

        }

        const id_type<CurveSegmentModel>& previous() const;
        void setPrevious(const id_type<CurveSegmentModel> &previous);

        const id_type<CurveSegmentModel> &following() const;
        void setFollowing(const id_type<CurveSegmentModel> &following);

        CurvePoint pos() const;
        void setPos(const CurvePoint &pos);

    private:
        id_type<CurveSegmentModel> m_previous, m_following;

        CurvePoint m_pos;
};
