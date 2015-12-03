#pragma once

class LoopPresenter;
class EventPresenter;
class TimeNodePresenter;
class TemporalConstraintPresenter;
class StatePresenter;

class LoopViewUpdater
{
    public:
        LoopViewUpdater(LoopPresenter& presenter);

        void updateEvent(const EventPresenter &event);

        void updateConstraint(const TemporalConstraintPresenter &pres);

        void updateTimeNode(const TimeNodePresenter &timenode);

        void updateState(const StatePresenter &state);

        LoopPresenter& m_presenter;
};

