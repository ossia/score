#pragma once
#include <iscore/tools/NamedObject.hpp>
#include "ProcessInterface/ZoomHelper.hpp"
#include "ProcessInterface/TimeValue.hpp"

class ConstraintModel;
class FullViewConstraintPresenter;
class StatePresenter;
class EventPresenter;
class BaseElementPresenter;

// Contains the elements that are shown (not necessarily the ones in
// BaseScenarioModel)
class DisplayedElementsPresenter : public QObject
{
    public:
        DisplayedElementsPresenter(BaseElementPresenter* parent);

        void on_displayedConstraintChanged(const ConstraintModel &m);
        void showConstraint();

        void on_zoomRatioChanged(ZoomRatio r);

        FullViewConstraintPresenter* constraintPresenter() const
        { return m_constraintPresenter; }

    public slots:
        void on_displayedConstraintDurationChanged(TimeValue);
    private:
        BaseElementPresenter* m_parent{};

        FullViewConstraintPresenter* m_constraintPresenter{};
        StatePresenter* m_startStatePresenter{};
        StatePresenter* m_endStatePresenter{};
        EventPresenter* m_startEventPresenter{};
        EventPresenter* m_endEventPresenter{};

};
