#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/selection/Selectable.hpp>
#include "StateMachine/CurvePoint.hpp"

// Gives the data.
class CurveSegmentModel : public IdentifiedObject<CurveSegmentModel>
{
        Q_OBJECT

        friend void Visitor<Writer<DataStream>>::writeTo<CurveSegmentModel>(CurveSegmentModel& ev);
        friend void Visitor<Writer<JSONObject>>::writeTo<CurveSegmentModel>(CurveSegmentModel& ev);
    public:
        Selectable selection;
        CurveSegmentModel(
                const id_type<CurveSegmentModel>& id,
                QObject* parent);

        template<typename Impl>
        CurveSegmentModel(Deserializer<Impl>& vis, QObject* parent) :
            IdentifiedObject<CurveSegmentModel>{vis, parent}
        {
            vis.writeTo(*this);
        }

        virtual CurveSegmentModel* clone(
                const id_type<CurveSegmentModel>& id,
                QObject* parent) const = 0;


        virtual QString name() const = 0;
        virtual void serialize(const VisitorVariant&) const = 0;
        virtual void updateData(int numInterp) = 0; // Will interpolate

        const QVector<QPointF>& data() const
        { return m_data; }


        void setStart(const CurvePoint& pt);
        CurvePoint start() const
        {
            return m_start;
        }

        void setEnd(const CurvePoint& pt);
        CurvePoint end() const
        {
            return m_end;
        }

        void setPrevious(const id_type<CurveSegmentModel>& previous);
        const id_type<CurveSegmentModel>& previous() const
        {
            return m_previous;
        }

        void setFollowing(const id_type<CurveSegmentModel>& following);
        const id_type<CurveSegmentModel>& following() const
        {
            return m_following;
        }


    signals:
        void dataChanged();
        void previousChanged();
        void followingChanged();

    protected:
        virtual void on_startChanged() = 0;
        virtual void on_endChanged() = 0;

        QVector<QPointF> m_data;
        bool m_valid{}; // Used to perform caching.

    private:
        CurvePoint m_start, m_end;
        id_type<CurveSegmentModel> m_previous, m_following;
};
