#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <Process/ZoomHelper.hpp>
#include <Process/TimeValue.hpp>
#include <iscore/tools/std/StdlibWrapper.hpp>

#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.hpp>
class ConstraintModel;
class StateModel;
class StatePresenter;
class EventModel;
class EventPresenter;
class TimeNodeModel;
class TimeNodePresenter;
class LayerPresenter;
class BaseElementPresenter;
class BaseGraphicsObject;

// Contains the elements that are shown (not necessarily the ones in
// BaseScenarioModel)
class DisplayedElementsPresenter final : public QObject
{
        Q_OBJECT
    public:
        DisplayedElementsPresenter(BaseElementPresenter* parent);

        void on_displayedConstraintChanged(const ConstraintModel &m);
        void showConstraint();

        void on_zoomRatioChanged(ZoomRatio r);

        IndirectContainer<std::vector, FullViewConstraintPresenter> constraints() const
        {
            return {m_constraintPresenter};
        }
        IndirectContainer<std::vector, StatePresenter> states() const
        {
            return {m_startStatePresenter, m_endStatePresenter};
        }
        IndirectContainer<std::vector, EventPresenter> events() const
        {
            return {m_startEventPresenter, m_endEventPresenter};
        }
        IndirectContainer<std::vector, TimeNodePresenter> timeNodes() const
        {
            return {m_startNodePresenter, m_endNodePresenter};
        }

        const EventPresenter& event(const Id<EventModel>& id) const;
        const TimeNodePresenter& timeNode(const Id<TimeNodeModel>& id) const;
        const FullViewConstraintPresenter& constraint(const Id<ConstraintModel>& id) const;
        const StatePresenter& state(const Id<StateModel>& id) const;

        const TimeNodeModel& startTimeNode() const;

        FullViewConstraintPresenter* constraintPresenter() const
        { return m_constraintPresenter; }

        bool event(QEvent* e) override
        {
            return QObject::event(e);
        }

    signals:
        void requestFocusedPresenterChange(LayerPresenter*);

    public slots:
        void on_displayedConstraintDurationChanged(TimeValue);
        void on_displayedConstraintHeightChanged(double);
    private:
        void updateLength(double);
        BaseElementPresenter* m_parent{};

        FullViewConstraintPresenter* m_constraintPresenter{};
        StatePresenter* m_startStatePresenter{};
        StatePresenter* m_endStatePresenter{};
        EventPresenter* m_startEventPresenter{};
        EventPresenter* m_endEventPresenter{};
        TimeNodePresenter* m_startNodePresenter{};
        TimeNodePresenter* m_endNodePresenter{};

        std::vector<QMetaObject::Connection> m_connections;

};
