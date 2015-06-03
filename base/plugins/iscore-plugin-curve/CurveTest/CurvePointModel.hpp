#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/selection/Selectable.hpp>
#include "CurveTest/StateMachine/CurvePoint.hpp"

class CurveSegmentModel;

class CurvePointModel : public QObject
{
    public:
        Selectable selection;
        CurvePointModel(QObject* parent):
            QObject{parent}
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
