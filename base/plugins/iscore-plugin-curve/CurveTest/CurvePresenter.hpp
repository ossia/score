#pragma once
#include <QStateMachine>
#include "CurveTest.hpp"

class CurveModel;
class CurveView;
class CurvePresenter
{
        QStateMachine curveSM;

        QPointF m_pressedPoint;
        CurveModel* m_model{};
        CurveView* m_view{};

    public:
        CurvePresenter(CurveModel*, CurveView*);

        CurveModel* model() const;

        // Taken from the view. First set this,
        // then send signals to the state machine.
        QPointF pressedPoint() const;
};
