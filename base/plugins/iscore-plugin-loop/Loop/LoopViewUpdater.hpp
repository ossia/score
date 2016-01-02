#pragma once

class EventPresenter;
class TimeNodePresenter;
class TemporalConstraintPresenter;
class StatePresenter;

namespace Loop
{
class LayerPresenter;
class ViewUpdater
{
    public:
        ViewUpdater(LayerPresenter& presenter);

        void updateEvent(const EventPresenter &event);

        void updateConstraint(const TemporalConstraintPresenter &pres);

        void updateTimeNode(const TimeNodePresenter &timenode);

        void updateState(const StatePresenter &state);

        LayerPresenter& m_presenter;
};

}
