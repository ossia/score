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

        iscore::IndirectContainer<std::vector, ConstraintPresenter_T> getConstraints() const
        { return {m_constraintPresenter}; }

        iscore::IndirectContainer<std::vector, Scenario::StatePresenter> getStates() const
        { return {m_startStatePresenter, m_endStatePresenter}; }

        iscore::IndirectContainer<std::vector, Scenario::EventPresenter> getEvents() const
        { return {m_startEventPresenter, m_endEventPresenter}; }

        iscore::IndirectContainer<std::vector, Scenario::TimeNodePresenter> getTimeNodes() const
        { return {m_startNodePresenter, m_endNodePresenter}; }

        const Scenario::EventPresenter& event(const Id<Scenario::EventModel>& id) const
        {
            if(id == m_model.startEvent().id())
                return *m_startEventPresenter;
            else if(id == m_model.endEvent().id())
                return *m_endEventPresenter;
            ISCORE_ABORT;
        }
        const Scenario::TimeNodePresenter& timeNode(const Id<Scenario::TimeNodeModel>& id) const
        {
            if(id == m_model.startTimeNode().id())
                return *m_startNodePresenter;
            else if(id == m_model.endTimeNode().id())
                return *m_endNodePresenter;
            ISCORE_ABORT;
        }
        const ConstraintPresenter_T& constraint(const Id<Scenario::ConstraintModel>& id) const
        {
            if(id == m_model.constraint().id())
                return *m_constraintPresenter;
            ISCORE_ABORT;
        }
        const Scenario::StatePresenter& state(const Id<Scenario::StateModel>& id) const
        {
            if(id == m_model.startState().id())
                return *m_startStatePresenter;
            else if(id == m_model.endState().id())
                return *m_endStatePresenter;
            ISCORE_ABORT;
        }

        const Scenario::TimeNodeModel& startTimeNode() const
        { return m_startNodePresenter->model(); }

        ConstraintPresenter_T* constraintPresenter() const
        { return m_constraintPresenter; }


    protected:
        const Model_T& m_model;

        ConstraintPresenter_T* m_constraintPresenter{};
        Scenario::StatePresenter* m_startStatePresenter{};
        Scenario::StatePresenter* m_endStatePresenter{};
        Scenario::EventPresenter* m_startEventPresenter{};
        Scenario::EventPresenter* m_endEventPresenter{};
        Scenario::TimeNodePresenter* m_startNodePresenter{};
        Scenario::TimeNodePresenter* m_endNodePresenter{};

};
