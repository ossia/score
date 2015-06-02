#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/selection/Selectable.hpp>
#include <QPointF>


// Gives the data.
class CurveSegmentModel : public IdentifiedObject<CurveSegmentModel>
{
        Q_OBJECT
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
        virtual QVector<QPointF> data(int numInterp) const = 0; // Will interpolate


        QPointF start() const;
        void setStart(const QPointF& pt);

        QPointF end() const;
        void setEnd(const QPointF& pt);

        const id_type<CurveSegmentModel>& previous() const;
        void setPrevious(const id_type<CurveSegmentModel>& previous);

        const id_type<CurveSegmentModel>& following() const;
        void setFollowing(const id_type<CurveSegmentModel>& following);


    signals:
        void dataChanged();
        void previousChanged();
        void followingChanged();

    protected:
        virtual void on_startChanged() = 0;
        virtual void on_endChanged() = 0;

    private:
        QPointF m_start, m_end;
        id_type<CurveSegmentModel> m_previous, m_following;
};
