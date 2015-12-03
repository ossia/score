#pragma once

#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/Event/EventView.hpp>
#include <Scenario/Document/TimeNode/TimeNodePresenter.hpp>
#include <Scenario/Document/TimeNode/TimeNodeView.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintView.hpp>
#include <iscore/tools/std/IndirectContainer.hpp>

template<
    typename Model_T,
    typename ConstraintPresenter_T>
class BaseScenarioPresenter
{
    public:
        BaseScenarioPresenter(const Model_T& model):
            m_model{model}
        {

        }

        virtual ~BaseScenarioPresenter() = default;

        IndirectContainer<std::vector, ConstraintPresenter_T> constraints() const
        { return {m_constraintPresenter}; }

        IndirectContainer<std::vector, StatePresenter> states() const
        { return {m_startStatePresenter, m_endStatePresenter}; }

        IndirectContainer<std::vector, EventPresenter> events() const
        { return {m_startEventPresenter, m_endEventPresenter}; }

        IndirectContainer<std::vector, TimeNodePresenter> timeNodes() const
        { return {m_startNodePresenter, m_endNodePresenter}; }

        const EventPresenter& event(const Id<EventModel>& id) const
        {
            if(id == m_model.startEvent().id())
                return *m_startEventPresenter;
            else if(id == m_model.endEvent().id())
                return *m_endEventPresenter;
            ISCORE_ABORT;
        }
        const TimeNodePresenter& timeNode(const Id<TimeNodeModel>& id) const
        {
            if(id == m_model.startTimeNode().id())
                return *m_startNodePresenter;
            else if(id == m_model.endTimeNode().id())
                return *m_endNodePresenter;
            ISCORE_ABORT;
        }
        const ConstraintPresenter_T& constraint(const Id<ConstraintModel>& id) const
        {
            if(id == m_model.constraint().id())
                return *m_constraintPresenter;
            ISCORE_ABORT;
        }
        const StatePresenter& state(const Id<StateModel>& id) const
        {
            if(id == m_model.startState().id())
                return *m_startStatePresenter;
            else if(id == m_model.endState().id())
                return *m_endStatePresenter;
            ISCORE_ABORT;
        }

        const TimeNodeModel& startTimeNode() const
        { return m_startNodePresenter->model(); }

        ConstraintPresenter_T* constraintPresenter() const
        { return m_constraintPresenter; }


    protected:
        const Model_T& m_model;

        ConstraintPresenter_T* m_constraintPresenter{};
        StatePresenter* m_startStatePresenter{};
        StatePresenter* m_endStatePresenter{};
        EventPresenter* m_startEventPresenter{};
        EventPresenter* m_endEventPresenter{};
        TimeNodePresenter* m_startNodePresenter{};
        TimeNodePresenter* m_endNodePresenter{};

};
