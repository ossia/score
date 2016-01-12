#pragma once

namespace Scenario
{
class EventPresenter;
class TimeNodePresenter;
class TemporalConstraintPresenter;
class StatePresenter;
}
namespace Loop
{
class LayerPresenter;
class ViewUpdater
{
    public:
        ViewUpdater(LayerPresenter& presenter);

        void updateEvent(const Scenario::EventPresenter &event);

        void updateConstraint(const Scenario::TemporalConstraintPresenter &pres);

        void updateTimeNode(const Scenario::TimeNodePresenter &timenode);

        void updateState(const Scenario::StatePresenter &state);

        LayerPresenter& m_presenter;
};

}
