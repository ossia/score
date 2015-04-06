#include "SelectStates.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"
#include "Process/Temporal/TemporalScenarioView.hpp"

#include "Document/Event/EventPresenter.hpp"
#include "Document/Event/EventView.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include <QGraphicsScene>
SelectAreaState::SelectAreaState(const TemporalScenarioPresenter& presenter,
                                 iscore::SelectionDispatcher& dispatcher):
    m_presenter{presenter},
    m_dispatcher{dispatcher}
{
    auto pressAreaSelection = new QState{this};
    auto moveAreaSelection = new QState{this};
    auto releaseAreaSelection = new QState{this};
}
