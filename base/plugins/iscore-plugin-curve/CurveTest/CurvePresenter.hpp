#pragma once
#include <QStateMachine>
#include "CurveTest.hpp"
class CurvePresenter
{
        QStateMachine curveSM;

        QPointF m_pressedPoint;
        CurveModel* m_model{};
        CurveView* m_view{};

    public:
        CurvePresenter(CurveModel*, CurveView*);

        CurveModel* model() const
        { return m_model; }

        // Taken from the view. First set this,
        // then send signals to the state machine.
        QPointF pressedPoint() const
        { return m_pressedPoint; }
};
