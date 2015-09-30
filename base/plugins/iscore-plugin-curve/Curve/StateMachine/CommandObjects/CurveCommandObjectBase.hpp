#pragma once
#include <QVector>
#include <QPointF>
#include "Curve/StateMachine/CurveStateMachineBaseStates.hpp"
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include "Curve/Commands/UpdateCurve.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>
class CurvePresenter;

/*
concept CommandObject
{
    public:
        void instantiate();
        void update();
        void commit();
        void rollback();
};
*/
// CreateSegment
// CreateSegmentBetweenPoints

// RemoveSegment -> easy peasy
// RemovePoint -> which segment do we merge ? At the left or at the right ?
// A point(view) has pointers to one or both of its curve segments.
class CurveSegmentModel;
class CurveCommandObjectBase
{
    public:
        CurveCommandObjectBase(CurvePresenter* pres, iscore::CommandStack&);

        void setCurveState(Curve::StateBase* stateBase) { m_state = stateBase; }
        void press();


        void handleLocking();

        // Get the current saved segments
        QVector<CurveSegmentModel*> deserializeSegments() const;

        // Creates and pushes an UpdateCurve command
        // from a vector of segments.
        // They are removed afterwards
        void submit(const QVector<CurveSegmentData>&);

    protected:
        auto find(
                QVector<CurveSegmentData>& segments,
                const Id<CurveSegmentModel>& id)
        {
            return std::find_if(
                        segments.begin(),
                        segments.end(),
                        [&] (const auto& seg) { return seg.id == id; });
        }
        auto find(
                const QVector<CurveSegmentData>& segments,
                const Id<CurveSegmentModel>& id)
        {
            return std::find_if(
                        segments.begin(),
                        segments.end(),
                        [&] (const auto& seg) { return seg.id == id; });
        }

        Id<CurveSegmentModel> getSegmentId(const QVector<CurveSegmentData>& ids)
        {
            Id<CurveSegmentModel> id {};

            do
            {
                id = Id<CurveSegmentModel>{getNextId()};
            }
            while(find(ids, id) != std::end(ids));

            return id;
        }

        virtual void on_press() = 0;

        QVector<QByteArray> m_oldCurveData;
        QPointF m_originalPress; // Note : there should be only one per curve...

        CurvePresenter* m_presenter{};

        Curve::StateBase* m_state{};


        SingleOngoingCommandDispatcher<UpdateCurveFast> m_dispatcher;
        QVector<CurveSegmentData> m_startSegments;

        // To prevent behind locked at 0.000001 or 0.9999
        double m_xmin{-1}, m_xmax{2};
};
