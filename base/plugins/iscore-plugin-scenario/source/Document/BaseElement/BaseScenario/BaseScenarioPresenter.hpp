#pragma once
#include <iscore/tools/NamedObject.hpp>

class ConstraintModel;
class FullViewConstraintPresenter;
class StatePresenter;
class EventPresenter;

// Contains the elements that are shown (not necessarily the ones in
// BaseScenarioModel)
class BaseScenarioPresenter : public NamedObject
{
    public:
        void on_displayedConstraintChanged(ConstraintModel *m);

        void showConstraint();

    private:
        FullViewConstraintPresenter* m_displayedConstraintPresenter{};
        StatePresenter* m_startStatePresenter{};
        StatePresenter* m_endStatePresenter{};
        EventPresenter* m_startEventPresenter{};
        EventPresenter* m_endEventPresenter{};

};
