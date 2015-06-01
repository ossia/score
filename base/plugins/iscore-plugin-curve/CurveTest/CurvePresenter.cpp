#include "CurvePresenter.hpp"
#include <iscore/command/OngoingCommandManager.hpp>

#include <QFinalState>
#include <QAbstractTransition>

CurvePresenter::CurvePresenter(CurveModel* model, CurveView* view):
    m_model{model},
    m_view{view}
{

}

CurveModel*CurvePresenter::model() const
{ return m_model; }

QPointF CurvePresenter::pressedPoint() const
{ return m_pressedPoint; }

