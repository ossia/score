#pragma once
#include <iscore/tools/NamedObject.hpp>
#include "ProcessInterface/ZoomHelper.hpp"

class ConstraintModel;
class FullViewConstraintPresenter;
class StatePresenter;
class EventPresenter;
class BaseElementPresenter;

// Contains the elements that are shown (not necessarily the ones in
// BaseScenarioModel)
class BaseScenarioPresenter : public NamedObject
{
    public:
        BaseScenarioPresenter(BaseElementPresenter* parent);

        void on_displayedConstraintChanged(const ConstraintModel *m);
        void showConstraint();

        void on_zoomRatioChanged(ZoomRatio r);

        auto constraintPresenter() const
        { return m_displayedConstraintPresenter; }
    private:
        BaseElementPresenter* m_parent{};

        FullViewConstraintPresenter* m_displayedConstraintPresenter{};
        StatePresenter* m_startStatePresenter{};
        StatePresenter* m_endStatePresenter{};
        EventPresenter* m_startEventPresenter{};
        EventPresenter* m_endEventPresenter{};

};
