#pragma once
#include <QVector>
#include <QPointF>
#include "Curve/StateMachine/CurveStateMachineBaseStates.hpp"
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include "Curve/Commands/UpdateCurve.hpp"
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
        void submit(const QVector<CurveSegmentModel*>);

    protected:
        virtual void on_press() = 0;

        QVector<QByteArray> m_oldCurveData;
        QPointF m_originalPress; // Note : there should be only one per curve...

        CurvePresenter* m_presenter{};

        Curve::StateBase* m_state{};


        SingleOngoingCommandDispatcher<UpdateCurve> m_dispatcher;
        QVector<QByteArray> m_startSegments;

        // To prevent behind locked at 0.000001 or 0.9999
        double m_xmin{-1}, m_xmax{2};
};
